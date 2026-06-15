#include "pch.h"
#include "Packet.h"

void PacketData::Set(PacketData& value)
{
	ClientIndex = value.ClientIndex;
	DataSize = value.DataSize;

	pPacketData = make_shared<char[]>(value.DataSize);
	CopyMemory(pPacketData.get(), value.pPacketData.get(), value.DataSize);
}

void PacketData::Set(UINT32 sessionIndex_, UINT32 dataSize_, shared_ptr<char[]> pData)
{
	ClientIndex = sessionIndex_;
	DataSize = dataSize_;

	pPacketData = make_shared<char[]>(dataSize_);
	CopyMemory(pPacketData.get(), pData.get(), dataSize_);
}

void PacketData::Release()
{
	pPacketData.reset();
}
