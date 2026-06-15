#pragma once
#include <memory>

//TODO : 동적할당 최소화 방안 마련
//TODO : MemoryPool::Alloc(size);
template<typename T>
shared_ptr<char[]> MakePacketBuffer(T& packet)
{
	auto buf = make_shared<char[]>(sizeof(T));
	CopyMemory(buf.get(), &packet, sizeof(T));
	return buf;
}