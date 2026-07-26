#include "Ps2VmJs.h"
#include "Jitter_CodeGen_Wasm.h"
#include "MemoryUtils.h"
#include "BasicBlock.h"
#include "PS2VM_Preferences.h"
#include "AppConfig.h"

extern "C" uint32 LWL_Proxy(uint32, uint32, CMIPS*);
extern "C" uint32 LWR_Proxy(uint32, uint32, CMIPS*);
extern "C" uint64 LDL_Proxy(uint32, uint64, CMIPS*);
extern "C" uint64 LDR_Proxy(uint32, uint64, CMIPS*);
extern "C" void SWL_Proxy(uint32, uint32, CMIPS*);
extern "C" void SWR_Proxy(uint32, uint32, CMIPS*);
extern "C" void SDL_Proxy(uint32, uint64, CMIPS*);
extern "C" void SDR_Proxy(uint32, uint64, CMIPS*);

//>>> PLAYSTATION-PORTFOLIO MISSING CALLABLES
// The recompiler calls these five, and none of them were registered. In a
// release build FindFunction's miss is a null dereference that reads zeros, so
// the codegen emitted call_indirect with signature index 0 and table index 0 —
// a module the browser refuses, killing the VM mid-boot. Shadow of the Colossus
// dies on TestVectorNaN, which is VU code, which is why a game like SmackDown
// never trips it.
//
// TestVectorNaN and FpAddTruncate have C++ linkage, and RegisterExternFunction
// resolves names through Module["_name"], so mangled symbols are unusable.
// These shims give them a C name to export while the registry stays keyed on
// the address the recompiler actually passes to Call().
#include "ee/FpAddTruncate.h"
#include "COP_SCU.h"
#include "ee/PS2OS.h"

void TestVectorNaN(CMIPS*, uint32, uint32);
extern "C" void MIPS_HandleTLBException(CMIPS*);

// The TLB set. PS2OS::UpdateTLBEnabledState swaps these in only once a game
// installs a TLB exception handler, so a game that never touches the TLB —
// SmackDown, NFS Underground 2 — runs fine with them missing, and one that does
// dies on its first TLB-guarded memory access. That is the whole reason this
// looked like a per-game bug rather than a missing registration.
extern "C" uint32 Portfolio_TranslateAddress(CMIPS* context, uint32 vaddrLo)
{
	return CPS2OS::TranslateAddress(context, vaddrLo);
}
extern "C" uint32 Portfolio_TranslateAddressTLB(CMIPS* context, uint32 vaddrLo)
{
	return CPS2OS::TranslateAddressTLB(context, vaddrLo);
}
extern "C" uint32 Portfolio_CheckTLBExceptions(CMIPS* context, uint32 vaddrLo, uint32 isWrite)
{
	return CPS2OS::CheckTLBExceptions(context, vaddrLo, isWrite);
}

extern "C" void Portfolio_TestVectorNaN(CMIPS* context, uint32 dest, uint32 offset)
{
	TestVectorNaN(context, dest, offset);
}
extern "C" uint32 Portfolio_FpAddTruncate(uint32 a, uint32 b)
{
	return FpAddTruncate(a, b);
}
extern "C" void Portfolio_HandleTLBRead(CMIPS* context)
{
	CCOP_SCU::HandleTLBRead(context);
}
extern "C" void Portfolio_HandleTLBWrite(CMIPS* context)
{
	CCOP_SCU::HandleTLBWrite(context);
}
extern "C" void TrapHandler(CMIPS*);
//<<< PLAYSTATION-PORTFOLIO MISSING CALLABLES

void CPs2VmJs::CreateVM()
{
	printf("Initializing PS2VM...\r\n");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&EmptyBlockHandler), "_EmptyBlockHandler", "vi");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_GetByteProxy), "_MemoryUtils_GetByteProxy", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_GetHalfProxy), "_MemoryUtils_GetHalfProxy", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_GetWordProxy), "_MemoryUtils_GetWordProxy", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_GetDoubleProxy), "_MemoryUtils_GetDoubleProxy", "jii");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_SetByteProxy), "_MemoryUtils_SetByteProxy", "viii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_SetHalfProxy), "_MemoryUtils_SetHalfProxy", "viii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_SetWordProxy), "_MemoryUtils_SetWordProxy", "viii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MemoryUtils_SetDoubleProxy), "_MemoryUtils_SetDoubleProxy", "viji");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&LWL_Proxy), "_LWL_Proxy", "iiii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&LWR_Proxy), "_LWR_Proxy", "iiii");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&LDL_Proxy), "_LDL_Proxy", "jiji");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&LDR_Proxy), "_LDR_Proxy", "jiji");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&SWL_Proxy), "_SWL_Proxy", "viii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&SWR_Proxy), "_SWR_Proxy", "viii");

	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&SDL_Proxy), "_SDL_Proxy", "viji");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&SDR_Proxy), "_SDR_Proxy", "viji");

	//>>> PLAYSTATION-PORTFOLIO MISSING CALLABLES
	// Keyed on the real function's address — that is what Call() hands the
	// registry — while the table entry points at the exported C shim.
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&TestVectorNaN), "_Portfolio_TestVectorNaN", "viii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&FpAddTruncate), "_Portfolio_FpAddTruncate", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&CCOP_SCU::HandleTLBRead), "_Portfolio_HandleTLBRead", "vi");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&CCOP_SCU::HandleTLBWrite), "_Portfolio_HandleTLBWrite", "vi");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&TrapHandler), "_TrapHandler", "vi");

	// Reached via m_pAddrTranslator / m_TLBExceptionChecker, which are function
	// pointers held in the CPU context — so no Call site names them literally
	// and grepping for Call(&fn) will never turn them up.
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&CPS2OS::TranslateAddress), "_Portfolio_TranslateAddress", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&CPS2OS::TranslateAddressTLB), "_Portfolio_TranslateAddressTLB", "iii");
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&CPS2OS::CheckTLBExceptions), "_Portfolio_CheckTLBExceptions", "iiii");
	// JumpTo target, resolved through the same registry as Call.
	Jitter::CWasmFunctionRegistry::RegisterFunction(reinterpret_cast<uintptr_t>(&MIPS_HandleTLBException), "_MIPS_HandleTLBException", "vi");
	//<<< PLAYSTATION-PORTFOLIO MISSING CALLABLES

	CPS2VM::CreateVM();
}

void CPs2VmJs::BootElf(std::string path)
{
	m_mailBox.SendCall([this, path]() {
		printf("Loading '%s'...\r\n", path.c_str());
		try
		{
			Reset();
			m_ee->m_os->BootFromFile(path);
		}
		catch(const std::exception& ex)
		{
			printf("Failed to start: %s.\r\n", ex.what());
			return;
		}
		printf("Starting...\r\n");
		ResumeImpl();
	});
}

void CPs2VmJs::BootDiscImage(std::string path)
{
	m_mailBox.SendCall([this, path]() {
		printf("Loading '%s'...\r\n", path.c_str());
		try
		{
			CAppConfig::GetInstance().SetPreferencePath(PREF_PS2_CDROM0_PATH, path);
			Reset();
			m_ee->m_os->BootFromCDROM();
		}
		catch(const std::exception& ex)
		{
			printf("Failed to start: %s.\r\n", ex.what());
			return;
		}
		printf("Starting...\r\n");
		ResumeImpl();
	});
}