#pragma once
#include <future>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <type_traits>

class ThreadPool
{
	std::vector<std::thread> _threads{};
	std::queue<std::function<void()>> _tasks{};
	std::mutex _mutex;
	std::condition_variable _cond;
	bool _isDead = false;
public:
	static ThreadPool& Inst() { static ThreadPool inst(3); return inst; }
	ThreadPool(size_t threadMax)
	{
		for (int i = 0; i < threadMax; i++)
		{
			_threads.emplace_back(std::thread([this]()
				{
					while (true)
					{
						std::function<void()> task;
						{
							std::unique_lock<std::mutex> lock(_mutex);
							_cond.wait(lock, [this]() { return _isDead || !_tasks.empty(); });
							if (_isDead && _tasks.empty()) return;
							task = std::move(_tasks.front());
							_tasks.pop();
						}
						task();
					}
				}));
		}
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_isDead = true;
		}
		_cond.notify_all();
		for (auto& t : _threads) t.join();
	}

	template<class F, class ...Args>
	std::future<typename std::invoke_result_t<F, Args...>> EnqueueTask(
		F&& f, Args&&... args)
	{
		using RetType = typename std::invoke_result_t<F, Args...>;
		std::shared_ptr<std::packaged_task<RetType()>> task = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
		std::future<RetType> ret = task->get_future();
		{
			std::unique_lock<std::mutex> lock(_mutex);
			if (_isDead) assert(false && "ThreadPool::EnqueueTask error: this should never happen!");
			_tasks.emplace([task]() { (*task)(); });
		}
		_cond.notify_one();
		return ret;
	}
};

