#pragma once

class Room;
class User;

class RoomManager
{
public:
	RoomManager() = default;
	~RoomManager();

	void Init(const UINT32 beginRoomNumber_, const INT32 maxRoomCount_, const INT32 maxRoomUserCount_);

	UINT GetMaxRoomCount() { return mMaxRoomCount; }

	UINT16 EnterUser(INT32 roomNumber_, shared_ptr<User> user_);

	INT16 LeaveUser(INT32 roomNumber_, shared_ptr<User> user_);

	shared_ptr<Room> GetRoomByNumber(INT32 number_);

	function<void(UINT32, UINT16, shared_ptr<char[]>)> SendPacketFunc;

private:
	vector<shared_ptr<Room>> mRoomList;
	INT32 mBeginRoomNumber = { 0 };
	INT32 mEndRoomNumber = { 0 };
	INT32 mMaxRoomCount = { 0 };
};

