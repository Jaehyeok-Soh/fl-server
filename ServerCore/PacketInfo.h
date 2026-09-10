#pragma once
#include "Types.h"

#pragma pack(push, 1)
struct PacketInfo
{
	uint32 ClientIndex = { 0 };
	uint16 PacketId = { 0 };
	uint16 DataSize = { 0 };
	std::shared_ptr<char[]> pDataPtr;
};
#pragma pack(pop)