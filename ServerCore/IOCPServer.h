#pragma once

#pragma comment(lib, "ws2_32")

#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#include "ClientInfo.h"

class IOCPServer
{
public:
	IOCPServer();

	virtual ~IOCPServer();

	bool Init(const uint32 maxIOWorkerThreadCount_);
	bool BindandListen(int nBindPort);
	bool StartServer(const uint32 maxClientCount);
	void DestroyThread();
	bool SendMsg(const uint32 sessionIndex_, const uint32 dataSize_, shared_ptr<char[]> pData);

	virtual void OnConnect(const uint32 clientIndex_) = 0;
	virtual void OnClose(const uint32 clientIndex_) = 0;
	virtual void OnReceive(const uint32 clientIndex_, const uint32 size_, shared_ptr<char[]> pData_) = 0;

private:
	void CreateClient(const uint32 maxClientCount);
	shared_ptr<stClientInfo> GetEmptyClientInfo();
	shared_ptr<stClientInfo> GetClientInfo(const uint32 sessionIndex);

	bool CreateWorkerThread();
	bool CreateAccepterThread();

	void CreateSendThread();

	void WorkerThread();

	void AccepterThread();

	void SendThread();

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false);
	

private:
	uint32 MaxIOWorkerThreadCount = { 0 };

	vector<shared_ptr<stClientInfo>> mClientInfos;

	SOCKET mListenSocket = INVALID_SOCKET;

	int mClientCnt = { 0 };

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	bool mIsWorkerRun = true;
	vector<thread> mIOWorkerThreads;
	
	bool mIsAccepterRun = true;
	thread mAccepterThread;
	
	bool mIsSenderRun = false;
	thread mSendThread;
};