#pragma once

#include "Packet.h"
#include "PacketBuffer.h"

class User
{
public:
	User() = default;
	~User() = default;

	void Init(const UINT32 index);

	void Clear();

	int SetLogin(shared_ptr<char> userID_);

	void EnterRoom(INT32 roomIndex_);

	void SetDomainState(DOMAIN_STATE::Enum value_) { mCurDomainState = value_; }

	INT32 GetCurrentRoom() { return mRoomIndex; }
	INT32 GetNetConnIdx() { return mIndex; }
	string GetUserId() const { return mUserID; }
	DOMAIN_STATE::Enum GetDomainState() { return mCurDomainState; }

	void SetPacketData(const UINT32 dataSize_, shared_ptr<char[]> pData_);
	PacketInfo GetPacket();

private:
	INT32 mIndex = { -1 };

	INT32 mRoomIndex = { -1 };

	string mUserID = { "" };
	bool mIsConfirm = { false };
	string mAuthToken = { "" };

	DOMAIN_STATE::Enum mCurDomainState = { DOMAIN_STATE::NONE };

	PacketBuffer mPacketBuffer = {};
};

