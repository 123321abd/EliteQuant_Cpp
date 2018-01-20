#include <mutex>
#include <Common/Brokerage/brokerage.h>
#include <Common/Order/orderstatus.h>
#include <Common/Order/ordermanager.h>
#include <Common/Logger/logger.h>
#include <Common/Security/portfoliomanager.h>

using namespace std;
namespace EliteQuant
{
	extern std::atomic<bool> gShutdown;

	mutex brokerage::mtx_CANCELALL;

	brokerage::brokerage() :
		_bkstate(BK_DISCONNECTED),
		m_brokerOrderId(-1)
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 500 * 1000;

		// message queue PUB factory
		if (CConfig::instance()._msgq == MSGQ::ZMQ) {
			//msgq_pair_ = std::make_unique<CMsgqZmq>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
			msgq_pair_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
		}
		else {
			msgq_pair_ = std::make_unique<CMsgqNanomsg>(MSGQ_PROTOCOL::PAIR, CConfig::instance().BROKERAGE_PAIR_PORT);
		}
	}

	brokerage::~brokerage() {
	}

	void brokerage::processBrokerageMessages() {
		if (!heatbeat(5)) {
			disconnectFromBrokerage();
			return;
		}
		switch (_bkstate) {
		case BK_GETORDERID:
			requestNextValidOrderID();
			break;
		case BK_ACCOUNT:
			requestBrokerageAccountInformation(CConfig::instance().account);
			break;
		case BK_ACCOUNTACK:
			break;
		case BK_READYTOORDER:
			monitorClientRequest();
			break;
		case BK_PLACEORDER_ACK:
			break;
		case BK_CANCELORDER:
			cancelOrder(0); //TODO
			break;
		case BK_CANCELORDER_ACK:
			break;
		}
	}

	//Keep calling this function from brokerage::processMessages()
	void brokerage::monitorClientRequest() {
		string msg = msgq_pair_->recmsg();

		if (msg.empty())
		{
			return;
		}

		PRINT_TO_FILE_AND_CONSOLE("INFO:[%s,%d][%s]msg received: %s\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());

		if (startwith(msg, CConfig::instance().close_all_msg)) {  // close all positions
			lock_guard<mutex> g(oid_mtx);
			PRINT_TO_FILE_AND_CONSOLE("INFO:[%s,%d][%s]Close all positions.\n", __FILE__, __LINE__, __FUNCTION__);

			for (auto iterator = PortfolioManager::instance()._positions.begin(); iterator != PortfolioManager::instance()._positions.end(); iterator++)
			{
				std::shared_ptr<Order> o = make_shared<Order>();
				o->fullSymbol = iterator->first;
				o->serverOrderId = m_serverOrderId;
				o->clientOrderId = -1;
				o->brokerOrderId = m_brokerOrderId;
				o->createTime = ymdhmsf();
				o->orderSize = (-1) * iterator->second._size;
				o->orderType = "MKT";
				o->orderStatus = OrderStatus::OS_NewBorn;
				m_serverOrderId++;
				m_brokerOrderId++;

				OrderManager::instance().trackOrder(o);
				placeOrder(o);
			}
		}
		else if (startwith(msg, CConfig::instance().new_order_msg)) {
			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);

			if (v[4] == "MKT")
			{
				if (v.size() >= 7)				// o|acc|ib|1|MKT|IBM|-500[|OrderFlag]
				{
					PRINT_TO_FILE_AND_CONSOLE("INFO[%s,%d][%s]receive market order: %s\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());
					std::shared_ptr<Order> o = make_shared<Order>();

					lock_guard<mutex> g(oid_mtx);
					//assert(m_orderId >= 0);			// start with 0
					//string oid = to_string(long(m_orderId));
					o->account = v[1];
					o->api = v[2];
					o->clientOrderId = stoi(v[3]);
					o->fullSymbol = v[5];
					o->orderSize = stoi(v[6]);
					o->serverOrderId = m_serverOrderId;
					o->brokerOrderId = m_brokerOrderId;
					o->createTime = ymdhmsf();
					
					o->orderType = "MKT";
					o->orderStatus = OrderStatus::OS_NewBorn;
					
					if (v.size() == 8) {
						o->orderFlag = static_cast<OrderFlag>(stoi(v[7]));
					}
					else {
						o->orderFlag = OrderFlag::OF_OpenPosition;
					}

					m_serverOrderId++;
					m_brokerOrderId++;

					OrderManager::instance().trackOrder(o);
					placeOrder(o);
				}
				else
				{
					PRINT_TO_FILE("ERROR:[%s,%d][%s]market order bad format.\n", __FILE__, __LINE__, __FUNCTION__);
				}
			}
			// TODO: assign order id here
			else if (v[4] == "LMT")			//o|acc|ib|1|LMT|IBM|-500|125.5[|OrderFlag]
			{
				if (v.size() >= 8)
				{
					//lock_guard<mutex> g(oid_mtx);
					PRINT_TO_FILE_AND_CONSOLE("INFO:[%s,%d][%s]recieved limit order: %s.\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());

					lock_guard<mutex> g(oid_mtx);
					// assert(m_orderId >= 0);			// start with 0

					std::shared_ptr<Order> o = make_shared<Order>();
					o->account = v[1];
					o->api = v[2];
					o->clientOrderId = stoi(v[3]);
					o->fullSymbol = v[5];
					o->orderSize = stoi(v[6]);
					o->limitPrice = stof(v[7]);

					o->serverOrderId = m_serverOrderId;
					o->brokerOrderId = m_brokerOrderId;
					o->createTime = ymdhmsf();
					o->orderType = "LMT";
					
					o->orderStatus = OrderStatus::OS_NewBorn;
					if (v.size() == 9) {
						o->orderFlag = static_cast<OrderFlag>(stoi(v[8]));
					}
					else {
						o->orderFlag = OrderFlag::OF_OpenPosition;
					}

					m_serverOrderId++;
					m_brokerOrderId++;

					OrderManager::instance().trackOrder(o);
					placeOrder(o);
				}
				else {
					PRINT_TO_FILE("ERROR:[%s,%d][%s]limit order bad format.\n", __FILE__, __LINE__, __FUNCTION__);
				}
			}
			else {
				PRINT_TO_FILE("ERROR:[%s,%d][%s]unrecognized order type.\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}	// endif new order
		else if (startwith(msg, CConfig::instance().cancel_order_msg)) {		// c|acc|api|server|client|broker
			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);
			if (v.size() == 6) {
				cancelOrder(atoi(v[3].c_str()));
			}
			else {
				PRINT_TO_FILE("ERROR:[%s,%d][%s]cancel order bad format.\n", __FILE__, __LINE__, __FUNCTION__);
			}
		} // endif cancel order
		// TODO: should this go to data thread?
		else if (startwith(msg, CConfig::instance().account_msg)) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]request account info %s.\n", __FILE__, __LINE__, __FUNCTION__, msg);

			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);
			requestBrokerageAccountInformation(v[1]);			// v[1] is account
		}	// endif account data request
		else if (startwith(msg, CConfig::instance().position_msg)) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]request position info.\n", __FILE__, __LINE__, __FUNCTION__);
			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);
			requestOpenPositions(v[1]);							// v[1] is account
		}	// endif position data request
		else if (startwith(msg, CConfig::instance().hist_msg)) {
			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);
			if (v.size() == 6) {
				//shared_ptr<IBBrokerage> ib = std::static_pointer_cast<IBBrokerage>(poms);
				//ib->requestHistoricalData(v[1], v[2], v[3], v[4], v[5]);
			}
			else {
				PRINT_TO_FILE("ERROR:[%s,%d][%s]historical data request bad format.\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}	// endif historical data request
		else if (startwith(msg, CConfig::instance().test_msg)) {
			static int count = 0;
			vector<string> v = stringsplit(msg, SERIALIZATION_SEPARATOR);
			PRINT_TO_FILE("INFO:[%s,%d][%s]TEST :%d,%s\n", __FILE__, __LINE__, __FUNCTION__, ++count, msg.c_str());
			if (v.size()>1) {
				string reverse(v[1].rbegin(), v[1].rend());
				msgq_pair_->sendmsg(CConfig::instance().test_msg + SERIALIZATION_SEPARATOR + reverse);
			}
		}
	}

	bool brokerage::isAllOrdersCancelled() {
		return OrderManager::instance().retrieveNonFilledOrderPtr().empty();
	}

	//******************************** Message serialization ***********************//
	void brokerage::sendOrderFilled(Fill& t) {
		// TODO: use OrderManager to check if the order is completely (not partially) filled. Then send one more message on order_status OS_FILLED
		string msg = CConfig::instance().fill_msg
			+ SERIALIZATION_SEPARATOR + t.serialize()
			+ SERIALIZATION_SEPARATOR + hmsf();

		msgq_pair_->sendmsg(msg);
	}

	void brokerage::sendOrderStatus(long serveroid) {
		std::shared_ptr<Order> o = OrderManager::instance().retrieveOrderFromServerOrderId(serveroid);

		if (o != nullptr)
		{
			string msg = CConfig::instance().order_status_msg
				+ SERIALIZATION_SEPARATOR + std::to_string(serveroid)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->clientOrderId)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->brokerOrderId)
				+ SERIALIZATION_SEPARATOR + o->fullSymbol
				+ SERIALIZATION_SEPARATOR + std::to_string(o->orderSize)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->orderFlag)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->limitPrice)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->stopPrice)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->filledSize)
				+ SERIALIZATION_SEPARATOR + std::to_string(o->avgFilledPrice)
				+ SERIALIZATION_SEPARATOR + o->createTime
				+ SERIALIZATION_SEPARATOR + o->cancelTime
				+ SERIALIZATION_SEPARATOR + o->account
				+ SERIALIZATION_SEPARATOR + o->api
				+ SERIALIZATION_SEPARATOR + std::to_string(int(o ? o->orderStatus : OrderStatus::OS_UNKNOWN))
				+ SERIALIZATION_SEPARATOR + hmsf();

			msgq_pair_->sendmsg(msg);
		}
		else {
			sendGeneralMessage("order status has invalid order id = " + std::to_string(serveroid));
		}
	}

	void brokerage::sendOpenPositionMessage(Position& pos) {
		//char str[512];
		//sprintf(str, "%d,%.4f,%.4f,%.4f", position, averageCost, unrealisedPNL, realisedPNL);
		//push(string(str));
		string msg = CConfig::instance().position_msg
			+ SERIALIZATION_SEPARATOR + pos._fullsymbol
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._avgprice)
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._size)
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._pre_size)
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._freezed_size)
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._closedpl)
			+ SERIALIZATION_SEPARATOR + std::to_string(pos._openpl)
			+ SERIALIZATION_SEPARATOR + pos._account
			+ SERIALIZATION_SEPARATOR + pos._api
			+ SERIALIZATION_SEPARATOR + hmsf();

		msgq_pair_->sendmsg(msg);
	}

	void brokerage::sendHistoricalBarMessage(string symbol, string time, double open, double high, double low, double close, int volume, int barcount, double wap)
	{
		string msg = CConfig::instance().hist_msg
			+ SERIALIZATION_SEPARATOR + symbol
			+ SERIALIZATION_SEPARATOR + time
			+ SERIALIZATION_SEPARATOR + std::to_string(open)
			+ SERIALIZATION_SEPARATOR + std::to_string(high)
			+ SERIALIZATION_SEPARATOR + std::to_string(low)
			+ SERIALIZATION_SEPARATOR + std::to_string(close)
			+ SERIALIZATION_SEPARATOR + std::to_string(volume)
			+ SERIALIZATION_SEPARATOR + std::to_string(barcount)
			+ SERIALIZATION_SEPARATOR + std::to_string(wap);

		msgq_pair_->sendmsg(msg);
	}

	void brokerage::sendAccountMessage() {
		// read from buffer
		string msg = CConfig::instance().account_msg
			+ SERIALIZATION_SEPARATOR + PortfolioManager::instance()._account.AccountID						// AccountID
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.PreviousDayEquityWithLoanValue)	// prev-day
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.NetLiquidation)				// balance
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.AvailableFunds)				// available
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.Commission)					// commission
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.FullMaintainanceMargin)		// margin
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.RealizedPnL)					// closed pnl
			+ SERIALIZATION_SEPARATOR + std::to_string(PortfolioManager::instance()._account.UnrealizedPnL)					// open pnl
			+ SERIALIZATION_SEPARATOR + hmsf();

		msgq_pair_->sendmsg(msg);
	}

	void brokerage::sendContractMessage(std::string symbol, string local_name, string min_tick) {
		string msg = CConfig::instance().contract_msg
			+ SERIALIZATION_SEPARATOR + symbol
			+ SERIALIZATION_SEPARATOR + local_name
			+ SERIALIZATION_SEPARATOR + min_tick;

		msgq_pair_->sendmsg(msg);
	}

	// comma separated general msg
	void brokerage::sendGeneralMessage(std::string gm) {
		string msg = CConfig::instance().general_msg 
			+ SERIALIZATION_SEPARATOR + gm
			+ SERIALIZATION_SEPARATOR + hmsf();

		msgq_pair_->sendmsg(msg);
	}

	//************************* End of message serialization ***********************//
}
