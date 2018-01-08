#include <Common/config.h>
#include <Common/Util/util.h>
#include <Common/Time/timeutil.h>
#include <Common/Logger/logger.h>
#include <Common/Data/datatype.h>
#include <Common/Order/ordermanager.h>
#include <Common/Data/datamanager.h>
#include <Common/Data/marketdatafeed.h>
#include <Services/Strategy/strategyservice.h>
#include <Strategy/strategyFactory.h>

#include <Strategy/smacross.h>
#include <atomic>
#ifdef _WIN32
#include <nanomsg/src/nn.h>
#include <nanomsg/src/pair.h>
#else
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#endif

using namespace std;

namespace EliteQuant
{
	extern std::atomic<bool> gShutdown;

	void StrategyManagerService() {
		std::unique_ptr<CMsgq> msgq_sub_;
		std::unique_ptr<CMsgq> msgq_pair_;

		// message queue factory
		if (CConfig::instance()._msgq == MSGQ::ZMQ) {
			//msgq_sub_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
			//msgq_pair_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
			msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
			msgq_pair_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
		}
		else {
			msgq_sub_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::SUB, CConfig::instance().MKT_DATA_PUBSUB_PORT);
			msgq_pair_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
		}
		
		// load strategy
		auto strategy = make_unique<SmaCross>();
		strategy->initialize();

		char *buf = nullptr;
		string msg;
		while (!gShutdown) {
			msg = msgq_sub_->recmsg(0);		
			if (!msg.empty()) {
				vector<string> vs = stringsplit(msg, SERIALIZATION_SEPARATOR);
				if ((vs.size() == 6)	|| (vs.size() == 17))	// Always Tick; actual contents are determined by DataType
				{
					Tick k;
					k.fullsymbol_ = vs[0];
					k.time_ = atoi(vs[1].c_str());
					k.datatype_ = (DataType)(atoi(vs[2].c_str()));
					k.price_ = atof(vs[3].c_str());
					k.size_ = atoi(vs[4].c_str());
					k.depth_ = atoi(vs[5].c_str());

					strategy->OnTick(k);
				}
			}

			msg = msgq_pair_->recmsg(1);
			
			if (!msg.empty()) {
				strategy->OnGeneralMessage(msg);
			}
		}
		PRINT_TO_FILE("INFO:[%s,%d][%s]Exit strategy thread\n", __FILE__, __LINE__, __FUNCTION__);
	}
}
