#include "pch.h"

#include "PacketBuffer.h"
#include "PacketInfo.h"
#include "PacketHeader.h"

void PacketBuffer::Init(UINT32 bufferSize_)
{
	mPacketDataBuffer = make_shared<char[]>(bufferSize_);
}

void PacketBuffer::Clear()
{
	mPacketDataBufferWPos = { 0 };
	mPacketDataBufferRPos = { 0 };
}

void PacketBuffer::SetPacketData(const UINT32 dataSize_, shared_ptr<char[]> pData_)
{
	if (dataSize_ > PACKET_DATA_BUFFER_SIZE)
	{
		spdlog::error("[CRITICAL] Incoming data size ({}) is larger then total buffer size ({})", dataSize_, PACKET_DATA_BUFFER_SIZE);
	}

	if ((mPacketDataBufferWPos + dataSize_) >= PACKET_DATA_BUFFER_SIZE)
	{
		auto remainDataSize = mPacketDataBufferWPos - mPacketDataBufferRPos;

		if (remainDataSize > 0)
		{
			CopyMemory(mPacketDataBuffer.get(), GetPtr(mPacketDataBufferRPos), remainDataSize);
			mPacketDataBufferWPos = remainDataSize;
		}
		else
		{
			mPacketDataBufferWPos = { 0 };
		}

		mPacketDataBufferRPos = { 0 };

		if ((mPacketDataBufferWPos + dataSize_) >= PACKET_DATA_BUFFER_SIZE)
		{
			spdlog::error("[ERROR] Buffer overflow detected after compaction, WPos: {}, Incoming Size: {}", mPacketDataBufferWPos, dataSize_);
			return;
		}
	}

	CopyMemory(GetPtr(mPacketDataBufferWPos), pData_.get(), dataSize_);
	mPacketDataBufferWPos += dataSize_;
}

PacketInfo PacketBuffer::GetPacket()
{
	const int PACKET_SIZE_LENGTH = { 2 };
	const int PACKET_TYPE_LENGTH = { 2 };
	short packetSize = { 0 };

	UINT32 remainByte = mPacketDataBufferWPos - mPacketDataBufferRPos;

	if (remainByte < PACKET_HEADER_LENGTH)
		return PacketInfo();

	auto pHeader = (PACKET_HEADER*)GetPtr(mPacketDataBufferRPos);

	if (pHeader->PacketLength > remainByte)
		return PacketInfo();

	PacketInfo packetInfo;
	packetInfo.PacketId = pHeader->PacketId;
	packetInfo.DataSize = pHeader->PacketLength;
	packetInfo.pDataPtr = make_shared<char[]>(pHeader->PacketLength);
	CopyMemory(packetInfo.pDataPtr.get(), GetPtr(mPacketDataBufferRPos), pHeader->PacketLength);

	mPacketDataBufferRPos += pHeader->PacketLength;

	return packetInfo;
}

char* PacketBuffer::GetPtr(UINT32 offset)
{
	return mPacketDataBuffer.get() + offset;
}
