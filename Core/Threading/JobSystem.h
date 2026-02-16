#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace Threading {

struct Job {
  std::function<void()> task;
  int priority = 0;
};

class JobSystem {
public:
  JobSystem() : stop(false), activeJobs(0) {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
      numThreads = 2;
    for (unsigned int i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queueMutex);
            this->condition.wait(
                lock, [this] { return this->stop || !this->jobs.empty(); });
            if (this->stop && this->jobs.empty())
              return;
            task = std::move(this->jobs.front());
            this->jobs.pop();
          }
          task();
          {
            std::unique_lock<std::mutex> lock(completionMutex);
            activeJobs--;
            if (activeJobs == 0) {
              completionCV.notify_all();
            }
          }
        }
      });
    }
  }

  ~JobSystem() {
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
      worker.join();
    }
  }

  template <class F, class... Args>
  auto PushJob(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
    using return_type = decltype(f(args...));

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      if (stop)
        throw std::runtime_error("PushJob on stopped JobSystem");

      {
        std::unique_lock<std::mutex> cLock(completionMutex);
        activeJobs++;
      }
      jobs.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
  }

  bool Busy() {
    std::unique_lock<std::mutex> lock(completionMutex);
    return activeJobs > 0;
  }

  void WaitAll() {
    std::unique_lock<std::mutex> lock(completionMutex);
    completionCV.wait(lock, [this] { return activeJobs == 0; });
  }

  unsigned int ThreadCount() const {
    return static_cast<unsigned int>(workers.size());
  }

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> jobs;

  std::mutex queueMutex;
  std::condition_variable condition;
  std::atomic<bool> stop;

  // Completion tracking (fixes race condition)
  std::mutex completionMutex;
  std::condition_variable completionCV;
  int activeJobs;
};

} // namespace Threading
} // namespace Core
} // namespace Mesozoic
