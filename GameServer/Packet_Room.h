#pragma once

#include "PacketHeader.h"

#pragma pack(push,1)
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	INT32 RoomNumber = { 0 };
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result = { 0 };
};

struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result = { 0 };
};

struct ROOM_JOIN_PACKET : public PACKET_HEADER
{
	INT32 ClientIndex = { 0 };
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };
};

struct ROOM_LEAVE_PACKET : public PACKET_HEADER
{
	INT32 ClientIndex = { 0 };
};
#pragma pack(pop)
