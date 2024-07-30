#include "c_connection.hpp"

void cb(Channel::ptr &channel, const std::string &consumer_tag, const WY::BasicProperties *bp, const std::string &body)
{
    std::cout << consumer_tag << "消费了: " << body << std::endl;
    channel->basicAck(bp->id());
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        DLOG("Usage: ./consumer_client queue1");
        return -1;
    }
    // 1. 实例化异步工作线程池
    AsynWorker::ptr awp = std::make_shared<AsynWorker>();
    // 2. 实例化连接对象
    Connection::ptr conn = std::make_shared<Connection>("127.0.0.1", 8888, awp);
    // 3. 通过连接创建信道
    Channel::ptr channel = conn->openChannel();
    // 4. 通过信道提供的服务完成所需
    //    a.声明一个交换机exchange1, 交换机类型为广播模式
    google::protobuf::Map<std::string, std::string> tmp_args;
    channel->declareExhcange("exchange1", ExchangeType::FANOUT, true, false, tmp_args);
    //    b.声明一个队列queue1
    channel->declareQueue("queue1", true, false, false, tmp_args);
    //    c.声明一个队列queue2  
    channel->declareQueue("queue2", true, false, false, tmp_args);
    //    d.绑定queue1 - exchange1, binding_key设置为queue1
    channel->queueBind("exchange1", "queue1", "queue1");
    //    d.绑定queue2 - exchange1, binding_key设置为new.music.#
    channel->queueBind("exchange1", "queue2", "new.music.#");
    auto func = std::bind(cb, channel, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    channel->basicConsume("consumer1", argv[1],false, func);

    while(1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    conn->closeChannel(channel);

    return 0;
}