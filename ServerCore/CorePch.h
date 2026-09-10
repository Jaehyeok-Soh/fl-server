#pragma once

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "mswsock.lib")
#include <mswsock.h>

#pragma region thirdparty

// git clone https://github.com/microsoft/vcpkg.git
// cd vcpkg
// C:\vcpkg\bootstrap-vcpkg.bat
// C:\vcpkg\vcpkg integrate install

// C:\vcpkg\vcpkg install spdlog:x64-windows
#include "spdlog/spdlog.h"

// C:\vcpkg\vcpkg install hiredis:x64-windows
//#include <hiredis/hiredis.h>

#pragma endregion

#include <string>
#include <span>
#include <vector>
#include <array>
#include <list>
#include <queue>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <mutex>
#include <cstring>
#include <utility>
#include <functional>
#include <ctime>
#include <variant>
#include <iostream>
#include <memory>
#include <stack>
#include <set>

using namespace std;

#include "Types.h"
#include "Server_Defines.h"
#include "Define.h"
#include "Enums.h"

void ServerCore();


