#pragma once

#include "Packet.h"
#include "PacketBuffer.h"

class User
{
public:
	User() = default;
	~User() = default;

	void Init(const uint32 index);

	void Clear();

	int SetLogin(shared_ptr<char> userID_);

	void EnterRoom(int32 roomIndex_);

	void SetDomainState(DOMAIN_STATE::Enum value_) { mCurDomainState = value_; }

	int32 GetCurrentRoom() { return mRoomIndex; }
	int32 GetNetConnIdx() { return mIndex; }
	string GetUserId() const { return mUserID; }
	DOMAIN_STATE::Enum GetDomainState() { return mCurDomainState; }

	void SetPacketData(const uint32 dataSize_, shared_ptr<char[]> pData_);
	PacketInfo GetPacket();

private:
	int32 mIndex = { -1 };

	int32 mRoomIndex = { -1 };

	string mUserID = { "" };
	bool mIsConfirm = { false };
	string mAuthToken = { "" };

	DOMAIN_STATE::Enum mCurDomainState = { DOMAIN_STATE::NONE };

	PacketBuffer mPacketBuffer = {};
};

