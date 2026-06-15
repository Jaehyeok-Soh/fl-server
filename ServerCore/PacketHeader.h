#pragma once
#include <windows.h>

struct PACKET_HEADER
{
	UINT16 PacketLength = { 0 };
	UINT16 PacketId = { 0 };
	UINT8 Type = { 0 };
};

const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
