#pragma once

#include "PacketInfo.h"

class UserManager;
class RoomManager;

class PacketManager
{
public:
	PacketManager();
	~PacketManager();

	void Init(const UINT32 maxClient_);

	bool Run();

	void End();

	void ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_);

	void PushSystemPacket(PacketInfo packet_);

	function<void(UINT32, UINT32, shared_ptr<char[]>)> SendPacketFunc;

private:
	void CreateComponent(const UINT32 maxClient_);

	void ClearConnectionInfo(INT32 clientIndex_);

	void EnqueuePacketData(const UINT32 clientIndex_);
	PacketInfo DequePacketData();

	PacketInfo DequeSystemPacketData();

	void ProcessPacket();

	void ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, shared_ptr<char[]> pPacket_);

	void ProcessSysUserConnectResponse(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);

	void ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);
	void ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);

	void ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);
	void ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);

	void ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);
	void ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);
	void ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);

	void ProcessCharacterSync(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_);

private:
	using PROCESS_RECV_PACKET_FUNCTION = void(PacketManager::*)(UINT32, UINT16, shared_ptr<char[]>);
	unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFunctionDictionary;

	unique_ptr<UserManager> mUserManager;
	unique_ptr<RoomManager> mRoomManager;

	function<void(int, shared_ptr<char>)> mSendMQDataFunc;

	bool mIsRunProcessThread = false;

	thread mProcessThread;

	mutex mLock;

	deque<UINT32> mInComingPacketUserIndex;

	deque<PacketInfo> mSystemPacketQueue;
};

