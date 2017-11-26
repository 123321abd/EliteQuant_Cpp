#ifndef _EliteQuant_Common_Calc_H
#define _EliteQuant_Common_Calc_H

#include <Common/config.h>
#include <Common/Security/position.h>
#include <Common/Order/fill.h>

namespace EliteQuant {
	namespace Calc {
		// Gets the open PL on a per-share basis, ignoring the size of the position.
		double OpenPT(double LastTrade, double AvgPrice, bool Side);
		double OpenPT(double LastTrade, double AvgPrice, int PosSize);
		// Gets the open PL considering all the shares held in a position.
		double OpenPL(double LastTrade, double AvgPrice, int PosSizeMultiplier);

		// Gets the closed PL on a per-share basis, ignoring how many shares are held.
		double ClosePT(Position& existing, Fill& adjust);
		// Gets the closed PL on a position basis, the PL that is registered to the account for the entire shares transacted.
		double ClosePL(Position& existing, Fill& adjust, int multiplier = 1);
	}
}

#endif
