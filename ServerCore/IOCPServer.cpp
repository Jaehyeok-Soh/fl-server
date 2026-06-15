#include "pch.h"

#include "IOCPServer.h"

IOCPServer::IOCPServer()
{
}

IOCPServer::~IOCPServer()
{
	WSACleanup();
}

bool IOCPServer::Init(const UINT32 maxIOWorkerThreadCount_)
{
	WSADATA wsaData;

	int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (nRet != 0)
	{
		spdlog::error("[ERROR] WSAStartup() failed : {}\n", WSAGetLastError());
		return false;
	}

	// TCP, Create Overlapped I/O Socket
	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (mListenSocket == INVALID_SOCKET)
	{
		spdlog::error("[ERROR] socket() failed : {}\n", WSAGetLastError());
		return false;
	}

	MaxIOWorkerThreadCount = maxIOWorkerThreadCount_;

	spdlog::info("init complete\n");
	return true;
}

bool IOCPServer::BindandListen(int nBindPort)
{
	SOCKADDR_IN stServerAddr;
	stServerAddr.sin_family = AF_INET;
	stServerAddr.sin_port = htons(nBindPort);
	stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int nRet = ::bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
	if (nRet != 0)
	{
		spdlog::error("[ERROR] bind() failed : %d\n", WSAGetLastError());
		return false;
	}

	nRet = listen(mListenSocket, 5);
	if (nRet != 0)
	{
		spdlog::error("[ERROR] listen() failed : %d\n", WSAGetLastError());
		return false;
	}

	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxIOWorkerThreadCount);
	if (mIOCPHandle == NULL)
	{
		spdlog::error("[ERROR] CreateIoCompletionPort() failed: %d\n", GetLastError());
		return false;
	}

	auto hIOCPHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
	if (hIOCPHandle == nullptr)
	{
		spdlog::error("[ERROR] failed bind listen socket IOCP : %d\n", WSAGetLastError());
		return false;
	}

	spdlog::info("server register complete\n");
	return true;
}

bool IOCPServer::StartServer(const UINT32 maxClientCount)
{
	CreateClient(maxClientCount);

	bool bRet = CreateWorkerThread();
	if (bRet == false)
	{
		return false;
	}

	bRet = CreateAccepterThread();
	if (bRet == false)
	{
		return false;
	}

	//CreateSendThread();

	spdlog::info("server start\n");
	return true;
}

void IOCPServer::DestroyThread()
{
	mIsSenderRun = false;

	if (mSendThread.joinable())
	{
		mSendThread.join();
	}

	mIsAccepterRun = false;
	closesocket(mListenSocket);

	if (mAccepterThread.joinable())
	{
		mAccepterThread.join();
	}

	mIsWorkerRun = false;
	CloseHandle(mIOCPHandle);

	for (auto& th : mIOWorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}
}

bool IOCPServer::SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, shared_ptr<char[]> pData)
{
	auto pClient = GetClientInfo(sessionIndex_);
	return pClient->SendMsg(dataSize_, pData);
}

void IOCPServer::CreateClient(const UINT32 maxClientCount)
{
	for (UINT32 i = 0; i < maxClientCount; i++)
	{
		auto client = make_shared<stClientInfo>();
		client->Init(i, mIOCPHandle);

		mClientInfos.push_back(client);
	}
}

shared_ptr<stClientInfo> IOCPServer::GetEmptyClientInfo()
{
	for (auto& client : mClientInfos)
	{
		if (client.get()->IsConnected() == false)
		{
			return client;
		}
	}

	return nullptr;
}

shared_ptr<stClientInfo> IOCPServer::GetClientInfo(const UINT32 sessionIndex)
{
	return mClientInfos[sessionIndex];
}

