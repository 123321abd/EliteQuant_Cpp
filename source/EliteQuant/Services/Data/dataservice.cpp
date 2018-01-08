#include <atomic>
#include <Services/Data/dataservice.h>
#include <Common/config.h>
#include <Common/Util/util.h>
#include <Common/Time/timeutil.h>
#include <Common/Logger/logger.h>
#include <Common/Data/datatype.h>
#include <Common/Data/datamanager.h>
#include <Common/Data/tickwriter.h>
#include <Common/Data/tickreader.h>
#include <Common/Order/ordermanager.h>
#include <Common/Order/ordertype.h>
#include <Common/Security/portfoliomanager.h>
#include <Common/Data/marketdatafeed.h>
#include <Strategy/strategyFactory.h>

using namespace std;

namespace EliteQuant
{
	extern std::atomic<bool> gShutdown;
	atomic<uint64_t> MICRO_SERVICE_NUMBER(0);

	void MarketDataService(shared_ptr<marketdatafeed> pdata, int clientid) {
		bool conn = false;
		int count = 0;

		pdata->_mode = TICKBAR;
		while (!gShutdown) {
			conn = pdata->connectToMarketDataFeed();

			if (conn && pdata->isConnectedToMarketDataFeed()) {
				msleep(2000);
				pdata->_mkstate = MK_ACCOUNT;			// after it is connected, start with MK_ACCOUNT
				while (!gShutdown && pdata->isConnectedToMarketDataFeed()) {
					pdata->processMarketMessages();
					msleep(10);
				}
			}
			else {
				msleep(5000);
				count++;
				if (count % 10 == 0) {
					PRINT_TO_FILE_AND_CONSOLE("ERROR:[%s,%d][%s]Cannot connect to Market Data Feed\n", __FILE__, __LINE__, __FUNCTION__);
				}
			}
		}
		pdata->disconnectFromMarketDataFeed();
		PRINT_TO_FILE("INFO:[%s,%d][%s]disconnect from market data feed.\n", __FILE__, __LINE__, __FUNCTION__);
	}

	void DataBoardService() {
		try {
			std::unique_ptr<CMsgq> msgq_sub_;

			// message queue factory
			if (CConfig::instance()._msgq == MSGQ::ZMQ) {
				//msgq_sub_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
				msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
			}
			else {
				msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
			}
	
			while (!gShutdown) {
				string msg = msgq_sub_->recmsg();

				if (!msg.empty()) {
					vector<string> vs = stringsplit(msg, SERIALIZATION_SEPARATOR);
					if (vs.size() == 6)		// Always Tick; actual contents are determined by DataType
					{
						Tick k;
						k.fullsymbol_ = vs[0];
						k.time_ = atoi(vs[1].c_str());
						k.datatype_ = (DataType)(atoi(vs[2].c_str()));
						k.price_ = atof(vs[3].c_str());
						k.size_ = atoi(vs[4].c_str());
						k.depth_ = atoi(vs[5].c_str());

						if (DataManager::instance()._latestmarkets.find(k.fullsymbol_) != DataManager::instance()._latestmarkets.end()) {
							DataManager::instance().SetTickValue(k);
							// PRINT_TO_FILE("ERROR:[%s,%d][%s]%s.\n", __FILE__, __LINE__, __FUNCTION__, buf);
						}
					}
					else if (vs.size() == 17)		// Always Tick; actual contents are determined by DataType
					{
						FullTick k;
						k.fullsymbol_ = vs[0];
						k.time_ = atoi(vs[1].c_str());
						k.datatype_ = (DataType)(atoi(vs[2].c_str()));
						k.price_ = atof(vs[3].c_str());
						k.size_ = atoi(vs[4].c_str());
						k.depth_ = atoi(vs[5].c_str());
						k.bidprice_L1_ = atoi(vs[6].c_str());
						k.bidsize_L1_ = atoi(vs[7].c_str());
						k.askprice_L1_ = atoi(vs[8].c_str());
						k.asksize_L1_ = atoi(vs[9].c_str());
						k.open_interest = atoi(vs[10].c_str());
						k.open_ = atoi(vs[11].c_str());
						k.high_ = atoi(vs[12].c_str());
						k.low_ = atoi(vs[13].c_str());
						k.pre_close_ = atoi(vs[14].c_str());
						k.upper_limit_price_ = atoi(vs[15].c_str());
						k.lower_limit_price_ = atoi(vs[16].c_str());

						if (DataManager::instance()._latestmarkets.find(k.fullsymbol_) != DataManager::instance()._latestmarkets.end()) {
							DataManager::instance().SetTickValue(k);
							// PRINT_TO_FILE("ERROR:[%s,%d][%s]%s.\n", __FILE__, __LINE__, __FUNCTION__, buf);
						}
					}
				}
			}
		}
		catch (exception& e) {
			PRINT_TO_FILE_AND_CONSOLE("ERROR:[%s,%d][%s]%s.\n", __FILE__, __LINE__, __FUNCTION__, e.what());
		}
		catch (...) {
		}
		PRINT_TO_FILE("INFO:[%s,%d][%s]disconnect from market data feed.\n", __FILE__, __LINE__, __FUNCTION__);
	}

	void BarAggregatorServcie() {
		// TODO: separate from DataBoardService
	}

	void TickRecordingService() {
		std::unique_ptr<CMsgq> msgq_sub_;

		// message queue factory
		if (CConfig::instance()._msgq == MSGQ::ZMQ) {
			//msgq_sub_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
			msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
		}
		else {
			msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT, false);
		}

		string ymdstr = ymd();
		string fname = CConfig::instance().dataDir() + "/marketdata-" + ymdstr + ".txt";
		FILE* fp = fopen(fname.c_str(), "a+");
		TickWriter fwriter;
		fwriter.fp = fp;

		char *buf = nullptr;
		if (fp) {
			while (!gShutdown) {
				string msg = msgq_sub_->recmsg(0);

				if (!msg.empty())
					fwriter.put(msg);
			}
		}
		//PRINT_TO_FILE("INFO:[%s,%d][%s]recording service stopped: %s\n", __FILE__, __LINE__, __FUNCTION__);
	}

	void TickReplayService(const std::string& filetoreplay)
	{
		std::unique_ptr<CMsgq> msgq_pub_;

		// message queue factory
		if (CConfig::instance()._msgq == MSGQ::ZMQ) {
			//msgq_pub_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::PUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
			msgq_pub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
		}
		else {
			msgq_pub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
		}

		uint64_t curt = 0;
		uint64_t logt = 0;
		vector<TimeAndMsg> lines = readreplayfile(filetoreplay);
		int i = 0, sz = lines.size();
		while (!gShutdown && i++ < sz) {
			logt = lines[i].t;
			curt = getMicroTime();
			static uint64_t diff = curt - logt;      //89041208806

			while (!gShutdown && (diff + logt > curt)) {
				curt = getMicroTime();
			}
			string& msg = lines[i].msg;

			msgq_pub_->sendmsg(msg);
		}
		msleep(2000);
		PRINT_TO_FILE("INFO:[%s,%d][%s]replay service stopped: %s\n", __FILE__, __LINE__, __FUNCTION__);
	}
}
