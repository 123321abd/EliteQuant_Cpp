﻿#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <sstream>
#include <mutex>

#include <Brokers/Ctp/ctpdatafeed.h>
#include <Brokers/Ctp/ThostFtdcMdApi.h>
#include <Common/Util/util.h>
#include <Common/Order/orderstatus.h>
#include <Common/Logger/logger.h>
#include <Common/Data/datamanager.h>
#include <Common/Order/ordermanager.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

namespace EliteQuant
{
	extern std::atomic<bool> gShutdown;

	ctpdatafeed::ctpdatafeed() 
		: loginReqId_(0)
		, isConnected_(false)
	{
		// 创建ctp目录
		string path = CConfig::instance().logDir() + "/ctp/";
		boost::filesystem::path dir(path.c_str());
		boost::filesystem::create_directory(dir);

		// 创建API对象
		this->api_ = CThostFtdcMdApi::CreateFtdcMdApi(path.c_str());
		///注册回调接口
		///@param pSpi 派生自回调接口类的实例
		this->api_->RegisterSpi(this);
	}

	ctpdatafeed::~ctpdatafeed() {
		if (api_ != NULL) {
			disconnectFromMarketDataFeed();
		}
	}

	// start http request thread
	bool ctpdatafeed::connectToMarketDataFeed()
	{
		if (!isConnected_) {
			///注册前置机网络地址
			///@param pszFrontAddress：前置机网络地址。
			///@remark 网络地址的格式为：“protocol://ipaddress:port”，如：”tcp://127.0.0.1:17001”。 
			///@remark “tcp”代表传输协议，“127.0.0.1”代表服务器地址。”17001”代表服务器端口号。
			this->api_->RegisterFront((char*)CConfig::instance().ctp_data_address.c_str());		// 服务器地址

			// 初始化运行环境, 只有调用后, 接口才开始工作
			// if success, server calls for onFrontConnected
			this->api_->Init();

			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp datasource connecting...!\n", __FILE__, __LINE__, __FUNCTION__);
		}

		return true;
	}

	// stop http request thread
	void ctpdatafeed::disconnectFromMarketDataFeed() {
		this->api_->RegisterSpi(NULL);
		this->api_->Release();
		this->api_ = NULL;
		isConnected_ = false;
	}

	// is http request thread running ?
	bool ctpdatafeed::isConnectedToMarketDataFeed() const {
		return isConnected_;
	}

	void ctpdatafeed::processMarketMessages() {
		if (!heatbeat(5)) {
			disconnectFromMarketDataFeed();
			return;
		}

		switch (_mkstate) {
		case MK_ACCOUNT:
			requestMarketDataAccountInformation(CConfig::instance().account);
			break;
		case MK_ACCOUNTACK:
			break;
		case MK_REQCONTRACT:
			break;
		case MK_REQCONTRACT_ACK:
			break;
		case MK_REQREALTIMEDATA:
			subscribeMarketData();
			break;
		case MK_REQREALTIMEDATAACK:
			break;
		}
	}

	////////////////////////////////////////////////////// outgoing function ///////////////////////////////////////

	void ctpdatafeed::subscribeMarketData() {
		for (auto it = CConfig::instance().securities.begin(); it != CConfig::instance().securities.end(); ++it)
		{
			char* buffer = (char*)(*it).c_str();
			char* myreq[1] = { buffer };
			int i = this->api_->SubscribeMarketData(myreq, 1);		// parameter 1 = myreq has one contract; return 0 = 发送订阅行情请求失败
		}

		/*int count = instruments.size();
		char* *allInstruments = new char*[count];
		for (int i = 0; i < count; i++) {
		allInstruments[i] = new char[7];
		strcpy(allInstruments[i], instruments.at(i).toStdString().c_str());
		}
		api_->SubscribeMarketData(allInstruments, count);*/

		if (_mkstate <= MK_REQREALTIMEDATAACK)
			_mkstate = MK_REQREALTIMEDATAACK;
	}

	void ctpdatafeed::unsubscribeMarketData(TickerId reqId) {
		for (auto it = CConfig::instance().securities.begin(); it != CConfig::instance().securities.end(); ++it)
		{
			char* buffer = (char*)(*it).c_str();
			char* myreq[1] = { buffer };
			int i = this->api_->UnSubscribeMarketData(myreq, 1);		// parameter 1 = myreq has one contract; return 0 = 发送订阅行情请求失败
		}
	}

