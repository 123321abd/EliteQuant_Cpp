#ifndef _EliteQuant_Common_Position_H_
#define _EliteQuant_Common_Position_H_

#include <string>
#include <assert.h>
#include <numeric>
#include <mutex>
#include <regex>
#include <atomic>
#include <map>
#include <Common/config.h>
#include <Common/Order/orderstatus.h>
#include <Common/Order/order.h>
#include <Common/Order/fill.h>
#include <Common/Logger/logger.h>

using namespace std;

namespace EliteQuant {
	struct Position {
		string _account = "";
		string _api = "";
		string _fullsymbol = "";
		double _avgprice = 0;
		int _size = 0;
		int _pre_size = 0;
		int _freezed_size = 0;
		double _openpl = 0;			// unrealized pnl
		double _closedpl = 0;		// realized pnl

		template<class Archive>
		void serialize(Archive & ar) {
			ar(CEREAL_NVP(_account),
				CEREAL_NVP(_api),
				//cereal::make_nvp("H", _high),
				CEREAL_NVP(_fullsymbol),
				CEREAL_NVP(_avgprice),
				CEREAL_NVP(_size),
				CEREAL_NVP(_openpl),
				CEREAL_NVP(_closedpl)
			);
		};

		string toJson(const regex& p) {
			stringstream ss;
			{
				cereal::JSONOutputArchive oarchive(ss);
				oarchive(cereal::make_nvp("_portfolio", *this));
			}
			return regex_replace(ss.str(), p, "$1");
		}

		double Adjust(Fill& fill);

		void updatepnl(double mp);		// mp = market price

		void report();
	};
}
#endif  // _EliteQuant_Common_Position_H_