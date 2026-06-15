#pragma once

#include "PacketHeader.h"
#include "Define.h"

#pragma pack(push,1)
struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char UserID[MAX_USER_ID_LEN + 1] = {};
	char UserPW[MAX_USER_PW_LEN + 1] = {};
};

const size_t LOGIN_REQUEST_PACKET_SIZE = sizeof(LOGIN_REQUEST_PACKET);

struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16 Result = { 0 };
};
#pragma pack(pop)