	void ctpdatafeed::subscribeMarketDepth() {
	}

	void ctpdatafeed::unsubscribeMarketDepth(TickerId reqId) {
	}

	void ctpdatafeed::subscribeRealTimeBars(TickerId id, const Security& security, int barSize, const string& whatToShow, bool useRTH) {

	}

	void ctpdatafeed::unsubscribeRealTimeBars(TickerId tickerId) {

	}

	void ctpdatafeed::requestContractDetails() {
	}

	void ctpdatafeed::requestHistoricalData(string contract, string enddate, string duration, string barsize, string useRTH) {

	}

	// 当前置机连接后 (OnFrontConnected), 用户开始请求登陆
	// 登陆成功后调用 OnRspUserLogin
	void ctpdatafeed::requestMarketDataAccountInformation(const string& account)
	{
		requestUserLogin();

		if (_mkstate <= MK_ACCOUNTACK)
			_mkstate = MK_ACCOUNTACK;
	}

	void ctpdatafeed::requestUserLogin() {
		PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server logging in....\n", __FILE__, __LINE__, __FUNCTION__);

		//CThostFtdcReqUserLoginField loginReq;
		//memset(&loginReq, 0, sizeof(loginReq));
		CThostFtdcReqUserLoginField loginField = CThostFtdcReqUserLoginField();

		strcpy(loginField.BrokerID, CConfig::instance().ctp_broker_id.c_str());
		strcpy(loginField.UserID, CConfig::instance().ctp_user_id.c_str());
		strcpy(loginField.Password, CConfig::instance().ctp_password.c_str());

		///用户登录请求
		int i = this->api_->ReqUserLogin(&loginField, loginReqId_++);		// i=0： 发送登录请求失败
	}

	///登出请求
	void ctpdatafeed::requestUserLogout(int nRequestID) {
		CThostFtdcUserLogoutField logoutField = CThostFtdcUserLogoutField();

		strcpy(logoutField.BrokerID, CConfig::instance().ctp_broker_id.c_str());
		strcpy(logoutField.UserID, CConfig::instance().ctp_user_id.c_str());

		int i = this->api_->ReqUserLogout(&logoutField, nRequestID);
	}
	/////////////////////////////////////////////// end of outgoing functions ///////////////////////////////////////

	////////////////////////////////////////////////////// incoming function ///////////////////////////////////////
	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
	void ctpdatafeed::OnFrontConnected() {
		 _mkstate = MK_CONNECTED;			// not used
		isConnected_ = true;

		PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server is connected.\n", __FILE__, __LINE__, __FUNCTION__);
	}

	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	///@param nReason 错误原因
	///        0x1001 网络读失败
	///        0x1002 网络写失败
	///        0x2001 接收心跳超时
	///        0x2002 发送心跳失败
	///        0x2003 收到错误报文
	void ctpdatafeed::OnFrontDisconnected(int nReason) {
		_mkstate = MK_DISCONNECTED;			// not used
		isConnected_ = false;

		PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server disconnected, nReason=%d.\n", __FILE__, __LINE__, __FUNCTION__, nReason);
	}

	void ctpdatafeed::OnHeartBeatWarning(int nTimeLapse) {
		PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server heartbeat overtime error, nTimeLapse=%d.\n", __FILE__, __LINE__, __FUNCTION__, nTimeLapse);
	}

