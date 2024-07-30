#pragma once
#include "muduo/net/TcpConnection.h"
#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include "../mqcommon/proto.pb.h"
#include "../mqcommon/threadpool.hpp"
#include "consume.hpp"
#include "host.hpp"
#include "route.hpp"
#include <iostream>
#include <string>
using namespace WY;

using ProtobufCodecPtr = std::shared_ptr<ProtobufCodec>;

using openChannelRequestPtr = std::shared_ptr<openChannelRequest>;
using closeChannelRequestPtr = std::shared_ptr<closeChannelRequest>;

using declareExchangeRequestPtr = std::shared_ptr<declareExchangeRequest>;
using deleteExchangeRequestPtr = std::shared_ptr<deleteExchangeRequest>;

using declareQueueRequestPtr = std::shared_ptr<declareQueueRequest>;    
using deleteQueueRequestPtr = std::shared_ptr<deleteQueueRequest>;

using queueBindRequestPtr = std::shared_ptr<queueBindRequest>;
using queueUnBindRequestPtr = std::shared_ptr<queueUnBindRequest>;

using basicPublishRequestPtr = std::shared_ptr<basicPublishRequest>;
using basicAckRequestPtr = std::shared_ptr<basicAckRequest>;

using basicConsumeRequestPtr = std::shared_ptr<basicConsumeRequest>;
using basicCancelRequestPtr = std::shared_ptr<basicCancelRequest>;
class Channel
{
public:
    using ptr = std::shared_ptr<Channel>;
    Channel(const std::string cid, const VirtualHost::ptr& host, const ConsumerManeger::ptr& cmp, 
        const ProtobufCodecPtr& codec, const muduo::net::TcpConnectionPtr& conn, const ThreadPool::ptr &pool)
        : _cid(cid)
        , _host(host)
        , _cmp(cmp)
        , _codec(codec)
        , _conn(conn)
        , _threadpool(pool)
    {
        DLOG("new Channel: %p", this);
    }
    ~Channel()
    {
        if(_consumer.get() != nullptr)
        {
            _cmp->remove(_consumer->tag, _consumer->qname);
        }
    }


    void declareExchange(const declareExchangeRequestPtr &req)
    {
        bool ret = _host->declareExchange(req->exchange_name(), req->exchange_type(), req->durable(), req->auto_delete(), req->args());
        basicResponse(ret, req->rid(), req->cid());
    }
    void deleteExchange(const deleteExchangeRequestPtr &req)
    {
        _host->deleteExchange(req->exchange_name());
        basicResponse(true, req->rid(), req->cid());
    }

    void declareQueue(const declareQueueRequestPtr &req)
    {
        bool ret = _host->declareQueue(req->queue_name(), req->durable(), req->exclusive(), req->auto_delete(), req->args());
        if(ret == false)
        {   
            basicResponse(false, req->rid(), req->cid());
            return;
        } 
        _cmp->initQueueConsumer(req->queue_name());
        basicResponse(true, req->rid(), req->cid());
    }
    void deleteQueue(const deleteQueueRequestPtr &req)
    {
        _cmp->destroyQueueConsumer(req->queue_name());
        _host->deleteQueue(req->queue_name());
        basicResponse(true, req->rid(), req->cid());
    }

    void bind(const queueBindRequestPtr &req)
    {
        bool ret = _host->bind(req->exchange_name(), req->queue_name(), req->binding_key());
        basicResponse(ret, req->rid(), req->cid());
    }
    void unbind(const queueUnBindRequestPtr &req)
    {
        _host->unbind(req->exchange_name(), req->queue_name());
        basicResponse(true, req->rid(), req->cid());
    }

    void basicPublish(const basicPublishRequestPtr &req)
    {
        // 1. 判断交换机是否存在
        bool ret = _host->existsExchange(req->exchange_name());
        if(ret == false)
        {
            basicResponse(false, req->rid(), req->cid());
            return;
        }
        // 2. 进行路由，判断消息可以发布到交换机所绑定的哪一个队列中
        Exchange::ptr ep = _host->selectExchange(req->exchange_name());
        MagQueueBindingMap mqbp = _host->exchangeBindings(req->exchange_name());

        std::string routing_key;
        BasicProperties *properties = nullptr;
        if(req->has_properties() == true)
        {
            properties = req->mutable_properties();
            routing_key = properties->routing_key();
        }

        for(auto &bp : mqbp)
        {
            if(Route::route(ep->type, routing_key, bp.second->binding_key))
            {
                // 3. 将消息添加到队列中
                _host->basicPublish(bp.first, properties, req->body());
                // 4. 向线程池中添加一个消息消费任务(向指定队列的订阅者去推送消息)
                auto task = std::bind(&Channel::consumer, this, bp.first);
                _threadpool->Push(task);
            }
        }
        basicResponse(true, req->rid(), req->cid());
    }
    void basicAck (const basicAckRequestPtr &req)
    {
        _host->basicAck(req->queue_name(), req->message_id());
        basicResponse(true, req->rid(), req->cid());
    }

