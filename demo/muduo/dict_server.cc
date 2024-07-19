#include "include/muduo/net/TcpServer.h"
#include "include/muduo/net/EventLoop.h"
#include "include/muduo/net/TcpConnection.h"
#include <iostream>
#include <unordered_map>

class TranslateServer
{
public:
    TranslateServer(int port) : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "Translate", muduo::net::TcpServer::kReusePort)
    {}
    void start()
    {
        _server.start();  // 开始监听连接事件
        _baseloop.loop(); // 开始事件监控，这是一个死循环阻塞接口
    
        auto func1 = std::bind(&TranslateServer::onConnection, this, std::placeholders::_1);
        _server.setConnectionCallback(func1);
        auto func2 = std::bind(&TranslateServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _server.setMessageCallback(func2);
    }
private:
    // onConnection应该是连接成功或者关闭的时候进行调用
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if(conn->connected() == true)
        {
            std::cout << "新连接建立成功\n";
        }
        else
        {
            std::cout << "关闭连接\n";
        }
    }
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        std::unordered_map<std::string, std::string> dict = {
            {"你好","hello"},
            {"apple","苹果"},
            {"goods", "商品"}
        };
        auto it = dict.find(buf->retrieveAllAsString());
        if(it == dict.end())
        {
            std::cout << "没找到\n";
        }
        conn->send(it->second);
    }
private:
    // _baseloop是epoll的事件监控，会进行描述符的监控，触发事件后的进行IO操作
    muduo::net::EventLoop _baseloop;
    // 这个server对象的功能: 通过这个对象来设置回调函数，告诉服务器收到请求时该如何进行操作
    muduo::net::TcpServer _server;
};

int main()
{
    TranslateServer server(8888);
    server.start();
    return 0;
}