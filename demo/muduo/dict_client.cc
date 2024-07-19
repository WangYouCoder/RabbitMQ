#include "include/muduo/net/TcpClient.h"
#include "include/muduo/net/EventLoopThread.h"
#include "include/muduo/net/TcpConnection.h"
#include "include/muduo/base/CountDownLatch.h"
#include <iostream>
#include <string>
class TranslateClient
{
public:
    TranslateClient(const std::string &ip, int port) : _latch(1), _client(_loopthread.startLoop(), muduo::net::InetAddress(ip, port), "Translateclient")
    {
        _client.setConnectionCallback(std::bind(&TranslateClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(std::bind(&TranslateClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    void connect()
    {
        _client.connect();
        _latch.wait();    // 阻塞等待，直到连接成功
    }
    bool send(std::string &msg)
    {
        if(_conn->connected() == true)
        {
            _conn->send(msg);
            return true;
        }
        return false;
    }
private:
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if(conn->connected() == true)
        {
            _latch.countDown();
            _conn = conn;
        }
        else
        {
            _conn->connectDestroyed();
        }
    }
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        std::cout << "翻译结果" << buf->retrieveAllAsString() << std::endl;
    }
private:
    muduo::CountDownLatch _latch;
    muduo::net::EventLoopThread _loopthread;
    muduo::net::TcpClient _client;
    muduo::net::TcpConnectionPtr _conn;
};

int main()
{
    TranslateClient client("127.0.0.1", 8888);
    client.connect();
    while(1)
    {
        std::string buf;
        std::cin >> buf;
        client.send(buf);
    }
    
    return 0;
}