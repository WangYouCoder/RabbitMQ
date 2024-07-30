#include "channel.hpp"

class Connection
{
public:
    using ptr = std::shared_ptr<Connection>;
    Connection(const VirtualHost::ptr& host, const ConsumerManeger::ptr& cmp, 
        const ProtobufCodecPtr& codec, const muduo::net::TcpConnectionPtr& conn, const ThreadPool::ptr &pool)
        : _host(host)
        , _codec(codec)
        , _cmp(cmp)
        , _conn(conn)
        , _threadpool(pool)
        , _channels(std::make_shared<ChannelManeger>())
    {}
    void openChannel(const openChannelRequestPtr &req)
    {
        bool ret = _channels->openChannel(req->cid(), _host, _cmp, _codec, _conn, _threadpool);
        DLOG("信道创建成功");
        if(ret == false)
        {
            return basicResponse(false, req->rid(), req->cid());
        }
        return basicResponse(true, req->rid(), req->cid());
    }
    void closeChannel(const closeChannelRequestPtr &req)
    {
        _channels->closeChannel(req->cid());
        basicResponse(true, req->rid(), req->cid());
    }
    Channel::ptr getChannel(const std::string &cid)
    {
        return _channels->getChannel(cid);
    }


private:
    void basicResponse(bool ok, const std::string &rid/*请求id*/, const std::string &cid/*信道id*/)
    {
        basicCommonResponse resp;
        resp.set_rid(rid);
        resp.set_cid(cid);
        resp.set_ok(ok);
        _codec->send(_conn, resp);
    }
private:
    muduo::net::TcpConnectionPtr _conn;  // 信道所对应的连接(一个连接包含多个信道)，⽤于向客⼾端发送数据（响应，推送的消息）
    ProtobufCodecPtr _codec;  // 用于解析网络数据，序列化和反序列化
    ConsumerManeger::ptr _cmp;  // 消费者句柄，信道关闭/取消订阅的时候，通过句柄删除订阅者信息
    VirtualHost::ptr _host;  // 交换机/队列/绑定/消息数据管理
    ThreadPool::ptr _threadpool;  // ⼯作线程池句柄（⼀条消息被发布到队列后，需要将消息推送给订阅了对应队列的消费者，过程由线程池完成）

    ChannelManeger::ptr _channels;
}; 


class ConnectionManager
{
public:
    using ptr = std::shared_ptr<ConnectionManager>;
    ConnectionManager()
    {}
    ~ConnectionManager()
    {}

    void newConnection(const VirtualHost::ptr& host, const ConsumerManeger::ptr& cmp, 
        const ProtobufCodecPtr& codec, const muduo::net::TcpConnectionPtr& conn, const ThreadPool::ptr &pool)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _conns.find(conn);
        if(it != _conns.end())
        {
            return;
        }
        Connection::ptr connection = std::make_shared<Connection>(host, cmp, codec, conn, pool);
        _conns.insert(std::make_pair(conn, connection));
    }
    void delConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _conns.erase(conn);
    }
    Connection::ptr getConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _conns.find(conn);
        if(it == _conns.end())
        {
            return Connection::ptr();
        }
        return it->second;
    }

private:
    std::mutex _mutex;
    std::unordered_map<muduo::net::TcpConnectionPtr, Connection::ptr> _conns;
};