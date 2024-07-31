#pragma once
#include "muduo/proto/dispatcher.h"
#include "muduo/proto/codec.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/net/EventLoop.h"

#include "c_channel.hpp"
// #include "c_consumer.hpp"
#include "worker.hpp"

class Connection {
    public:
        using ptr = std::shared_ptr<Connection>;
        Connection(const std::string &sip, int sport, const AsynWorker::ptr &worker)
            : _latch(1)
            , _client(worker->loopthread.startLoop(), muduo::net::InetAddress(sip, sport), "Client")
            , _dispatcher(std::bind(&Connection::onUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
            , _codec(std::make_shared<ProtobufCodec>( std::bind(&ProtobufDispatcher::onProtobufMessage, &_dispatcher, 
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)))
            , _worker(worker)
            , _channel_manager(std::make_shared<ChannelManager>())
        {

                _dispatcher.registerMessageCallback<basicCommonResponse>(std::bind(&Connection::basicResponse, this, 
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                    
                _dispatcher.registerMessageCallback<basicConsumeResponse>(std::bind(&Connection::consumeResponse, this, 
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

                _client.setMessageCallback(std::bind(&ProtobufCodec::onMessage, _codec.get(),
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

                _client.setConnectionCallback(std::bind(&Connection::onConnection, this, std::placeholders::_1));      

                
                _client.connect();
                _latch.wait();     //阻塞等待，直到连接建立成功
        }
        // 创建一个信道，用于进行后续的一系列服务，如: 声明交换机等待
        Channel::ptr openChannel() 
        {
            // 通过信道管理模块创建一个信道服务，通过信道服务向服务端发起请求

            Channel::ptr channel = _channel_manager->create(_conn, _codec);
            // debug 
            if (_conn.get() == nullptr) {
                std::cout << "_conn is nullptr" << std::endl;
                abort();
            }
            
            // 向服务端发起建立信道的请求
            bool ret = channel->openChannel();
            if (ret == false) {
                DLOG("打开信道失败！");
                return Channel::ptr();
            }
            return channel;
        }
        
        // 关闭信道服务
        void closeChannel(const Channel::ptr &channel) 
        {
            channel->closeChannel();
            _channel_manager->remove(channel->cid());
        }
    private:
        // 已经在构造函数当中注册到了任务分发器中，当收到服务端发来的响应时，会自动调用该函数
        void basicResponse(const muduo::net::TcpConnectionPtr& conn, const basicCommonResponsePtr& message, muduo::Timestamp) 
        {
            // 在信道管理模块会管理所有的信道，当响应来临时，会先判断在客户端保存的信道信息中是否存在与响应信息相匹配的信道
            // 每一个信道都有独属于自己的信道id，通过这个条件进行查找
            // 当找到之后会添加到 管理该信道所有响应的map当中(用于给服务端发送确认响应，因为在服务端有的请求是需要收到客户端的确认响应后才会删除)

            //1. 找到信道
            Channel::ptr channel = _channel_manager->get(message->cid());
            if (channel.get() == nullptr) {
                DLOG("未找到信道信息！");
                return;
            }
            //2. 将得到的响应对象，添加到信道的基础响应hash_map中
            channel->putBasicResponse(message);
        }

        // 当服务端发来消费响应时，会自动调用该函数
        void consumeResponse(const muduo::net::TcpConnectionPtr& conn, const basicConsumeResponsePtr& message, muduo::Timestamp)
        {
            //1. 找到信道
            Channel::ptr channel = _channel_manager->get(message->cid());
            if (channel.get() == nullptr) {
                DLOG("未找到信道信息！");
                return;
            }
            //2. 封装异步任务（消息处理任务），抛入线程池
            _worker->pool.Push([channel, message](){
                channel->consume(message);
            });
        }

        void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn, const MessagePtr& message, muduo::Timestamp) 
        {
            LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
            conn->shutdown();
        }

        void onConnection(const muduo::net::TcpConnectionPtr&conn)
        {
            if (conn->connected()) {
                // printf("onConnection, conn: %p\n", conn.get());
                _conn = conn;
                _latch.countDown();//唤醒主线程中的阻塞
                // printf("onConnection, _conn: %p\n", _conn.get());

            }else {
                //连接关闭时的操作
                _conn.reset();
            }
        }
    private:
        muduo::CountDownLatch _latch;//实现同步的
        muduo::net::TcpConnectionPtr _conn;//客户端对应的连接
        muduo::net::TcpClient _client;//客户端
        ProtobufDispatcher _dispatcher;//请求分发器
        ProtobufCodecPtr _codec;//协议处理器

        AsynWorker::ptr _worker;
        ChannelManager::ptr _channel_manager;
};