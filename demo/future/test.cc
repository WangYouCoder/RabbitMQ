#include <iostream>
#include <thread>
#include <future>
#include <chrono>
int Add(int num1, int num2)
{
    std::cout << "加法\n";
    return num1 + num2;
}

// int main()
// {
//     // std::launch::deferred: 在执行get获取异步结果的时候，才会执行异步任务
//     auto result = std::async(std::launch::deferred, Add, 10, 20);
//     result.get();
//     return 0;
// }

// int main()
// {
//     auto ptask = std::make_shared<std::packaged_task<int(int,int)>>(Add);
//     std::future<int> fu = ptask->get_future();
//     std::thread thr([ptask](){
//         (*ptask)(10,20);
//     });

//     int sum = fu.get();
//     thr.join();
//     return 0;
// }

// void task(std::promise<int> result_promise)
// {
//     result_promise.set_value(10 + 20);
// }

// int main()
// {
//     std::promise<int> result_promise;
//     std::future<int> fu = result_promise.get_future();
//     std::thread thr(task, std::ref(result_promise));

//     int num = fu.get();

//     thr.join();
//     return 0;
// }

#include "threadpool.hpp"

int main()
{
    ThreadPool pool(5);
    std::future<int> fu = pool.Push(Add, 1 , 1);
    std::cout << fu.get() << std::endl;

    pool.stop();
    return 0;
}