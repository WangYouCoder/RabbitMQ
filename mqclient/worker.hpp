#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/util.hpp"
#include "../mqcommon/threadpool.hpp"
#include "muduo/net/EventLoopThread.h"

class AsynWorker
{
public:
    using ptr = std::shared_ptr<AsynWorker>;
    muduo::net::EventLoopThread loopthread;
    ThreadPool pool;
};