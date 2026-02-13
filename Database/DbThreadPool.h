#pragma once
#include "Utils/ThreadPool.h"

class DbThreadPool : public ThreadPool
{
	DbThreadPool(size_t threadMax) : ThreadPool(threadMax) {}
	DbThreadPool(const DbThreadPool&) = delete;
	DbThreadPool& operator=(const DbThreadPool&) = delete;
public:
	static DbThreadPool& Inst()
	{
		static DbThreadPool inst(3);
		return inst;
	}
};
