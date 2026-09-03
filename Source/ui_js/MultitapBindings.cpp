//>>> PLAYSTATION-PORTFOLIO MULTITAP
// JS entry point for multitap control.
//
// Deliberately a NEW file with its OWN EMSCRIPTEN_BINDINGS block rather than an
// edit to Main.cpp's block: embind allows any number of binding blocks, so this
// costs zero upstream conflict surface on a rebase.
//
// From JavaScript:
//   Module.setMultitapEnabled(0, true);   // tap on controller port 1
//   Module.setMultitapEnabled(1, true);   // tap on controller port 2  -> 8 pads
//   Module.getMultitapEnabled(0);
//   Module.getMultitapPadCount();         // players currently addressable
//
// Must be called BEFORE booting a disc: games latch slot counts during init.

#include <emscripten/bind.h>
#include "AppConfig.h"
#include "Ps2VmJs.h"
#include "PS2VM_Preferences.h"
#include "GSH_OpenGLJs.h" // PREF_CGSH_OPENGL_RESOLUTION_FACTOR

// counters live in the CodeGen library's wasm memory-function path
namespace PortfolioFrameStats { extern uint64_t flips; extern uint64_t vblanks; }
namespace PortfolioEeScale { extern uint32_t num, den; }

// defined in Main.cpp — must be declared at file scope, outside the anonymous
// namespace, or the extern acquires internal linkage and fails to link
extern CPs2VmJs* g_virtualMachine;
#ifdef PROFILE
namespace PortfolioProf { extern double ee, iop, spu, gssync, other; }
#endif
namespace PortfolioLastModule { extern uint8_t bytes[]; extern uint32_t size; extern uint32_t count; }
namespace PortfolioMissingFct { extern uint32_t ptr; extern uint32_t count; }

namespace PortfolioJitStats
{
	extern uint64_t blocksCompiled;
	extern uint64_t blocksLive;
	extern uint64_t codeBytes;
	extern double compileMs;
}
#include "../iop/Iop_MultitapConfig.h"
#include "../input/PH_GenericInput.h"
#include "Ps2VmJs.h"

extern CPs2VmJs* g_virtualMachine;

namespace
{
	void SetMultitapEnabled(unsigned int port, bool enabled)
	{
		Iop::Multitap::SetEnabled(port, enabled);
	}

	bool GetMultitapEnabled(unsigned int port)
	{
		return Iop::Multitap::IsEnabled(port);
	}

	//How many distinct pads the current configuration can address:
	//2 with no tap, 5 with one tap (4 + the other port), 8 with two.
	unsigned int GetMultitapPadCount()
	{
		if(!Iop::Multitap::AnyEnabled()) return Iop::Multitap::MAX_PORTS;
		unsigned int count = 0;
		for(unsigned int port = 0; port < Iop::Multitap::MAX_PORTS; port++)
		{
			count += Iop::Multitap::IsEnabled(port) ? Iop::Multitap::MAX_SLOTS : 1;
		}
		return count;
	}

	void SetMultitapTracing(bool on) { Iop::Multitap::SetTracing(on); }

	//—— test hook ————————————————————————————————————————————————————————
	// Reads a pad binding's LIVE value. GetBindingValue resolves straight from
	// the input provider, so this needs no disc and no running game — which is
	// the point: 'is pad N actually bound and receiving keys?' was being
	// answered by asking the user to boot a real game and report back, over and
	// over. It is answerable here in milliseconds.
	//
	//   Module.getPadButton(0, 14)  -> CROSS on pad 1
	int GetPadButton(unsigned int pad, unsigned int button)
	{
		if(!g_virtualMachine) return -1;
		auto padHandler = static_cast<CPH_GenericInput*>(g_virtualMachine->GetPadHandler());
		if(!padHandler) return -1;
		if(button >= PS2::CControllerInfo::MAX_BUTTONS) return -1;
		return static_cast<int>(padHandler->GetBindingManager().GetBindingValue(
		    pad, static_cast<PS2::CControllerInfo::BUTTON>(button)));
	}
	void SetMultitapSlotSwitching(bool on) { Iop::Multitap::SetSlotSwitching(on); }

