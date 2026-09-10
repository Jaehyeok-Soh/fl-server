#pragma once
#include "Types.h"

#pragma pack(push, 1)
struct PACKET_HEADER
{
	uint16 PacketLength = { 0 };
	uint16 PacketId = { 0 };
	uint8 Type = { 0 };
};

const uint32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
#pragma pack(pop)