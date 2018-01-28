#ifndef _EliteQuant_Brokers_BtcChinaDataFeed_H_
#define _EliteQuant_Brokers_BtcChinaDataFeed_H_

#include <mutex>
#include <Common/config.h>
#include <Common/Data/marketdatafeed.h>
#include <map>
#include <string>

using std::mutex;
using std::string;
using std::list;
using std::vector;

namespace EliteQuant
{
	struct Security;

	class btcchinadatafeed : public marketdatafeed {
	public:
		btcchinadatafeed();
		~btcchinadatafeed();

		virtual void processMarketMessages();
		virtual bool connectToMarketDataFeed();
		virtual void disconnectFromMarketDataFeed();
		virtual bool isConnectedToMarketDataFeed() const;

		virtual void subscribeMarketData();
		virtual void unsubscribeMarketData(TickerId reqId);
		virtual void subscribeMarketDepth();
		virtual void unsubscribeMarketDepth(TickerId reqId);
		virtual void subscribeRealTimeBars(TickerId id, const Security& security, int barSize, const string& whatToShow, bool useRTH);
		virtual void unsubscribeRealTimeBars(TickerId tickerId);
		virtual void requestContractDetails();
		virtual void requestHistoricalData(string contract, string enddate, string duration, string barsize, string useRTH);
		virtual void requestMarketDataAccountInformation(const string& account);
	public:
		// events

	private:
        std::map<std::string, double> pricemap;
		string _host = "spotusd-data.btcc.com";
		void Thread_GetQuoteLoop();
	};
}

#endif // _EliteQuant_Brokers_BtcChinaDataFeed_H_
