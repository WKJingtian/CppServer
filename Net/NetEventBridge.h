#pragma once
#include "CppServerAPI.h"
#include "Net/NetEventQueue.h"
#include <vector>

class INetEngine;
class CPPSERVER_API NetEventBridge
{
public:
	static void Dispatch(const std::vector<NetEvent>& events, INetEngine* engine);
};
