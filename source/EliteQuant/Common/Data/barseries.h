#ifndef __EliteQuant_Common_BarSeries_H__
#define __EliteQuant_Common_BarSeries_H__

#include <Common/config.h>
#include <Common/Data/bar.h>
#include <Common/Msgq/msgq.h>

namespace EliteQuant
{
	struct Bar;

	struct BarSeries {
		string fullsymbol;
		int interval_;			// bar interval in seconds
		vector<Bar> bars_;

		BarSeries();
		BarSeries(string sym, int interval);
		~BarSeries();

		void resize(int len);
		int getBarOrder(int time);
		bool newTick(const Tick& k);
		bool addBar(const string& s);
		string getLastUpdate(string name);

		template<class Archive>
		void serialize(Archive & ar) {
			ar(CEREAL_NVP(fullsymbol),
				CEREAL_NVP(interval_),
				CEREAL_NVP(bars_)
			);
		}

		string serialize() const;

		string toJson(const std::regex& p) {
			std::stringstream ss;
			{
				cereal::JSONOutputArchive oarchive(ss);
				oarchive(cereal::make_nvp("barseries", *this));
			}
			string r = regex_replace(ss.str(), p, "$1");
			return r;
		}
	};
}

#endif		// __EliteQuant_BarSeries_H__
