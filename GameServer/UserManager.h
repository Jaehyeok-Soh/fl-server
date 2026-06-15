#pragma once

class User;

class UserManager
{
public:
	UserManager() = default;
	~UserManager() = default;

	void Init(const UINT32 maxUserCount_);

	INT32 GetCurrentUserCnt() { return mCurrentUserCnt; }

	INT32 GetMaxUserCnt() { return mMaxUserCnt; }

	void IncreaseUserCnt() { mCurrentUserCnt++; }

	void DecreaseUserCnt() { if (mCurrentUserCnt > 0) mCurrentUserCnt--; }

	ERROR_CODE::Enum AddUser(shared_ptr<char> userID_, int clientIndex_);

	INT32 FindUserIndexByID(const char* userID_);

	void DeleteUserInfo(shared_ptr<User> user_);

	shared_ptr<User> GetUserByConnIdx(INT32 clientIndex_);

private:
	INT32 mMaxUserCnt = { 0 };
	INT32 mCurrentUserCnt = { 0 };

	vector<shared_ptr<User>> mUserObjPool;
	unordered_map<string, int> mUserIDDictionary;
};