	//—— JIT stats ————————————————————————————————————————————————————————
	// "Some games are slow" and "some games die" are the same question asked
	// twice, and neither is answerable from outside the emulator. Every
	// recompiled block becomes a WebAssembly module the browser compiles
	// synchronously; these four numbers say how many, how big and how long.
	//   Module.getJitBlocksCompiled()  cumulative blocks handed to the browser
	//   Module.getJitBlocksLive()      still resident — this one is unbounded
	//   Module.getJitCodeBytes()       total generated wasm
	//   Module.getJitCompileMs()       wall time lost to compiling it
	double GetJitBlocksCompiled() { return static_cast<double>(PortfolioJitStats::blocksCompiled); }
	double GetJitBlocksLive()     { return static_cast<double>(PortfolioJitStats::blocksLive); }
	double GetJitCodeBytes()      { return static_cast<double>(PortfolioJitStats::codeBytes); }
	double GetJitCompileMs()      { return PortfolioJitStats::compileMs; }
	/** GS presents so far. Sampled over time this is emulation speed: a PS2
	 *  game presents ~60/s (NTSC), so 30/s means it is running at half pace. */
	double GetFrameCount()        { return static_cast<double>(PortfolioFrameStats::flips); }
	double GetVblankCount()       { return static_cast<double>(PortfolioFrameStats::vblanks); }

	// Benchmarking. The limiter paces the VM to the PS2's field rate, which caps
	// a fast game at ~60 and hides any throughput change above that. Turning it
	// off makes "wall time to reach N emulated frames" a pure measure of how fast
	// the recompiled code runs. Read at boot, so set it BEFORE the disc spins.

	// The PCSX2-style "EE cyclerate" underclock, using the scaling upstream
	// already ships for its arcade drivers (SetEeFrequencyScale plumbs the
	// ratio into the hblank/vblank/SPU/IOP tick totals). Halving the EE clock
	// halves the instructions the recompiler must execute per emulated frame,
	// so a host that only manages 40% real-time speed can reach 80-100% — at
	// the cost of in-game framerate, exactly like a struggling real PS2.
	// Call BEFORE the disc boots.
	void SetEeFreqScale(uint32 numerator, uint32 denominator)
	{
		if(numerator == 0 || denominator == 0) return;
		// stored first: ResetVM() re-applies this during the boot that follows
		PortfolioEeScale::num = numerator;
		PortfolioEeScale::den = denominator;
		if(g_virtualMachine) g_virtualMachine->SetEeFrequencyScale(numerator, denominator);
	}

	void SetFrameLimit(bool on)
	{
		CAppConfig::GetInstance().SetPreferenceBoolean(PREF_PS2_LIMIT_FRAMERATE, on);
	}

	// Internal render resolution: the GS draws every framebuffer at N× the PS2's
	// native size, which is what turns a 512×448 game into a clean 1024×896 or
	// 1536×1344 picture — the same lever PCSX2 and Play!'s own Qt UI expose. The
	// preference already exists upstream (renderer.opengl.resfactor); this only
	// makes it reachable from JavaScript.
	//
	// Applies LIVE: NotifyPreferencesChanged is marshalled onto the GS thread by
	// SendGSCall, where the OpenGL handler reloads the pref and drops its cached
	// framebuffers/depthbuffers so they are recreated at the new scale. Also safe
	// before boot — the value is simply read at GS initialisation.
	void SetResolutionFactor(uint32 factor)
	{
		if(factor < 1 || factor > 4) return; // 4× of 640×448 is already 2560×1792 — anything past that is a hang on a phone
		CAppConfig::GetInstance().SetPreferenceInteger(PREF_CGSH_OPENGL_RESOLUTION_FACTOR, factor);
		if(g_virtualMachine)
		{
			if(auto gs = g_virtualMachine->GetGSHandler())
			{
				gs->NotifyPreferencesChanged();
			}
		}
	}

