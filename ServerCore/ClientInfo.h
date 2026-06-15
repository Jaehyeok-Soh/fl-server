#pragma once

#include <winsock2.h>
#include <Ws2tcpip.h>

#include "Define.h"

#include "OverlappedEx.h"

struct stClientInfo
{
public:
	stClientInfo();
	
	~stClientInfo() = default;

	void Init(const UINT32 index, HANDLE iocpHandle_);
	
	UINT32 GetIndex() { return mIndex; }

	bool IsConnected() { return mIsConnect == 1; }

	SOCKET GetSock() { return mSocket; }

	UINT64 GetLastestClosedTimeSec() { return mLastestClosedTimeSec; }

	shared_ptr<char[]> RecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_);

	void Close(bool bIsForce = false);

	void Clear();

	bool PostAccept(SOCKET listenSock_, const UINT64 curTimeSec_);

	bool AcceptCompletion();

	bool BindIOCompletionPort(HANDLE iocpHandle_);
	
	bool BindRecv();

	bool SendMsg(const UINT32 dataSize_, shared_ptr<char[]> pMsg_);

	bool SendIO();

	void SendCompleted(const UINT32 dataSize_);

	bool SetSocketOption();

private:
	INT32 mIndex = { 0 };
	HANDLE mIOCPHandle = { INVALID_HANDLE_VALUE };

	INT64 mIsConnect = 0;
	UINT64 mLastestClosedTimeSec = 0;

	SOCKET mSocket;
	SOCKET mListenSocket = INVALID_SOCKET;

	stOverlappedEx mAcceptContext;
	char mAcceptBuf[64] = {};

	stOverlappedEx mRecvOverlappedEx;
	stOverlappedEx mSendOverlappedEx;

	shared_ptr<char[]> mRecvBuf = {};

	mutex mSendLock;
	atomic<bool> mIsSending = false;
	UINT64 mSendPos = 0;
	char mSendBuf[MAX_SOCK_SENDBUF];
	char mSendingBuf[MAX_SOCK_SENDBUF];
};