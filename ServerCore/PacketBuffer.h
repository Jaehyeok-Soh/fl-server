#pragma once

struct PacketInfo;

class PacketBuffer
{
public:
	void Init(UINT32 bufferSize_);
	void Clear();

	void SetPacketData(const UINT32 dataSize_, shared_ptr<char[]> pData_);
	PacketInfo GetPacket();

	char* GetPtr(UINT32 offset = 0);

private:
	UINT32 mPacketDataBufferWPos = { 0 };
	UINT32 mPacketDataBufferRPos = { 0 };
	shared_ptr<char[]> mPacketDataBuffer;
};

