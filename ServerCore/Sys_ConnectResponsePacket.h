#pragma once

#include "PacketHeader.h"

#pragma pack(push,1)
struct SYS_CONNECT_RESPONSE_PACKET : public PACKET_HEADER
{
	uint32 ClientId = { 0 };
};

const uint32 SYS_CONNECT_RESPONSE_PACKET_SIZE = sizeof(SYS_CONNECT_RESPONSE_PACKET);
#pragma pack(pop)
