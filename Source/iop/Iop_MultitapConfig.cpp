//>>> PLAYSTATION-PORTFOLIO MULTITAP
#include "Iop_MultitapConfig.h"
#include <array>
#include <cstdio>
#include <cstdarg>

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
		}

		bool IsTracing() { return g_tracing; }
		void SetTracing(bool on) { g_tracing = on; }

		void Trace(const char* fmt, ...)
		{
			if(!g_tracing) return;
			char buf[512];
			va_list args;
			va_start(args, fmt);
			vsnprintf(buf, sizeof(buf), fmt, args);
			va_end(args);
			//emscripten routes stdout to console.log
			printf("[mtap] %s\n", buf);
			fflush(stdout);
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
