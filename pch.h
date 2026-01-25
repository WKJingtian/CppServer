#include <iostream>
#include <thread>
#include <mutex>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <chrono>

#include <functional>
#include <type_traits>
#include <assert.h>

#include <vector>
#include <unordered_set>
#include <set>
#include <map>

#include "Const.h"

#include "mysql/include/jdbc/mysql_connection.h"
#include "mysql/include/mysql/jdbc.h"
#include "mysql/include/mysqlx/xdevapi.h"

#include "Utils/ThreadPool.h"
#include "Utils/ReadWriteLock.h"
#include "Utils/TickInfoUtil.h"

#include "Database/MySqlMgr.h"

#include "Room/RoomMgr.h"

#include "Player/PlayerMgr.h"
#include "Player/Player.h"

#include "Net/NetPack.h"
#include "Net/NetPackHandler.h"
#include "Net/RpcEnum.h"
#include "Net/RpcError.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#pragma comment(lib, "Ws2_32.lib")

#define FIXED_TIME_STEP 200