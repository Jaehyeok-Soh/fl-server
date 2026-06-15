#pragma once

#pragma comment(lib, "ws2_32")

#include <winsock2.h>
#include <Ws2tcpip.h>

#include <thread>
#include <vector>

#include "ClientInfo.h"

using namespace std;

class IOCPServer
{
public:
	IOCPServer();

	virtual ~IOCPServer();

	bool Init(const UINT32 maxIOWorkerThreadCount_);
	bool BindandListen(int nBindPort);
	bool StartServer(const UINT32 maxClientCount);
	void DestroyThread();
	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, shared_ptr<char[]> pData);

	virtual void OnConnect(const UINT32 clientIndex_) = 0;
	virtual void OnClose(const UINT32 clientIndex_) = 0;
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_) = 0;

private:
	void CreateClient(const UINT32 maxClientCount);
	shared_ptr<stClientInfo> GetEmptyClientInfo();
	shared_ptr<stClientInfo> GetClientInfo(const UINT32 sessionIndex);

	bool CreateWorkerThread();
	bool CreateAccepterThread();

	void CreateSendThread();

	void WorkerThread();

	void AccepterThread();

	void SendThread();

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false);
	

private:
	UINT32 MaxIOWorkerThreadCount = { 0 };

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