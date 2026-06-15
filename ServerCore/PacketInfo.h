#pragma once
#include <windows.h>
#include <memory>

#pragma pack(push, 1)
struct PacketInfo
{
	UINT32 ClientIndex = { 0 };
	UINT16 PacketId = { 0 };
	UINT16 DataSize = { 0 };
	shared_ptr<char[]> pDataPtr;
};
#pragma pack(pop)