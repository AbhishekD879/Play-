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

// counters live in the CodeGen library's wasm memory-function path
namespace PortfolioFrameStats { extern uint64_t flips; }

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
}

EMSCRIPTEN_BINDINGS(PortfolioMultitap)
{
	emscripten::function("getFrameCount", &GetFrameCount);
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
