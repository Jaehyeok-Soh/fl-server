#include "pch.h"

#include "PacketManager.h"

#include "Sys_ConnectResponsePacket.h"

#include "UserManager.h"
#include "User.h"

#include "RoomManager.h"
#include "Room.h"

#include "Packet_GamesServer.h"

PacketManager::PacketManager() = default;

PacketManager::~PacketManager() = default;

void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFunctionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFunctionDictionary[PACKET_ID::SYS_USER_CONNECT_RESPONSE] = &PacketManager::ProcessSysUserConnectResponse;

	mRecvFunctionDictionary[PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFunctionDictionary[PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;

	mRecvFunctionDictionary[PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFunctionDictionary[PACKET_ID::LOGIN_RESPONSE] = &PacketManager::ProcessLogin;

	mRecvFunctionDictionary[PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFunctionDictionary[PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFunctionDictionary[PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;

	mRecvFunctionDictionary[PACKET_ID::CHARACTER_SYNC] = &PacketManager::ProcessCharacterSync;

	CreateComponent(maxClient_);
}

bool PacketManager::Run()
{
	mIsRunProcessThread = true;
	mProcessThread = thread([this]() { ProcessPacket(); });

	return true;
}

void PacketManager::End()
{
	mIsRunProcessThread = false;

	if (mProcessThread.joinable())
		mProcessThread.join();
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	EnqueuePacketData(clientIndex_);
}

void PacketManager::PushSystemPacket(PacketInfo packet_)
{
	lock_guard<mutex> guard(mLock);
	mSystemPacketQueue.push_back(packet_);
}

void PacketManager::CreateComponent(const UINT32 maxClient_)
{
	mUserManager = make_unique<UserManager>();
	mUserManager->Init(maxClient_);

	// TODO : 하드코딩 제거
	UINT32 startRoomNumber = { 0 };
	UINT32 maxRoomCount = { 10 };
	UINT32 maxRoomUserCount = { 4 };
	mRoomManager = make_unique<RoomManager>();
	mRoomManager->SendPacketFunc = SendPacketFunc;
	mRoomManager->Init(startRoomNumber, maxRoomCount, maxRoomUserCount);
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pReqUser->GetDomainState() == DOMAIN_STATE::ROOM)
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pReqUser);
	}

	if (pReqUser->GetDomainState() != DOMAIN_STATE::NONE)
		mUserManager->DeleteUserInfo(pReqUser);
}

void PacketManager::EnqueuePacketData(const UINT32 clientIndex_)
{
	lock_guard<mutex> guard(mLock);
	mInComingPacketUserIndex.push_back(clientIndex_);
}

PacketInfo PacketManager::DequePacketData()
{
	UINT32 userIndex = 0;

	lock_guard<mutex> guard(mLock);
	if (mInComingPacketUserIndex.empty())
		return PacketInfo();

	userIndex = mInComingPacketUserIndex.front();
	mInComingPacketUserIndex.pop_front();

	auto pUser = mUserManager->GetUserByConnIdx(userIndex);
	auto packetData = pUser->GetPacket();
	packetData.ClientIndex = userIndex;

	return packetData;
}

PacketInfo PacketManager::DequeSystemPacketData()
{
	lock_guard<mutex> guard(mLock);
	if (mSystemPacketQueue.empty())
		return PacketInfo();

	auto packetData = mSystemPacketQueue.front();
	mSystemPacketQueue.pop_front();

	return packetData;
}

void PacketManager::ProcessPacket()
{
	while (mIsRunProcessThread)
	{
		bool isIdle = true;

		if (auto packetData = DequePacketData(); packetData.PacketId > PACKET_ID::SYS_END)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);

			auto pUser = mUserManager->GetUserByConnIdx(packetData.ClientIndex);
			while (true)
			{
				auto nextPacket = pUser->GetPacket();
				if (nextPacket.PacketId <= PACKET_ID::SYS_END)
					break;

				ProcessRecvPacket(packetData.ClientIndex, nextPacket.PacketId, nextPacket.DataSize, nextPacket.pDataPtr);
			}
		}

		if (auto packetData = DequeSystemPacketData(); packetData.PacketId != 0)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (isIdle)
		{
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}
}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	auto iter = mRecvFunctionDictionary.find(packetId_);
	if (iter != mRecvFunctionDictionary.end())
		(this->*(iter->second))(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessSysUserConnectResponse(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	SYS_CONNECT_RESPONSE_PACKET connectResPacket = {};
	connectResPacket.PacketId = PACKET_ID::SYS_USER_CONNECT_RESPONSE;
	connectResPacket.PacketLength = sizeof(SYS_CONNECT_RESPONSE_PACKET);

	connectResPacket.ClientId = clientIndex_;
	SendPacketFunc(clientIndex_, sizeof(SYS_CONNECT_RESPONSE_PACKET), MakePacketBuffer(connectResPacket));
}

void PacketManager::ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	spdlog::info("[ProcessUserConnect] clientIndex : {}\n", clientIndex_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->Clear();
}

void PacketManager::ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	spdlog::info("[ProcessUserDisConnect] clientIndex : {}\n", clientIndex_);
	ClearConnectionInfo(clientIndex_);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
		return;

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_.get());

	spdlog::info("requested user id = {}\n", pLoginReqPacket->UserID);

	LOGIN_RESPONSE_PACKET loginResPacket = {};
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt())
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}

	if (mUserManager->FindUserIndexByID(pLoginReqPacket->UserID) == -1)
	{
		mUserManager->AddUser(make_shared<char>(*pLoginReqPacket->UserID), clientIndex_);

		loginResPacket.Result = ERROR_CODE::NONE;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}
	else
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	spdlog::info("ProcessLoginDBResult. UserIndex: {}", clientIndex_);

	//auto pBody = reinterpret_cast<RedisLoginRes*>(pPacket_.get());

	//if (pBody->Result == ERROR_CODE::NONE)
	//{
	//	// 로그인 완료 처리
	//}

	//LOGIN_RESPONSE_PACKET loginResPacket = {};
	//loginResPacket.PacketId = PACKET_ID::LOGIN_RESPONSE;
	//loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	//loginResPacket.Result = pBody->Result;
	//SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
}

