#pragma once
#include "Enum_IOOperation.h"

struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped; //Overlapped I/O 구조체
	WSABUF m_wsaBuf; //Overlapped I/O 작업 버퍼
	IOOperation::Enum m_eOperation;
	UINT32 SessionIndex = 0;
};