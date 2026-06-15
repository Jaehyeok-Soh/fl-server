#include "pch.h"

#include "RoomManager.h"
#include "Room.h"
#include "User.h"

RoomManager::~RoomManager() = default;

void RoomManager::Init(const UINT32 beginRoomNumber_, const INT32 maxRoomCount_, const INT32 maxRoomUserCount_)
{
	mBeginRoomNumber = beginRoomNumber_;
	mMaxRoomCount = maxRoomCount_;
	mEndRoomNumber = beginRoomNumber_ + maxRoomCount_;

	mRoomList = vector<shared_ptr<Room>>(maxRoomCount_);

	for (auto i = 0; i < maxRoomCount_; i++)
	{
		mRoomList[i] = make_shared<Room>();
		mRoomList[i]->SendPacketFunc = SendPacketFunc;
		mRoomList[i]->Init(i + beginRoomNumber_, maxRoomUserCount_);
	}
}

UINT16 RoomManager::EnterUser(INT32 roomNumber_, shared_ptr<User> user_)
{
	auto pRoom = GetRoomByNumber(roomNumber_);
	if (pRoom == nullptr)
		return ERROR_CODE::ROOM_INVALID_INDEX;

	return pRoom->EnterUser(user_);
}

INT16 RoomManager::LeaveUser(INT32 roomNumber_, shared_ptr<User> user_)
{
	auto pRoom = GetRoomByNumber(roomNumber_);
	if (pRoom == nullptr)
		return ERROR_CODE::ROOM_INVALID_INDEX;

	user_->SetDomainState(DOMAIN_STATE::LOGIN);
	pRoom->LeaveUser(user_);
	
	return ERROR_CODE::NONE;
}

shared_ptr<Room> RoomManager::GetRoomByNumber(INT32 number_)
{
	if (number_ < mBeginRoomNumber || number_ >= mEndRoomNumber)
		return nullptr;

	auto index = (number_ - mBeginRoomNumber);
	return mRoomList[index];
}
