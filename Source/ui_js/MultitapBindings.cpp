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
#include "../iop/Iop_MultitapConfig.h"

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
	void SetMultitapSlotSwitching(bool on) { Iop::Multitap::SetSlotSwitching(on); }
}

EMSCRIPTEN_BINDINGS(PortfolioMultitap)
{
	emscripten::function("setMultitapTracing", &SetMultitapTracing);
	emscripten::function("setMultitapSlotSwitching", &SetMultitapSlotSwitching);
	emscripten::function("setMultitapEnabled", &SetMultitapEnabled);
	emscripten::function("getMultitapEnabled", &GetMultitapEnabled);
	emscripten::function("getMultitapPadCount", &GetMultitapPadCount);
}
//<<< PLAYSTATION-PORTFOLIO MULTITAP