	uint32 GetResolutionFactor()
	{
		return static_cast<uint32>(CAppConfig::GetInstance().GetPreferenceInteger(PREF_CGSH_OPENGL_RESOLUTION_FACTOR));
	}
#ifdef PROFILE
	double GetProfEe()     { return PortfolioProf::ee; }
	double GetProfIop()    { return PortfolioProf::iop; }
	double GetProfSpu()    { return PortfolioProf::spu; }
	double GetProfGsSync() { return PortfolioProf::gssync; }
	double GetProfOther()  { return PortfolioProf::other; }
#else
	double GetProfEe()     { return -1.0; }
	double GetProfIop()    { return -1.0; }
	double GetProfSpu()    { return -1.0; }
	double GetProfGsSync() { return -1.0; }
	double GetProfOther()  { return -1.0; }
#endif
	/** The most recently generated wasm module — read HEAPU8 at this pointer for
	 *  this many bytes. After a CompileError this IS the module the browser
	 *  refused, which is the only way to disassemble what the codegen got wrong. */
	unsigned GetLastModulePtr()   { return reinterpret_cast<unsigned>(PortfolioLastModule::bytes); }
	unsigned GetLastModuleSize()  { return PortfolioLastModule::size; }
	unsigned GetLastModuleCount() { return PortfolioLastModule::count; }
	/** Function pointer the recompiler tried to call but that was never
	 *  registered in the Wasm function registry. Resolve it from JS with
	 *  Module.wasmTable.get(ptr) to name the culprit. */
	unsigned GetMissingFctPtr()   { return PortfolioMissingFct::ptr; }
	unsigned GetMissingFctCount() { return PortfolioMissingFct::count; }
}

EMSCRIPTEN_BINDINGS(PortfolioMultitap)
{
	emscripten::function("getFrameCount", &GetFrameCount);
	emscripten::function("getVblankCount", &GetVblankCount);
	emscripten::function("setEeFreqScale", &SetEeFreqScale);
	emscripten::function("setFrameLimit", &SetFrameLimit);
	emscripten::function("setResolutionFactor", &SetResolutionFactor);
	emscripten::function("getResolutionFactor", &GetResolutionFactor);
	emscripten::function("getProfEe", &GetProfEe);
	emscripten::function("getProfIop", &GetProfIop);
	emscripten::function("getProfSpu", &GetProfSpu);
	emscripten::function("getProfGsSync", &GetProfGsSync);
	emscripten::function("getProfOther", &GetProfOther);
	emscripten::function("getLastModulePtr", &GetLastModulePtr);
	emscripten::function("getLastModuleSize", &GetLastModuleSize);
	emscripten::function("getLastModuleCount", &GetLastModuleCount);
	emscripten::function("getMissingFctPtr", &GetMissingFctPtr);
	emscripten::function("getMissingFctCount", &GetMissingFctCount);
	emscripten::function("getJitBlocksCompiled", &GetJitBlocksCompiled);
	emscripten::function("getJitBlocksLive", &GetJitBlocksLive);
	emscripten::function("getJitCodeBytes", &GetJitCodeBytes);
	emscripten::function("getJitCompileMs", &GetJitCompileMs);
	emscripten::function("setMultitapTracing", &SetMultitapTracing);
	emscripten::function("getPadButton", &GetPadButton);
	emscripten::function("setMultitapSlotSwitching", &SetMultitapSlotSwitching);
	emscripten::function("setMultitapEnabled", &SetMultitapEnabled);
	emscripten::function("getMultitapEnabled", &GetMultitapEnabled);
	emscripten::function("getMultitapPadCount", &GetMultitapPadCount);
}
//<<< PLAYSTATION-PORTFOLIO MULTITAP
