#pragma once

#define WIN32_LEAN_AND_MEAN

#include "PacketInfo.h"

struct PacketData
{
	UINT32 ClientIndex = { 0 };
	UINT32 DataSize = { 0 };
	shared_ptr<char[]> pPacketData = {nullptr};

	void Set(PacketData& value);

	void Set(UINT32 sessionIndex_, UINT32 dataSize_, shared_ptr<char[]> pData);

	void Release();
};

