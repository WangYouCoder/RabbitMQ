#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
using namespace WY;
using ConsumerCallback = std::function<void(const std::string, const BasicProperties* bp, const std::string)>;

class Consumer
{
public:
    using ptr = std::shared_ptr<Consumer>;
    std::string tag;   // 消费者标识: 可以理解为名字
    std::string qname; // 消费者所订阅的队列
    bool auto_ack;     // 在消费消息时，是否需要从队列中自动删除消息
    ConsumerCallback callback;  // 处理消息的回调函数

    Consumer()
    {
        DLOG("new Consumer: %p", this);
    }
    Consumer(const std::string &ctag, const std::string &queue_name, bool ack, ConsumerCallback& cb)
    : tag(ctag)
    , qname(queue_name)
    ,auto_ack(ack)
    ,callback(cb)
    {
        DLOG("new Consumer: %p", this);
    }
};