bool IOCPServer::CreateWorkerThread()
{
	unsigned int uiThreadId = { 0 };

	// WaitingThread Queue에 대기 상태로 권장되는 쓰레드 개수 : (cpu 개수 * 2 + 1)
	for (UINT32 i = 0; i < MaxIOWorkerThreadCount; i++)
	{
		mIOWorkerThreads.emplace_back([this]() { WorkerThread(); });
	}

	spdlog::info("WorkerThread start\n");
	return true;
}

bool IOCPServer::CreateAccepterThread()
{
	mIsAccepterRun = true;
	mAccepterThread = thread([this]() { AccepterThread(); });

	spdlog::info("AccepterThread start\n");
	return true;
}

void IOCPServer::CreateSendThread()
{
	mIsSenderRun = true;
	mSendThread = thread([this]() { SendThread(); });
	spdlog::info("SenderThread start\n");
}

void IOCPServer::WorkerThread()
{
	stClientInfo* pClientInfo = nullptr;
	BOOL bSuccess = TRUE;
	DWORD dwIoSize = { 0 };
	LPOVERLAPPED lpOverlapped = NULL;

	while (mIsWorkerRun)
	{
		bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
			&dwIoSize,
			(PULONG_PTR)&pClientInfo,
			&lpOverlapped,
			INFINITE);

		if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL)
		{
			mIsWorkerRun = false;
			continue;
		}

		if (lpOverlapped == NULL)
		{
			continue;
		}

		auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

		if (bSuccess == FALSE || (dwIoSize == 0 && pOverlappedEx->m_eOperation != IOOperation::ACCEPT))
		{
			if (pClientInfo != nullptr)
			{
				spdlog::info("socket({}) connection lost\n", (int)pClientInfo->GetSock());
				CloseSocket(pClientInfo);
			}
			continue;
		}


		if (pOverlappedEx->m_eOperation == IOOperation::ACCEPT)
		{
			pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex).get();
			if (pClientInfo->AcceptCompletion())
			{
				++mClientCnt;

				OnConnect(pClientInfo->GetIndex());
			}
			else
			{
				CloseSocket(pClientInfo, true);
			}
		}
		else if (IOOperation::RECV == pOverlappedEx->m_eOperation)
		{
			OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());

			spdlog::info("[RECEIVED] bytes : {} , msg : {}\n", dwIoSize, pClientInfo->RecvBuffer().get());

			pClientInfo->BindRecv();
		}
		else if (IOOperation::SEND == pOverlappedEx->m_eOperation)
		{
			pClientInfo->SendCompleted(dwIoSize);
		}
		else
		{
			spdlog::warn("[EXCEPTION] socket({})\n", pClientInfo->GetIndex());
		}
	}
}

void IOCPServer::AccepterThread()
{
	while (mIsAccepterRun)
	{
		auto curTimeSec = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now().time_since_epoch()).count();

		for (auto client : mClientInfos)
		{
			if (client->IsConnected())
				continue;

			if ((UINT64)curTimeSec < client->GetLastestClosedTimeSec())
				continue;
			
			auto diff = curTimeSec - client->GetLastestClosedTimeSec();
			if (diff <= RE_USE_SESSION_WAIT_TIMESEC)
				continue;

			client->PostAccept(mListenSocket, curTimeSec);
		}

		this_thread::sleep_for(chrono::milliseconds(32));
	}
}

void IOCPServer::SendThread()
{
	while (mIsSenderRun)
	{
		for (auto client : mClientInfos)
		{
			if (client->IsConnected() == false)
				continue;

			if (client->GetSock() == INVALID_SOCKET)
				continue;

			client->SendIO();
		}

		this_thread::sleep_for(chrono::milliseconds(8));
	}
}

void IOCPServer::CloseSocket(stClientInfo* pClientInfo, bool bIsForce)
{
	if (pClientInfo->IsConnected() == false)
		return;

	auto clientIndex = pClientInfo->GetIndex();

	pClientInfo->Close(bIsForce);

	OnClose(clientIndex);
}
