#pragma once

#define WIN32_LEAN_AND_MEAN

#include "PacketInfo.h"

#pragma pack(push, 1)
struct PacketData
{
	uint32 ClientIndex = { 0 };
	uint32 DataSize = { 0 };
	shared_ptr<char[]> pPacketData = {nullptr};

	void Set(PacketData& value);

	void Set(uint32 sessionIndex_, uint32 dataSize_, shared_ptr<char[]> pData);

	void Release();
};
#pragma pack(pop)
