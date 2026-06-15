#include "pch.h"

#include "UserManager.h"
#include "User.h"

void UserManager::Init(const UINT32 maxUserCount_)
{
	mMaxUserCnt = maxUserCount_;
	mUserObjPool = vector<shared_ptr<User>>(mMaxUserCnt);

	for (auto i = 0; i < mMaxUserCnt; i++)
	{
		mUserObjPool[i] = make_shared<User>();
		mUserObjPool[i]->Init(i);
	}
}

ERROR_CODE::Enum UserManager::AddUser(shared_ptr<char> userID_, int clientIndex_)
{
	auto user_idx = clientIndex_;

	mUserObjPool[user_idx]->SetLogin(userID_);
	mUserIDDictionary.insert(pair<char*, int>(userID_.get(), clientIndex_));

	return ERROR_CODE::NONE;
}

INT32 UserManager::FindUserIndexByID(const char* userID_)
{
	if (auto res = mUserIDDictionary.find(userID_); res != mUserIDDictionary.end())
	{
		return res->second;
	}

	return -1;
}

void UserManager::DeleteUserInfo(shared_ptr<User> user_)
{
	mUserIDDictionary.erase(user_->GetUserId());
	user_->Clear();
}

shared_ptr<User> UserManager::GetUserByConnIdx(INT32 clientIndex_)
{
	return mUserObjPool[clientIndex_];
}
