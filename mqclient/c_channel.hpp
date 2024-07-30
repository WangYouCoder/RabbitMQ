#pragma once
#include "muduo/net/TcpConnection.h"
#include "muduo/proto/codec.h"
#include "muduo/proto/dispatcher.h"
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include "../mqcommon/proto.pb.h"
#include "../mqcommon/threadpool.hpp"
#include "c_consumer.hpp"
#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
using namespace WY;

using ProtobufCodecPtr = std::shared_ptr<ProtobufCodec>;
using basicConsumeResponsePtr = std::shared_ptr<basicConsumeResponse>;
using basicCommonResponsePtr = std::shared_ptr<basicCommonResponse>;
typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

class Channel
{
public:
    using ptr = std::shared_ptr<Channel>;
    Channel(const muduo::net::TcpConnectionPtr &conn, const ProtobufCodecPtr &codec)
        : _cid(Util::uuid())
        , _conn(conn)
        , _codec(codec)
    {}
    ~Channel()
    {
        basicCancel();
    }

    std::string cid()
    {
        return _cid;
    }

    bool openChannel()
    {
        openChannelRequest req;
        std::string rid = Util::uuid();
        req.set_rid(rid);
        req.set_cid(_cid);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
        return resp->ok();
    }

    void closeChannel()
    {
        closeChannelRequest req;
        std::string rid = Util::uuid();
        req.set_rid(rid);
        req.set_cid(_cid);
        _codec->send(_conn, req);
        waitResponse(rid);
    }

    bool declareExhcange(const std::string &name, ExchangeType type, bool durable, bool auto_delete, 
        google::protobuf::Map<std::string, std::string> &args)
    {
        // 构造一个声明虚拟机的请求对象
        std::string rid = Util::uuid();
        declareExchangeRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_exchange_name(name);
        req.set_exchange_type(type);
        req.set_durable(durable);
        req.set_auto_delete(auto_delete);
        req.mutable_args()->swap(args);
        // 向服务器发送请求
        _codec->send(_conn, req);   // 其中是异步完成的，也就是muduo内部实际上是自己封装了发送和接收缓冲区，send目前只是发送到了muduo中自己封装的缓冲区中
        // 等待服务器响应
        basicCommonResponsePtr resp = waitResponse(rid);
        // 返回
        return resp->ok();
    }

    void deleteExchange(const std::string &name)
    {
        std::string rid = Util::uuid();
        deleteExchangeRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_exchange_name(name);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
    }

    bool declareQueue(const std::string &qname, bool qdurable, bool qexclusive, bool qauto_delete,
        google::protobuf::Map<std::string, std::string> &qargs)
    {
        std::string rid = Util::uuid();
        declareQueueRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_queue_name(qname);
        req.set_durable(qdurable);
        req.set_exclusive(qexclusive);
        req.set_auto_delete(qauto_delete);
        req.mutable_args()->swap(qargs);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
        return resp->ok();

    }
    void deleteQueue(const std::string &qname)
    {
        std::string rid = Util::uuid();
        deleteQueueRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_queue_name(qname);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
    }

    bool queueBind(const std::string &ename, const std::string &qname, const std::string &key)
    {
        std::string rid = Util::uuid();
        queueBindRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_queue_name(qname);
        req.set_exchange_name(ename);
        req.set_binding_key(key);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
        return resp->ok();
    }
    void qeueuUnBind(const std::string &ename, const std::string &qname)
    {
        std::string rid = Util::uuid();
        queueUnBindRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_queue_name(qname);
        req.set_exchange_name(ename);
        _codec->send(_conn, req);
        waitResponse(rid);
    }

    void basicPulish(const std::string &ename, BasicProperties *bp, const std::string &body)
    {
        std::string rid = Util::uuid();
        basicPublishRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_exchange_name(ename);
        req.set_body(body);
        if(bp)
        {
            req.mutable_properties()->set_id(bp->id());
            req.mutable_properties()->set_delivery_mode(bp->delivery_mode());
            req.mutable_properties()->set_routing_key(bp->routing_key());
        }
        
        _codec->send(_conn, req);
        waitResponse(rid);
    }

