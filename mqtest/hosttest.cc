#include <gtest/gtest.h>
#include "../mqserver/host.hpp"


class HostTest : public testing::Test {
    public:
        void SetUp() override {
            std::unordered_map<std::string, std::string> empty_map = {{"k1", "v1"}};
            _host = std::make_shared<VirtualHost>("host1", "./data/host1/message/", "./data/host1/host1.db");
            _host->declareExchange("exchange1", ExchangeType::DIRECT, true, false, empty_map);
            _host->declareExchange("exchange2", ExchangeType::DIRECT, true, false, empty_map);
            _host->declareExchange("exchange3", ExchangeType::DIRECT, true, false, empty_map);

            _host->declareQueue("queue1", true, false, false, empty_map);
            _host->declareQueue("queue2", true, false, false, empty_map);
            _host->declareQueue("queue3", true, false, false, empty_map);

            _host->bind("exchange1", "queue1", "news.music.#");
            _host->bind("exchange1", "queue2", "news.music.#");
            _host->bind("exchange1", "queue3", "news.music.#");
            
            _host->bind("exchange2", "queue1", "news.music.#");
            _host->bind("exchange2", "queue2", "news.music.#");
            _host->bind("exchange2", "queue3", "news.music.#");

            _host->bind("exchange3", "queue1", "news.music.#");
            _host->bind("exchange3", "queue2", "news.music.#");
            _host->bind("exchange3", "queue3", "news.music.#");

            _host->basicPublish("queue1", nullptr, "Hello World-1");
            _host->basicPublish("queue1", nullptr, "Hello World-2");
            _host->basicPublish("queue1", nullptr, "Hello World-3");
            
            _host->basicPublish("queue2", nullptr, "Hello World-1");
            _host->basicPublish("queue2", nullptr, "Hello World-2");
            _host->basicPublish("queue2", nullptr, "Hello World-3");
            
            _host->basicPublish("queue3", nullptr, "Hello World-1");
            _host->basicPublish("queue3", nullptr, "Hello World-2");
            _host->basicPublish("queue3", nullptr, "Hello World-3");
        }
        void TearDown() override {
            // _host->clear();
        }
    public:
        VirtualHost::ptr _host;
};


TEST_F(HostTest, init_test) {
    ASSERT_EQ(_host->existsExchange("exchange1"), true);
    ASSERT_EQ(_host->existsExchange("exchange2"), true);
    ASSERT_EQ(_host->existsExchange("exchange3"), true);
    
    ASSERT_EQ(_host->existsQueue("queue1"), true);
    ASSERT_EQ(_host->existsQueue("queue2"), true);
    ASSERT_EQ(_host->existsQueue("queue3"), true);
    
    ASSERT_EQ(_host->existsBinding("exchange1", "queue1"), true);
    ASSERT_EQ(_host->existsBinding("exchange1", "queue2"), true);
    ASSERT_EQ(_host->existsBinding("exchange1", "queue3"), true);
    
    ASSERT_EQ(_host->existsBinding("exchange2", "queue1"), true);
    ASSERT_EQ(_host->existsBinding("exchange2", "queue2"), true);
    ASSERT_EQ(_host->existsBinding("exchange2", "queue3"), true);

    ASSERT_EQ(_host->existsBinding("exchange3", "queue1"), true);
    ASSERT_EQ(_host->existsBinding("exchange3", "queue2"), true);
    ASSERT_EQ(_host->existsBinding("exchange3", "queue3"), true);

    MessagePtr msg1 = _host->basicConsume("queue1");
    ASSERT_EQ(msg1->paylaod().body(), std::string("Hello World-1"));
    MessagePtr msg2 = _host->basicConsume("queue1");
    ASSERT_EQ(msg2->paylaod().body(), std::string("Hello World-2"));
    MessagePtr msg3 = _host->basicConsume("queue1");
    ASSERT_EQ(msg3->paylaod().body(), std::string("Hello World-3"));
    MessagePtr msg4 = _host->basicConsume("queue1");
    ASSERT_EQ(msg4.get(), nullptr);
}


TEST_F(HostTest, remove_exchange) {
    _host->deleteExchange("exchange1");
    ASSERT_EQ(_host->existsBinding("exchange1", "queue1"), false);
    ASSERT_EQ(_host->existsBinding("exchange1", "queue2"), false);
    ASSERT_EQ(_host->existsBinding("exchange1", "queue3"), false);
}

TEST_F(HostTest, remove_queue) {
    _host->deleteQueue("queue1");
    ASSERT_EQ(_host->existsBinding("exchange1", "queue1"), false);
    ASSERT_EQ(_host->existsBinding("exchange2", "queue1"), false);
    ASSERT_EQ(_host->existsBinding("exchange3", "queue1"), false);
    
    MessagePtr msg1 = _host->basicConsume("queue1");
    ASSERT_EQ(msg1.get(), nullptr);
}


TEST_F(HostTest, ack_message) {
    MessagePtr msg1 = _host->basicConsume("queue1");
    ASSERT_EQ(msg1->paylaod().body(), std::string("Hello World-1"));
    _host->basicAck(std::string("queue1"), msg1->paylaod().properties().id());

    MessagePtr msg2 = _host->basicConsume("queue1");
    ASSERT_EQ(msg2->paylaod().body(), std::string("Hello World-2"));
    _host->basicAck(std::string("queue1"), msg2->paylaod().properties().id());

    MessagePtr msg3 = _host->basicConsume("queue1");
    ASSERT_EQ(msg3->paylaod().body(), std::string("Hello World-3"));
    _host->basicAck(std::string("queue1"), msg3->paylaod().properties().id());

}



int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    return 0;
}