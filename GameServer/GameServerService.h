#pragma once
#include "PacketManager.h"
#include "IOCPServer.h"

class GameServerService : public IOCPServer
{
public:
	GameServerService() = default;
	~GameServerService();

	void Run(const UINT32 maxClient);

	void End();

	virtual void OnConnect(const UINT32 clientIndex_) override;
	virtual void OnClose(const UINT32 clientIndex_) override;
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_) override;

private:
	unique_ptr<PacketManager> m_pPacketManager;
};