    void basicConsume(const basicConsumeRequestPtr &req)
    {
        bool ret = _host->existsQueue(req->queue_name());
        if(ret == false)
        {
            basicResponse(false, req->rid(), req->cid());
            return;
        }

        auto cb = std::bind(&Channel::callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

        // 创建了消费者后，当前的channel就是一个消费者了
        _consumer = _cmp->create(req->consumer_tag(), req->queue_name(), req->auto_ack(), cb);
        basicResponse(true, req->rid(), req->cid());
    }
    void basicCancel(const basicCancelRequestPtr &req)
    {
        _cmp->remove(req->consumer_tag(), req->queue_name());
        basicResponse(true, req->rid(), req->cid());
    }


private:
    void callback(const std::string &tag, const BasicProperties* bp, const std::string &body)
    {
        basicConsumeResponse resp; 
        resp.set_cid(_cid);
        resp.set_body(body);
        resp.set_consumer_tag(tag);
        if(bp)
        {
            resp.mutable_properties()->set_id(bp->id());
            resp.mutable_properties()->set_delivery_mode(bp->delivery_mode());
            resp.mutable_properties()->set_routing_key(bp->routing_key());
        }
        _codec->send(_conn, resp);
    }
    void consumer(const std::string &qname)
    {
        // 1. 从队列中获取一条消息
        mq::MessagePtr mp = _host->basicConsume(qname);
        if(mp.get() == nullptr)
        {
            ELOG("执行消费任务失败，%s 队列没有消息", qname.c_str());
        }
        // 2. 从队列订阅者中取出一个订阅者
        Consumer::ptr cp = _cmp->choose(qname);
        if(cp.get() == nullptr)
        {
            ELOG("执行消费任务失败，%s 队列没有消费者", qname.c_str());
            return;
        }
        // 3. 调用订阅者对应的消息处理函数，实现消息推送
        cp->callback(cp->tag, mp->mutable_paylaod()->mutable_properties(), mp->paylaod().body());
        // 4. 判断订阅者是否是自动确认，如果是的话，推送完消息直接删除，否则需要等待外部收到消息确认后再删除
        if(cp->auto_ack == true)
        {
            _host->basicAck(qname, mp->paylaod().properties().id());
        }
    }
    void basicResponse(bool ok, const std::string &rid, const std::string &cid)
    {
        basicCommonResponse resp;
        resp.set_rid(rid);
        resp.set_cid(cid);
        resp.set_ok(ok);
        _codec->send(_conn, resp);
    }
private:
    std::string _cid;  // 信道id
    Consumer::ptr _consumer;  // 信道所对应的消费者(一个信道对应一个消费者)，⽤于消费者信道在关闭的时候取消订阅，删除订阅者信息
    muduo::net::TcpConnectionPtr _conn;  // 信道所对应的连接(一个连接包含多个信道)，⽤于向客⼾端发送数据（响应，推送的消息）
    ProtobufCodecPtr _codec;  // 用于解析网络数据，序列化和反序列化
    ConsumerManeger::ptr _cmp;  // 消费者句柄，信道关闭/取消订阅的时候，通过句柄删除订阅者信息
    VirtualHost::ptr _host;  // 交换机/队列/绑定/消息数据管理
    ThreadPool::ptr _threadpool;  // ⼯作线程池句柄（⼀条消息被发布到队列后，需要将消息推送给订阅了对应队列的消费者，过程由线程池完成）
};

// 推送消息的流程:
// 1. 先调用basicPublish进行消息推送: 其内部会选择交换机，进行路由，发送到相匹配的队列上，然后将 "推送任务" 放到线程池中
// 2. 线程池: 只要任务池中有任务，就会进行处理(内部是while死循环)
// 3. 推送任务consumer: 从队列中取出一条消息，再取出一个消费者，调用消费者内部中对应的 "消息处理回调函数" ，并判断是否需要自动删除
// 4. 消息处理回调函数: 将消息做成一个回应，发送给 "对应连接"

// 5. 调用basicConsume进行消息消费: 会先判断队列是否存在，然后会创建一个消费者，并将自己的this指针绑定到内部的 "消息处理回调函数" 当中
//                                 创建消费者时，会将它添加到指定的队列所管理的消费者的vector当中



class ChannelManeger
{
public:
    using ptr = std::shared_ptr<ChannelManeger>;
    ChannelManeger() {}
    bool openChannel(const std::string &id, const VirtualHost::ptr& host, const ConsumerManeger::ptr& cmp, 
        const ProtobufCodecPtr& codec, const muduo::net::TcpConnectionPtr& conn, const ThreadPool::ptr &pool)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _channels.find(id);
        if(it != _channels.end())
        {
            return false;
        }
        auto channel = std::make_shared<Channel>(id, host,cmp, codec,conn, pool);
        _channels.insert(std::make_pair(id, channel));
        return true;
    }
    void closeChannel(const std::string id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _channels.erase(id);
    }
    Channel::ptr getChannel(const std::string id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _channels.find(id);
        if(it == _channels.end())
        {
            return Channel::ptr();
        }
        return it->second;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string/*信道id*/, Channel::ptr> _channels;
};