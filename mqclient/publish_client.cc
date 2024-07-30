#include "c_connection.hpp"

int main()
{
    // 1. 实例化异步工作线程池
    AsynWorker::ptr awp = std::make_shared<AsynWorker>();
    std::cout << "create awp done" << std::endl;

    // 2. 实例化连接对象
    Connection::ptr conn = std::make_shared<Connection>("127.0.0.1", 8888, awp);
    std::cout << "create connection done" << std::endl;

    // 3. 通过连接创建信道
    Channel::ptr channel = conn->openChannel();
    std::cout << "open channel done" << std::endl;

    // 4. 通过信道提供的服务完成所需
    //    a.声明一个交换机exchange1, 交换机类型为广播模式
    google::protobuf::Map<std::string, std::string> tmp_args;
    channel->declareExhcange("exchange1", ExchangeType::FANOUT, true, false, tmp_args);
    std::cout << "declareExchange exchange1 done" << std::endl;

    //    b.声明一个队列queue1
    channel->declareQueue("queue1", true, false, false, tmp_args);
    std::cout << "declareQueue queue1 done" << std::endl;

    //    c.声明一个队列queue2
    channel->declareQueue("queue2", true, false, false, tmp_args);
    std::cout << "declareQueue queue2 done" << std::endl;

    //    d.绑定queue1 - exchange1, binding_key设置为queue1
    channel->queueBind("exchange1", "queue1", "queue1");
    std::cout << "declareBind exchange1 with queue1 done" << std::endl;

    //    d.绑定queue2 - exchange1, binding_key设置为new.music.#
    channel->queueBind("exchange1", "queue2", "new.music.#");
    std::cout << "declareBind exchange1 with queue2 done" << std::endl;

    // 5. 循环向交换机发布消息
    for(int i = 0; i < 10; i++)
    {
        channel->basicPulish("exchange1", nullptr, "hello word-" + std::to_string(i));
    }
    // 6. 关闭连接
    conn->closeChannel(channel);
    return 0;
}