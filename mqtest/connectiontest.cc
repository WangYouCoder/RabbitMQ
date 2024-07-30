#include "../mqserver/connection.hpp"

int main()
{
    ConnectionManager::ptr cmp = std::make_shared<ConnectionManager>();
    cmp->newConnection(
        std::make_shared<VirtualHost>("host1", "./data/host1/message/", "./data/host1/message/host1.db"),
        std::make_shared<ConsumerManeger>(),
        ProtobufCodecPtr(),
        muduo::net::TcpConnectionPtr(),
        ThreadPool::ptr()
    );

    return 0;
}