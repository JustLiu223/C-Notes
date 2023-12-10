//c++线程池，加入了单例模式，异步执行，原子操作等

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <future>

class ThreadPool {
public:
	static ThreadPool& getInstance(size_t numThreads=2) {
		static ThreadPool instance(numThreads);
		return instance;
	}
	template<class F,class ... Args>
	auto enqueue(F&& f,Args&& ...args) ->std::future<typename std::invoke_result_t<F, Args...>> {
		using return_type = typename std::invoke_result_t<F, Args...>;
		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex>lock(mtx);
			if (stop) {
				throw std::runtime_error("enqueue on stopeed ThreadPool");
			}
			tasksQueue.emplace([task]() {(*task)(); });
		}
		condition.notify_one();
		return res;
	}
	
private:
	ThreadPool(size_t numThreads) :stop(false) {
		for (int i = 0; i < numThreads; i++) {
			threads.emplace_back([this]{
				while (true)
				{
					std::function<void()>task;
					{
						std::unique_lock<std::mutex>lock(mtx);
						condition.wait(lock, [this] {
							return stop || !tasksQueue.empty();
						});
						if (stop && tasksQueue.empty()) return;
						task = std::move(tasksQueue.front());
						tasksQueue.pop();
					}
					task();
				}
			});
		}
	}

	~ThreadPool() {
		{
			std::unique_lock<std::mutex>lock(mtx);
			stop = true;
		}
		condition.notify_all();
		for (auto& t : threads) {
			t.join();
		}
	}
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	std::vector<std::thread> threads;
	std::queue<std::function<void()>> tasksQueue;
	std::condition_variable condition;
	std::mutex mtx;
	std::atomic_bool stop;
};

void exampleTask(int i) {
	std::cout << "task: " << i << "is running on thread" << std::this_thread::get_id() << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::cout << "task: " << i << "is done on thread" << std::this_thread::get_id() << std::endl;
}

int main() {

	ThreadPool* pool = &ThreadPool::getInstance(4);
	for (int i = 0; i < 10; i++) {
		pool->enqueue(exampleTask,i);
	}
	std::this_thread::sleep_for(std::chrono::seconds(5));
	return 0;
}