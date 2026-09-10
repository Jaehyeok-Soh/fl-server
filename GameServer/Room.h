#pragma once

class User;

class Room
{
public:
	Room() = default;
	~Room() = default;

	int32 GetMaxUserCount() { return mMaxUserCount; }
	int32 GetCurrentUserCount() { return mCurrentUserCount; }
	int32 GetRoomNumber() { return mRoomNum; }

	void Init(const int32 roomNum_, const int32 maxUserCount_);

	INT16 EnterUser(shared_ptr<User> user_);

	void LeaveUser(shared_ptr<User> leaveUser_);

	void NotifyChat(int32 clientIndex_, const char* userID_, const char* msg_);

	void NotifyNewGuest(int32 clientIndex_, const char* userID_);

	void CharacterSync(int32 clientIndex_, shared_ptr<char[]> pData);

	function<void(uint32, uint32, shared_ptr<char[]>)> SendPacketFunc;

private:
	void SendToAllUser(const uint16 dataSize_, shared_ptr<char[]> data_, const int32 passUserIndex_, bool exceptMe);

	int32 mRoomNum = { -1 };
	list<shared_ptr<User>> mUserList;
	int32 mMaxUserCount = { 0 };
	uint16 mCurrentUserCount = { 0 };

	mutex m_RoomLock;
};

