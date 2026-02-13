#pragma once
#include <queue>
#include <vector>
#include <mutex>
#include <functional>
#include <future>
#include <chrono>
#include <type_traits>
#include <iostream>
#include <utility>
#include <memory>

class MainThreadDispatcher
{
	struct IWatcher
	{
		virtual ~IWatcher() = default;
		virtual bool Poll() = 0;
	};

	template <class T, class F>
	struct Watcher : IWatcher
	{
		std::future<T> fut;
		F cb;

		Watcher(std::future<T>&& f, F&& fn) : fut(std::move(f)), cb(std::forward<F>(fn)) {}

		bool Poll() override
		{
			using namespace std::chrono_literals;
			if (fut.wait_for(0ms) != std::future_status::ready)
				return false;
			try
			{
				if constexpr (std::is_void_v<T>)
				{
					fut.get();
					cb();
				}
				else
				{
					T value = fut.get();
					cb(std::move(value));
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "MainThreadDispatcher future exception: " << e.what() << std::endl;
			}
			catch (...)
			{
				std::cerr << "MainThreadDispatcher future exception: unknown" << std::endl;
			}
			return true;
		}
	};

	std::queue<std::function<void()>> _tasks{};
	std::vector<std::unique_ptr<IWatcher>> _watchers{};
	std::mutex _mutex;

	MainThreadDispatcher() = default;
	MainThreadDispatcher(const MainThreadDispatcher&) = delete;
	MainThreadDispatcher& operator=(const MainThreadDispatcher&) = delete;

public:
	static MainThreadDispatcher& Instance()
	{
		static MainThreadDispatcher inst;
		return inst;
	}

	static void Post(std::function<void()> task)
	{
		auto& inst = Instance();
		{
			std::lock_guard<std::mutex> lock(inst._mutex);
			inst._tasks.emplace(std::move(task));
		}
	}

	template <class T, class F>
	static void Watch(std::future<T>&& fut, F&& cb)
	{
		auto& inst = Instance();
		using Callback = std::decay_t<F>;
		auto watcher = std::make_unique<Watcher<T, Callback>>(std::move(fut), Callback(std::forward<F>(cb)));
		{
			std::lock_guard<std::mutex> lock(inst._mutex);
			inst._watchers.emplace_back(std::move(watcher));
		}
	}

	static void Drain()
	{
		auto& inst = Instance();
		std::queue<std::function<void()>> tasks;
		std::vector<std::unique_ptr<IWatcher>> watchers;
		{
			std::lock_guard<std::mutex> lock(inst._mutex);
			std::swap(tasks, inst._tasks);
			std::swap(watchers, inst._watchers);
		}

		while (!tasks.empty())
		{
			auto task = std::move(tasks.front());
			tasks.pop();
			try
			{
				if (task) task();
			}
			catch (const std::exception& e)
			{
				std::cerr << "MainThreadDispatcher task exception: " << e.what() << std::endl;
			}
			catch (...)
			{
				std::cerr << "MainThreadDispatcher task exception: unknown" << std::endl;
			}
		}

		std::vector<std::unique_ptr<IWatcher>> remaining;
		remaining.reserve(watchers.size());
		for (auto& watcher : watchers)
		{
			if (!watcher->Poll())
				remaining.emplace_back(std::move(watcher));
		}

		if (!remaining.empty())
		{
			std::lock_guard<std::mutex> lock(inst._mutex);
			for (auto& watcher : remaining)
				inst._watchers.emplace_back(std::move(watcher));
		}
	}
};
