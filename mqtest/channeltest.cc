#include "../mqserver/channel.hpp"

int main()
{
    ChannelManeger::ptr cmp = std::make_shared<ChannelManeger>();

    cmp->openChannel("c1",
        std::make_shared<VirtualHost>("host1", "./data/host1/message/", "./data/host1/message/host1.db"),
        std::make_shared<ConsumerManeger>(),
        ProtobufCodecPtr(),
        muduo::net::TcpConnectionPtr(),
        ThreadPool::ptr()
    );

    return 0;
}