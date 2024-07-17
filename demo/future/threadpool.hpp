#pragma once
#include <iostream>
#include <vector>
#include <condition_variable>
#include <future>
#include <thread>
#include <mutex>
#include <functional>

class ThreadPool
{
public:
    using func_t = std::function<void(void)>;
    ThreadPool(int count = 1) : _stop(false)
    {
        for(int i = 0; i < count; i++)
        {
            _threadpool.emplace_back(&ThreadPool::entry, this);
        }
    }

    template<typename F, typename ...Args>
    auto Push(F&& func, Args&& ...args) -> std::future<decltype(func(args...))>
    {
        using return_type = decltype(func(args...));
        auto f = std::bind(std::forward<F>(func), std::forward<Args>(args)...);   // 将各种各样的函数统一转化为f()  ---> f是函数名
        // 将函数用package_task打包，但是有可能会有局部变量生命周期的问题，因此再加上智能指针
        auto task = std::make_shared<std::packaged_task<return_type()>>(f);

        std::future<return_type> fu = task->get_future();
        // 构造一个lambda匿名函数
        {
            std::unique_lock<std::mutex> lock(_mutex);
            // 将构造的匿名函数对象放到任务池中
            _taskpool.push_back( [task](){ (*task)(); });
            _cv.notify_one();  // 解除当前等待此条件的一个线程的阻塞
        }

        return fu;
    }   

    void stop()
    {
        if(_stop == true) return;
        _stop = true;
        _cv.notify_all();  // 解除当前等待此条件的所有线程的阻塞
        for(auto &thread : _threadpool)
        {
            thread.join();
        }
    }

    ~ThreadPool()
    {
        stop();
    }
private:
    // 线程入口函数——内部不断的从任务池中取出任务进行执行
    void entry()
    {
        while(!_stop)
        {
            std::vector<func_t> taskpool;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                // 等待任务不为空或者_stop被置位返回
                _cv.wait(lock, [this](){  return _stop || !_taskpool.empty();  });
                _taskpool.swap(taskpool);
            }
            for(auto &task : taskpool)
            {
                task();
            }
        }
    }
private:
    std::mutex _mutex;
    std::atomic<bool> _stop;
    std::vector<func_t> _taskpool;
    std::vector<std::thread> _threadpool;
    std::condition_variable _cv;
};
