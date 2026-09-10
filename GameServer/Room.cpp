#include "pch.h"

#include "Room.h"
#include "User.h"

#include "Packet_GamesServer.h"

void Room::Init(const int32 roomNum_, const int32 maxUserCount_)
{
	mRoomNum = roomNum_;
	mMaxUserCount = maxUserCount_;
}

int16 Room::EnterUser(shared_ptr<User> user_)
{
	vector<shared_ptr<User>> existingUsers;

	{
		lock_guard<mutex> lock(m_RoomLock);

		if (mCurrentUserCount >= mMaxUserCount)
			return ERROR_CODE::ENTER_ROOM_FULL_USER;

		existingUsers.reserve(mUserList.size());
		for(const auto& pUser : mUserList)
		{
			if (pUser != nullptr)
				existingUsers.push_back(pUser);
		}

		mUserList.push_back(user_);
		++mCurrentUserCount;

		user_->EnterRoom(mRoomNum);
	}

	for (auto& pOldUser : existingUsers)
	{
		ROOM_JOIN_PACKET oldUserPkt = {};
		oldUserPkt.PacketId = PACKET_ID::ROOM_JOIN_NOTIFY;
		oldUserPkt.PacketLength = sizeof(ROOM_JOIN_PACKET);
		oldUserPkt.ClientIndex = pOldUser->GetNetConnIdx();
		strncpy_s(oldUserPkt.UserID, sizeof(oldUserPkt.UserID), pOldUser->GetUserId().c_str(), _TRUNCATE);

		SendPacketFunc(user_->GetNetConnIdx(), sizeof(ROOM_JOIN_PACKET), MakePacketBuffer(oldUserPkt));
	}

	NotifyNewGuest(user_->GetNetConnIdx(), user_->GetUserId().c_str());

	return ERROR_CODE::NONE;
}

void Room::LeaveUser(shared_ptr<User> leaveUser_)
{
	lock_guard<mutex> lock(m_RoomLock);

	const auto beforeSize = mUserList.size();

	mUserList.remove_if([leaveClientId = leaveUser_->GetNetConnIdx()](shared_ptr<User> pUser)
		{
			return leaveClientId == pUser->GetNetConnIdx();
		});

	const auto afterSize = mUserList.size();

	if (beforeSize > afterSize)
		mCurrentUserCount -= static_cast<uint16>(beforeSize - afterSize);

	{
		for (auto& pStayUser : mUserList)
		{
			ROOM_LEAVE_PACKET stayUserPkt = {};
			stayUserPkt.PacketId = PACKET_ID::ROOM_LEAVE_NOTIFY;
			stayUserPkt.PacketLength = sizeof(ROOM_JOIN_PACKET);
			stayUserPkt.ClientIndex = leaveUser_->GetNetConnIdx();

			SendPacketFunc(pStayUser->GetNetConnIdx(), sizeof(ROOM_JOIN_PACKET), MakePacketBuffer(stayUserPkt));
		}
	}
}

void Room::NotifyChat(int32 clientIndex_, const char* userID_, const char* msg_)
{
	ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt = {};
	roomChatNtfyPkt.PacketId = PACKET_ID::ROOM_CHAT_NOTIFY;
	roomChatNtfyPkt.PacketLength = sizeof(roomChatNtfyPkt);

	CopyMemory(roomChatNtfyPkt.Msg, msg_, sizeof(roomChatNtfyPkt.Msg));
	CopyMemory(roomChatNtfyPkt.UserID, userID_, sizeof(roomChatNtfyPkt.UserID));
	SendToAllUser(sizeof(roomChatNtfyPkt), MakePacketBuffer(roomChatNtfyPkt), clientIndex_, false);
}

void Room::NotifyNewGuest(int32 clientIndex_, const char* userID_)
{
	ROOM_JOIN_PACKET newGuestPkt = {};
	newGuestPkt.PacketId = PACKET_ID::ROOM_JOIN_NOTIFY;
	newGuestPkt.PacketLength = sizeof(ROOM_JOIN_PACKET);
	newGuestPkt.ClientIndex = clientIndex_;
	strncpy_s(newGuestPkt.UserID, sizeof(newGuestPkt.UserID), userID_, _TRUNCATE);

	shared_ptr<char[]> sharedNewGuestPkt = MakePacketBuffer(newGuestPkt);

	SendToAllUser(sizeof(ROOM_JOIN_PACKET), sharedNewGuestPkt, clientIndex_, true);
}

void Room::CharacterSync(int32 clientIndex_, shared_ptr<char[]> pData)
{
	SendToAllUser(sizeof(CHARACTER_SYNC_PACKET), pData, clientIndex_, false);
}

void Room::SendToAllUser(const uint16 dataSize_, shared_ptr<char[]> data_, const int32 passUserIndex_, bool exceptMe)
{
	vector<shared_ptr<User>> userListSnapshot;

	{
		lock_guard<mutex> lock(m_RoomLock);
		userListSnapshot.reserve(mUserList.size());

		for (const auto& pUser : mUserList)
		{
			if (pUser != nullptr)
				userListSnapshot.push_back(pUser);
		}
	}

	for (auto& pUser : userListSnapshot)
	{
		if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_)
			continue;

		SendPacketFunc(pUser->GetNetConnIdx(), static_cast<uint32>(dataSize_), data_);
	}
}
