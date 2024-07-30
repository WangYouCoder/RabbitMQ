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

    Consumer(){}
    Consumer(const std::string &ctag, const std::string &queue_name, bool ack, const ConsumerCallback& cb)
    : tag(ctag)
    , qname(queue_name)
    ,auto_ack(ack)
    ,callback(cb)
    {
        DLOG("new Consumer: %p", this);
    }
};

class QueueConsume
{
public:
    using ptr = std::shared_ptr<QueueConsume>;
    QueueConsume(const std::string& qname) : _qname(qname), _rr_seq(0) {}
    Consumer::ptr create(const std::string &ctag, const std::string &queue_name, bool ack, const ConsumerCallback& cb)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        for(auto &consumer : _consumer)
        {
            if(consumer->tag == ctag)
                return Consumer::ptr();
        }
        auto consumer = std::make_shared<Consumer>(ctag, queue_name, ack, cb);
        _consumer.push_back(consumer);
        return consumer;
    }
    void remove(const std::string &ctag)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        for(auto it = _consumer.begin(); it != _consumer.end(); ++it)
        {
            if((*it)->tag == ctag)
            {
                _consumer.erase(it);
                return;
            }
        }
    }

    // 队列获取消费者: 采用RR轮转的方式获取
    Consumer::ptr choose()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if(_consumer.size() == 0)
            return Consumer::ptr();
        uint64_t idx = _rr_seq % _consumer.size();
        _rr_seq++;
        return _consumer[idx];
    }
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _consumer.size() == 0;
    }

    bool exists(const std::string &ctag)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        for(auto it = _consumer.begin(); it != _consumer.end(); ++it)
        {
            // DLOG("%s",(*it)->tag.c_str());
            if(((*it)->tag) == ctag)
            {
                return true;
            }
        }
        // DLOG("没找到 %s ", ctag.c_str());

        return false;
    }
    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _consumer.clear();
        _rr_seq = 0;
    }

    size_t size()
    {
        return _consumer.size();
    }
private:
    std::string _qname;  // 队列名称
    std::mutex _mutex;   // 锁
    uint64_t _rr_seq;    // 轮转消费者
    std::vector<Consumer::ptr> _consumer;  // 管理所有的消费者
};

class ConsumerManeger
{
public:
    using ptr = std::shared_ptr<ConsumerManeger>;
    ConsumerManeger() {}
    void initQueueConsumer(const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _qconsumers.find(qname);
        if(it != _qconsumers.end())
            return;
        auto qconsumer = std::make_shared<QueueConsume>(qname);
        _qconsumers.insert(std::make_pair(qname, qconsumer));
    }
    void destroyQueueConsumer(const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _qconsumers.find(qname);
        if(it == _qconsumers.end())
            return;
        _qconsumers.erase(qname);
    }

    Consumer::ptr create(const std::string &ctag, const std::string &queue_name, bool ack, const ConsumerCallback& cb)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return Consumer::ptr();
            }
            qconsumer = it->second;
        }
        
        return qconsumer->create(ctag, queue_name, ack, cb);
    }

    void remove(const std::string &ctag, const std::string &queue_name)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return ;
            }
            qconsumer = it->second;
        }
        
        return qconsumer->remove(ctag);
    }

    Consumer::ptr choose(const std::string &queue_name)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return Consumer::ptr();
            }
            qconsumer = it->second;
        }
        
        return qconsumer->choose();
    }
    bool empty(const std::string &queue_name)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return false;
            }
            qconsumer = it->second;
        }
        
        return qconsumer->empty();
    }
    bool exists(const std::string &ctag, const std::string &queue_name)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return false;
            }
            qconsumer = it->second;
        }
        
        return qconsumer->exists(ctag);
    }
    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _qconsumers.clear();
    }

    size_t size(const std::string &queue_name)
    {
        QueueConsume::ptr qconsumer;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(queue_name);
            if(it == _qconsumers.end())
            {
                ELOG("没有找到队列 %s 的消费者管理句柄", queue_name.c_str());
                return false;
            }
            qconsumer = it->second;
        }
        return qconsumer->size();
    }
private:
    std::mutex _mutex;
    std::unordered_map<std::string/*队列名称*/, QueueConsume::ptr> _qconsumers;  // 会存在多个队列，因此需要将所有队列的消费者管理起来
};