	///错误应答
	void ctpdatafeed::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server error: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);	// TODO: decode to 'gbk'	
	}

	///登录请求响应
	void ctpdatafeed::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		if (pRspInfo->ErrorID == 0) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server user logged in; TradingDay=%s, LoginTime=%s, BrokerID=%s, UserID=%s.\n", __FILE__, __LINE__, __FUNCTION__,
				pRspUserLogin->TradingDay, pRspUserLogin->LoginTime, pRspUserLogin->BrokerID, pRspUserLogin->UserID);

			if (_mkstate <= MK_REQREALTIMEDATA)
				_mkstate = MK_REQREALTIMEDATA;
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server user login failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);	// TODO: decode to 'gbk'	
		}
	}

	///登出请求响应
	void ctpdatafeed::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		if (pRspInfo->ErrorID == 0) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server user logged out; BrokerID=%s, UserID=%s.\n", __FILE__, __LINE__, __FUNCTION__, pUserLogout->BrokerID, pUserLogout->UserID);
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server user login failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);	// TODO: decode to 'gbk'
		}
	}

	///订阅行情应答
	void ctpdatafeed::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		bool bResult = pRspInfo && (pRspInfo->ErrorID != 0);
		if (!bResult) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server OnRspSubMarketData: InstrumentID=%s.\n", __FILE__, __LINE__, __FUNCTION__, pSpecificInstrument->InstrumentID);
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server OnRspSubMarketData failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
		}

	}

	///取消订阅行情应答
	void ctpdatafeed::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		bool bResult = pRspInfo && (pRspInfo->ErrorID != 0);
		if (!bResult) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server OnRspUnSubMarketData: InstrumentID=%s.\n", __FILE__, __LINE__, __FUNCTION__, pSpecificInstrument->InstrumentID);
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server OnRspUnSubMarketData failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
		}
	}

	///订阅询价应答
	void ctpdatafeed::OnRspSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		bool bResult = pRspInfo && (pRspInfo->ErrorID != 0);
		if (!bResult) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server OnRspSubForQuoteRsp: InstrumentID=%s.\n", __FILE__, __LINE__, __FUNCTION__, pSpecificInstrument->InstrumentID);
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server OnRspSubForQuoteRsp failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
		}
	}

	///取消订阅询价应答
	void ctpdatafeed::OnRspUnSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
		bool bResult = pRspInfo && (pRspInfo->ErrorID != 0);
		if (!bResult) {
			PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server OnRspUnSubForQuoteRsp: InstrumentID=%s.\n", __FILE__, __LINE__, __FUNCTION__, pSpecificInstrument->InstrumentID);
		}
		else {
			PRINT_TO_FILE("ERROR:[%s,%d][%s]Ctp data server OnRspUnSubForQuoteRsp failed: ErrorID=%d, ErrorMsg=%s.\n", __FILE__, __LINE__, __FUNCTION__, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
		}
	}

	/// 深度行情通知
	/// CTP只有一档行情
	void ctpdatafeed::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData) {
		// TODO: add pDepthMarketData->ExchangeID to fullsymbol			ExchangeInstID=合约在交易所的代码
		// TODO: use pDepthMarketData->UpdateTime instead of now	
		FullTick k;

		k.time_ = hmsf();
		k.fullsymbol_ = pDepthMarketData->InstrumentID;
		k.price_ = pDepthMarketData->LastPrice;
		k.size_ = pDepthMarketData->Volume;			// not valid without volume
		k.bidprice_L1_ = pDepthMarketData->BidPrice1;
		k.bidsize_L1_ = pDepthMarketData->BidVolume1;
		k.askprice_L1_ = pDepthMarketData->AskPrice1;
		k.asksize_L1_ = pDepthMarketData->AskVolume1;
		k.open_interest = pDepthMarketData->OpenInterest;
		k.open_ = pDepthMarketData->OpenPrice;
		k.high_ = pDepthMarketData->HighestPrice;
		k.low_ = pDepthMarketData->LowestPrice;
		k.pre_close_ = pDepthMarketData->PreClosePrice;
		k.upper_limit_price_ = pDepthMarketData->UpperLimitPrice;
		k.lower_limit_price_ = pDepthMarketData->LowerLimitPrice;
		
		msgq_pub_->sendmsg(k.serialize());
	}

	///询价通知
	void ctpdatafeed::OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp) {
		PRINT_TO_FILE("INFO:[%s,%d][%s]Ctp data server OnRtnForQuoteRsp; TradingDay=%s, ExchangeID=%s, InstrumentID=%s, ForQuoteSysID=%s.\n", __FILE__, __LINE__, __FUNCTION__,
			pForQuoteRsp->TradingDay, pForQuoteRsp->ExchangeID, pForQuoteRsp->InstrumentID, pForQuoteRsp->ForQuoteSysID);
	}
	/////////////////////////////////////////////// end of incoming functions ///////////////////////////////////////

	/////////////////////////////////////////////// begin auxiliary functions ///////////////////////////////////////
	string ctpdatafeed::SecurityFullNameToCtpSymbol(const std::string& symbol) {
		return symbol;
	}

	string ctpdatafeed::CtpSymbolToSecurityFullName(const std::string& symbol) {
		return symbol;
	}
	/////////////////////////////////////////////// end auxiliary functions ///////////////////////////////////////
}
