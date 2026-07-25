//>>> PLAYSTATION-PORTFOLIO MULTITAP
#include "Iop_MultitapConfig.h"
#include <array>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace Iop
{
	namespace Multitap
	{
		namespace
		{
			//Process-wide rather than per-VM: the web build runs exactly one VM,
			//and this has to be readable from both CSio2 and CPadMan without
			//threading a config object through two unrelated call chains (which
			//would mean editing far more upstream files than it is worth).
			std::array<bool, MAX_PORTS> g_enabled = {false, false};
			bool g_tracing = false;
			bool g_slotSwitching = false;
			//Diagnostics are bounded: a runaway trace must not be able to wedge
			//the emulator, which is exactly what an unbounded one did.
			constexpr unsigned int TRACE_BUDGET = 3000;
			unsigned int g_traceCount = 0;
			char g_lastTrace[512] = {0};
		}

		bool SlotSwitchingEnabled() { return g_slotSwitching; }
		void SetSlotSwitching(bool on) { g_slotSwitching = on; }

		bool IsTracing() { return g_tracing; }
		void SetTracing(bool on) { g_tracing = on; g_traceCount = 0; g_lastTrace[0] = 0; }

		//★ This runs inside SIO2 command processing, which a game drives HUNDREDS
		//of times a second. An unthrottled printf+fflush per call crosses the
		//wasm->JS boundary every time and drags the emulator to a standstill —
		//it looks exactly like "the controller stopped working". So: no fflush
		//(stdout flushes on newline anyway), skip repeats, and stop dead after a
		//fixed budget. Diagnostics must never be able to break the emulator.
		void Trace(const char* fmt, ...)
		{
			if(!g_tracing) return;
			if(g_traceCount > TRACE_BUDGET) return;

			char buf[512];
			va_list args;
			va_start(args, fmt);
			vsnprintf(buf, sizeof(buf), fmt, args);
			va_end(args);

			//Collapse repeats: a poll loop otherwise emits the same line forever.
			if(!strcmp(buf, g_lastTrace)) return;
			snprintf(g_lastTrace, sizeof(g_lastTrace), "%s", buf);

			if(g_traceCount == TRACE_BUDGET)
			{
				printf("[mtap] trace budget reached - further lines suppressed\n");
				g_traceCount++;
				return;
			}
			g_traceCount++;
			printf("[mtap] %s\n", buf);
		}

		bool IsEnabled(unsigned int port)
		{
			if(port >= MAX_PORTS) return false;
			return g_enabled[port];
		}

		void SetEnabled(unsigned int port, bool enabled)
		{
			if(port >= MAX_PORTS) return;
			g_enabled[port] = enabled;
		}

		bool AnyEnabled()
		{
			for(auto e : g_enabled)
			{
				if(e) return true;
			}
			return false;
		}

		void Reset()
		{
			g_enabled = {false, false};
		}
	}
}
//<<< PLAYSTATION-PORTFOLIO MULTITAP
