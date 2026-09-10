#pragma once
#include "PacketManager.h"
#include "IOCPServer.h"

class GameServerService : public IOCPServer
{
public:
	GameServerService() = default;
	~GameServerService();

	void Run(const uint32 maxClient);

	void End();

	virtual void OnConnect(const uint32 clientIndex_) override;
	virtual void OnClose(const uint32 clientIndex_) override;
	virtual void OnReceive(const uint32 clientIndex_, const uint32 size_, shared_ptr<char[]> pData_) override;

private:
	unique_ptr<PacketManager> m_pPacketManager;
};
