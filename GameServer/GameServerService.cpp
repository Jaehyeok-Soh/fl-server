#include "pch.h"

#include "GameServerService.h"

GameServerService::~GameServerService()
{
}

void GameServerService::Run(const uint32 maxClient)
{
	auto sendPacketFunc = [&](uint32 clientIndex_, uint16 packetSize, shared_ptr<char[]> pSendPacket)
		{
			SendMsg(clientIndex_, packetSize, pSendPacket);
		};

	m_pPacketManager = make_unique<PacketManager>();
	m_pPacketManager->SendPacketFunc = sendPacketFunc;
	m_pPacketManager->Init(maxClient);
	m_pPacketManager->Run();

	StartServer(maxClient);
}

void GameServerService::End()
{
	m_pPacketManager->End();

	DestroyThread();
}

void GameServerService::OnConnect(const uint32 clientIndex_)
{
	spdlog::info("[OnConnect] client : index({})\n", clientIndex_);

	PacketInfo packet{ clientIndex_, PACKET_ID::SYS_USER_CONNECT, 0 };
	m_pPacketManager->PushSystemPacket(packet);

	packet = PacketInfo{ clientIndex_, PACKET_ID::SYS_USER_CONNECT_RESPONSE, 0 };
	m_pPacketManager->PushSystemPacket(packet);
}

void GameServerService::OnClose(const uint32 clientIndex_)
{
	spdlog::info("[OnClose] client : index({})\n", clientIndex_);

	PacketInfo packet{ clientIndex_, PACKET_ID::SYS_USER_DISCONNECT, 0 };
	m_pPacketManager->PushSystemPacket(packet);
}

void GameServerService::OnReceive(const uint32 clientIndex_, const uint32 size_, shared_ptr<char[]> pData_)
{
	spdlog::info("[OnReceive] client : index({}), dataSize({})\n", clientIndex_, size_);

	m_pPacketManager->ReceivePacketData(clientIndex_, size_, pData_);
}