void PacketManager::ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pPacket_.get());
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (!pReqUser || pReqUser == nullptr)
		return;

	ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket = {};
	roomEnterResPacket.PacketId = PACKET_ID::ROOM_ENTER_RESPONSE;
	roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);
	roomEnterResPacket.Result = mRoomManager->EnterUser(pRoomEnterReqPacket->RoomNumber, pReqUser);

	SendPacketFunc(clientIndex_, sizeof(ROOM_ENTER_RESPONSE_PACKET), MakePacketBuffer(roomEnterResPacket));
	spdlog::info("[ENTER ROOM] Response packet sended\n");
}

void PacketManager::ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket = {};
	roomLeaveResPacket.PacketId = PACKET_ID::ROOM_LEAVE_RESPONSE;
	roomLeaveResPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();

	roomLeaveResPacket.Result = mRoomManager->LeaveUser(roomNum, reqUser);
	SendPacketFunc(clientIndex_, sizeof(ROOM_LEAVE_RESPONSE_PACKET), MakePacketBuffer(roomLeaveResPacket));
}

void PacketManager::ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomChatReqPacket = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pPacket_.get());

	ROOM_CHAT_RESPONSE_PACKET roomChatResPacket = {};
	roomChatResPacket.PacketId = PACKET_ID::ROOM_CHAT_RESPONSE;
	roomChatResPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);
	roomChatResPacket.Result = ERROR_CODE::NONE;

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();

	auto pRoom = mRoomManager->GetRoomByNumber(roomNum);
	if (pRoom == nullptr)
	{
		roomChatResPacket.Result = ERROR_CODE::CHAT_ROOM_INVALID_ROOM_NUMBER;
		SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), MakePacketBuffer(roomChatResPacket));
		return;
	}

	SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), MakePacketBuffer(roomChatResPacket));

	pRoom->NotifyChat(clientIndex_, reqUser->GetUserId().c_str(), pRoomChatReqPacket->Message);
}

void PacketManager::ProcessCharacterSync(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	auto pCharacterSyncPacket = reinterpret_cast<CHARACTER_SYNC_PACKET*>(pPacket_.get());

	CHARACTER_SYNC_PACKET characterSyncBroadCastPacket = {};
	characterSyncBroadCastPacket = *pCharacterSyncPacket;
	characterSyncBroadCastPacket.PacketLength = sizeof(CHARACTER_SYNC_PACKET);
	characterSyncBroadCastPacket.PacketId = PACKET_ID::CHARACTER_SYNC_BROADCAST;

	auto now = chrono::steady_clock::now().time_since_epoch();
	characterSyncBroadCastPacket.TimeStamp = static_cast<UINT32>(chrono::duration_cast<chrono::milliseconds>(now).count());

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();

	auto pRoom = mRoomManager->GetRoomByNumber(roomNum);
	if (pRoom == nullptr)
		return;

	pRoom->CharacterSync(clientIndex_, MakePacketBuffer(characterSyncBroadCastPacket));
}

