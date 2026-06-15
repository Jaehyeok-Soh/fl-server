#include "pch.h"

#include "ClientInfo.h"

stClientInfo::stClientInfo()
{
	ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
	ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));
	mSocket = INVALID_SOCKET;
	mRecvBuf = make_shared<char[]>(MAX_SOCKBUF);
	ZeroMemory(mRecvBuf.get(), MAX_SOCKBUF);
}

void stClientInfo::Init(const UINT32 index, HANDLE iocpHandle_)
{
	mIndex = index;
	mIOCPHandle = iocpHandle_;
}

bool stClientInfo::OnConnect(HANDLE iocpHandle_, SOCKET socket_)
{
	mSocket = socket_;
	mIsConnect = 1;

	Clear();

	if (BindIOCompletionPort(iocpHandle_) == false)
	{
		return false;
	}

	return BindRecv();
}

void stClientInfo::Close(bool bIsForce)
{
	linger stLinger = { 0, 0 }; // SO_DONTLINGER

	// bIsForce == true :: SO_LINGER, timeout = 0 // force shutdown, warning : data lost
	if (bIsForce == true)
	{
		stLinger.l_onoff = 1;
	}

	shutdown(mSocket, SD_BOTH);

	setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

	mIsConnect = 0;
	mLastestClosedTimeSec = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now().time_since_epoch()).count();

	closesocket(mSocket);

	mSocket = INVALID_SOCKET;
}

void stClientInfo::Clear()
{
	mSendPos = 0;
	mIsSending = false;
}

bool stClientInfo::PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_)
{
	spdlog::info("PostAccept. client Index: {}\n", GetIndex());

	mListenSocket = listenSock_;

	mLastestClosedTimeSec = UINT32_MAX;

	mSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
		NULL, 0, WSA_FLAG_OVERLAPPED);
	if (mSocket == INVALID_SOCKET)
	{
		spdlog::error("[ERROR] client Socket WSASocket : {}\n", GetLastError());
		return false;
	}

	ZeroMemory(&mAcceptContext, sizeof(stOverlappedEx));

	DWORD bytes = 0;
	DWORD flags = 0;
	mAcceptContext.m_wsaBuf.len = 0;
	mAcceptContext.m_wsaBuf.buf = nullptr;
	mAcceptContext.m_eOperation = IOOperation::ACCEPT;
	mAcceptContext.SessionIndex = mIndex;

	if (FALSE == AcceptEx(listenSock_, mSocket, mAcceptBuf, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			spdlog::error("AcceptEx Error : {}\n", WSAGetLastError());
			return false;
		}
	}

	return true;
}

bool stClientInfo::AcceptCompletion()
{
	spdlog::info("AcceptCompletion : SessionIndex({})\n", mIndex);

	if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)&mListenSocket,
		sizeof(SOCKET)))
	{
		spdlog::error("SO_UPDATE_ACCEPT_CONTEXT failed: {}", WSAGetLastError());
		return false;
	}

	if (OnConnect(mIOCPHandle, mSocket) == false)
		return false;

	SOCKADDR_IN stClientAddr;
	int nAddrLen = sizeof(SOCKADDR_IN);
	char clientIP[32] = { 0, };
	inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
	spdlog::info("Client Connect : IP({}) SOCKET({})\n", clientIP, (int)mSocket);

	return true;
}

bool stClientInfo::BindIOCompletionPort(HANDLE iocpHandle_)
{
	auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(),
		iocpHandle_,
		(ULONG_PTR)(this),
		0);

	spdlog::info("BindIOCP result: {} socket: {}", (INT64)hIOCP, (int)GetSock());

	if (hIOCP == INVALID_HANDLE_VALUE)
	{
		spdlog::error("[ERROR] CreateIoCompletionPort() failed: {}", GetLastError());
		return false;
	}

	return true;
}

bool stClientInfo::BindRecv()
{
	DWORD dwFlag = { 0 };
	DWORD dwRecvNumBytes = { 0 };

	mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
	mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf.get();
	mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

	int nRet = WSARecv(mSocket,
		&(mRecvOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		&dwFlag,
		(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		spdlog::error("[ERROR] WSARecv() failed : {}\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool stClientInfo::SendMsg(const UINT32 dataSize_, shared_ptr<char[]> pMsg_)
{
	lock_guard<mutex> guard(mSendLock);

	if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF)
		mSendPos = 0;

	auto pSendBuf = &mSendBuf[mSendPos];

	CopyMemory(pSendBuf, pMsg_.get(), dataSize_);
	mSendPos += dataSize_;

	if (!mIsSending)
		SendIO();

	return true;
}

bool stClientInfo::SendIO()
{
	if (mSendPos <= 0 || mIsSending)
		return true;

	if (mSocket == INVALID_SOCKET)
		return false;

	spdlog::info("SendIO socket handle: {}", (int)mSocket);

	int optVal = { 0 };
	int optLen = sizeof(optVal);
	int ret = getsockopt(mSocket, SOL_SOCKET, SO_TYPE, (char*)&optVal, &optLen);
	spdlog::info("getsockopt SO_TYPE ret: {} optVal: {} err: {}", ret, optVal, WSAGetLastError());

	ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));

	mIsSending = true;

	CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

	mSendOverlappedEx.m_wsaBuf.len = mSendPos;
	mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
	mSendOverlappedEx.m_eOperation = IOOperation::SEND;

	DWORD dwRecvNumBytes = { 0 };
	int nRet = WSASend(mSocket,
		&(mSendOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		0,
		(LPWSAOVERLAPPED) & (mSendOverlappedEx),
		NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		spdlog::error("[ERROR] WSASend() failed : {}\n", WSAGetLastError());
		return false;
	}

	mSendPos = 0;
	return true;
}

void stClientInfo::SendCompleted(const UINT32 dataSize_)
{
	mIsSending = false;
	spdlog::info("[Send Completed] bytes : {}\n", dataSize_);
}

bool stClientInfo::SetSocketOption()
{
	int opt = 1;
	if (SOCKET_ERROR == setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)))
	{
#ifdef _DEBUG
		spdlog::debug("[DEBUF] TCP_NODELAY error : {}\n", GetLastError());
#endif // DEBUG
		return false;
	}

	opt = 0;
	if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&opt, sizeof(int)))
	{
#ifdef _DEBUG
		spdlog::debug("[DEBUG] SO_RCVBUF change error : {}\n", GetLastError());
#endif // _DEBUG
		return false;
	}

	return true;
}