    void basicAck(const std::string msg_id)
    {
        if(_consumer.get() == nullptr)
        {
            DLOG("消息确认时，找不到消费者信息");
            return;
        }
        std::string rid = Util::uuid();
        basicAckRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_queue_name(_consumer->qname);
        req.set_message_id(msg_id);
        _codec->send(_conn, req);
        waitResponse(rid);
        return;
        //_consumer.reset();
    }

    bool basicConsume(const std::string &consumer_tag, const std::string &qname, bool auto_ack, ConsumerCallback cb)
    {
        if(_consumer.get() != nullptr)
        {
            DLOG("当前信道已订阅其他队列消息")
        }
        std::string rid = Util::uuid();
        basicConsumeRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_consumer_tag(consumer_tag);
        req.set_queue_name(qname);
        req.set_auto_ack(auto_ack);
        _codec->send(_conn, req);
        basicCommonResponsePtr resp = waitResponse(rid);
        if(resp->ok() == false)
        {
            DLOG("订阅失败");
            return false;
        }

        _consumer = std::make_shared<Consumer>(consumer_tag, qname, auto_ack, cb);
        return true;
    }
    void basicCancel()
    {
        if(_consumer.get() == nullptr);
        {
            return;
        }
        std::string rid = Util::uuid();
        basicCancelRequest req;
        req.set_rid(rid);
        req.set_cid(_cid);
        req.set_consumer_tag(_consumer->tag);
        req.set_queue_name(_consumer->qname);
        _codec->send(_conn, req);
        waitResponse(rid);
    }
    // 收到响应后，添加到_basic_resp
    void putBasicResponse(const basicCommonResponsePtr &resp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _basic_resp.insert(std::make_pair(resp->rid(), resp));
        _cv.notify_all(); // 唤醒所有的阻塞
    }

    // 连接接到消息后，需要通过信道找到对应的消费者，通过调用回调函数进行消息处理
    void consume(const basicConsumeResponsePtr &resp)
    {
        if(_consumer.get() == nullptr);
        {
            DLOG("消息处理是，未找到订阅者信息");
            return;
        }
        if(_consumer->tag != resp->consumer_tag())
        {
            DLOG("收到的推送消息的消费者标识，与当前信道消费者不相符");
            return;
        }

        _consumer->callback(resp->consumer_tag(), resp->mutable_properties(), resp->body());
    }
private:
    basicCommonResponsePtr waitResponse(const std::string &rid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&rid, this](){
            return _basic_resp.find(rid) != _basic_resp.end();
        });

        basicCommonResponsePtr basic_resp = _basic_resp[rid];
        _basic_resp.erase(rid);
        return  basic_resp;
    }
private:
    std::string _cid;
    muduo::net::TcpConnectionPtr _conn;
    ProtobufCodecPtr _codec;
    Consumer::ptr _consumer;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::unordered_map<std::string, basicCommonResponsePtr> _basic_resp;
};


class ChannelManager
{
public:
    using ptr = std::shared_ptr<ChannelManager>;
    ChannelManager(){}

    Channel::ptr create(const muduo::net::TcpConnectionPtr &conn, const ProtobufCodecPtr &codec)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        Channel::ptr channel = std::make_shared<Channel>(conn, codec);
        _channels.insert(std::make_pair(channel->cid(), channel));
        return channel;
    }
    void remove(const std::string &cid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _channels.erase(cid);
    }
    Channel::ptr get(const std::string &cid)
    {
        auto it = _channels.find(cid);
        if(it == _channels.end())
        {
            DLOG("没找到对应的信道");
            return Channel::ptr();
        }
        return it->second;
    }
private:
    std::mutex _mutex;
    std::unordered_map<std::string, Channel::ptr> _channels;
};