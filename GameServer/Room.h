#pragma once

class User;

class Room
{
public:
	Room() = default;
	~Room() = default;

	INT32 GetMaxUserCount() { return mMaxUserCount; }
	INT32 GetCurrentUserCount() { return mCurrentUserCount; }
	INT32 GetRoomNumber() { return mRoomNum; }

	void Init(const INT32 roomNum_, const INT32 maxUserCount_);

	INT16 EnterUser(shared_ptr<User> user_);

	void LeaveUser(shared_ptr<User> leaveUser_);

	void NotifyChat(INT32 clientIndex_, const char* userID_, const char* msg_);

	void NotifyNewGuest(INT32 clientIndex_, const char* userID_);

	void CharacterSync(INT32 clientIndex_, shared_ptr<char[]> pData);

	function<void(UINT32, UINT32, shared_ptr<char[]>)> SendPacketFunc;

private:
	void SendToAllUser(const UINT16 dataSize_, shared_ptr<char[]> data_, const INT32 passUserIndex_, bool exceptMe);

	INT32 mRoomNum = { -1 };
	list<shared_ptr<User>> mUserList;
	INT32 mMaxUserCount = { 0 };
	UINT16 mCurrentUserCount = { 0 };

	mutex m_RoomLock;
};

