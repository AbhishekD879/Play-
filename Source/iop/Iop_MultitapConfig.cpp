//>>> PLAYSTATION-PORTFOLIO MULTITAP
#include "Iop_MultitapConfig.h"
#include <array>

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
