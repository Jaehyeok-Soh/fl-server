#include "pch.h"
#include <iostream>
#include "CorePch.h"
#include "GameServerService.h"

int main()
{
	GameServerService server;

	server.Init(MAX_IO_WORKER_THREAD);

	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);

	spdlog::info("quit : enter 'quit'\n");
	while (true)
	{
		string inputCmd;
		::getline(::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	return 0;
}