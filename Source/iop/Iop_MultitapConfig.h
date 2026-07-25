#pragma once

//>>> PLAYSTATION-PORTFOLIO MULTITAP
// Shared multitap state for the two independent pad paths:
//   · CSio2   — low-level SIO2 device emulation (games talking to the hardware)
//   · CPadMan — HLE of rom0:PADMAN / XPADMAN (games using the standard SDK)
// Both must agree on which ports have a tap and how a (port, slot) pair maps to
// the flat pad index that CPH_GenericInput / CInputBindingManager use.
//
// This lives in a NEW file on purpose: it can never conflict on an upstream
// rebase. See docs/ps2-multitap/PATCHES.md in the portfolio repo.

#include "Types.h"

namespace Iop
{
	namespace Multitap
	{
		enum
		{
			//The PS2 has exactly two PHYSICAL controller ports. Extra players
			//come from a multitap multiplexing up to four pads into one port —
			//never from a third port. Do not "fix" this to 6.
			MAX_PORTS = 2,
			MAX_SLOTS = 4,
			MAX_PADS = MAX_PORTS * MAX_SLOTS, //8
		};

		//—— tracing ————————————————————————————————————————————————————
		//Play!'s own CLog compiles to a NO-OP in release builds
		//(LOGGING_ENABLED == 0), so none of its Print/Warn calls exist in the
		//wasm we ship. This is a separate, always-compiled trace that emscripten
		//routes to console.log, gated at runtime so it costs nothing when off.
		//It is the only way to see what a real game actually asks for.
		bool IsTracing();
		void SetTracing(bool);
		void Trace(const char* fmt, ...);

		//Per-port enable. Set before a disc boots; not expected to change while
		//a game is running (a game latches slot counts at init).
		bool IsEnabled(unsigned int port);
		void SetEnabled(unsigned int port, bool enabled);
		bool AnyEnabled();
		void Reset();

		//(port, slot) -> the flat pad index the binding manager uses.
		//
		//With NO tap anywhere we keep the stock mapping (port 0 -> pad 0,
		//port 1 -> pad 1) so a build with multitap switched off is behaviourally
		//identical to upstream. As soon as any tap exists we switch to the
		//uniform layout, because the legacy mapping would collide: port 1 slot 0
		//and port 0 slot 1 would both want pad 1.
		inline unsigned int PadIndex(unsigned int port, unsigned int slot)
		{
			if(!AnyEnabled()) return port;
			return (port * MAX_SLOTS) + slot;
		}
	}
}
//<<< PLAYSTATION-PORTFOLIO MULTITAP
