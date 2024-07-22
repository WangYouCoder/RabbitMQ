#include "../mqserver/message.hpp"
#include <gtest/gtest.h>
#include "../mqcommon/util.hpp"
MessageManager::ptr mmp;

class MessageTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        mmp = std::make_shared<MessageManager>("./data/message/");
        mmp->initQueueMessage("queue1");
    }
    virtual void TearDown() override
    {
        // mmp->clear();
    }
};

TEST(message_test, insert_test)
{
    BasicProperties properties;
    properties.set_id(Util::uuid());
    properties.set_delivery_mode(DeliveryMode::DURABLE);
    properties.set_routing_key("new.music.pop");
    mmp->insert("queue1", &properties, "hello world-1", DeliveryMode::DURABLE);
    mmp->insert("queue1", nullptr, "hello world-2", DeliveryMode::DURABLE);
    mmp->insert("queue1", nullptr, "hello world-3", DeliveryMode::DURABLE);
    mmp->insert("queue1", nullptr, "hello world-4", DeliveryMode::DURABLE);
    mmp->insert("queue1", nullptr, "hello world-5", DeliveryMode::UNDURABLE);
    ASSERT_EQ(mmp->getable_count("queue1"), 5);
    ASSERT_EQ(mmp->total_count("queue1"), 4);
    ASSERT_EQ(mmp->valid_count("queue1"), 4);
    ASSERT_EQ(mmp->waitack_count("queue1"), 0);
}   

// TEST(message_test, select_test)
// {
//     MessagePtr msg1 = mmp->front("queue1");
//     ASSERT_EQ(msg1->paylaod().body(), "hello world-1");
//     ASSERT_EQ(mmp->getable_count("queue1"), 3);
//     ASSERT_EQ(mmp->waitack_count("queue1"), 1);
//     MessagePtr msg2 = mmp->front("queue1");
//     ASSERT_EQ(msg2->paylaod().body(), "hello world-2");
//     ASSERT_EQ(mmp->getable_count("queue1"), 2);
//     ASSERT_EQ(mmp->waitack_count("queue1"), 2);
//     MessagePtr msg3 = mmp->front("queue1");
//     ASSERT_EQ(msg3->paylaod().body(), "hello world-3");
//     ASSERT_EQ(mmp->getable_count("queue1"), 1);
//     ASSERT_EQ(mmp->waitack_count("queue1"), 3);
//     MessagePtr msg4 = mmp->front("queue1");
//     ASSERT_EQ(msg4->paylaod().body(), "hello world-4");
//     ASSERT_EQ(mmp->getable_count("queue1"), 0);
//     ASSERT_EQ(mmp->waitack_count("queue1"), 4);
//     MessagePtr msg5 = mmp->front("queue1");
//     ASSERT_EQ(msg5.get(), nullptr);
// }

// TEST(message_test, recovery_test)
// {
//     ASSERT_EQ(mmp->getable_count("queue1"), 4);
// }

TEST(message_test, delete_test)
{
    ASSERT_EQ(mmp->getable_count("queue1"), 5);
    MessagePtr msg1 = mmp->front("queue1");
    ASSERT_NE(msg1.get(), nullptr);
    ASSERT_EQ(msg1->paylaod().body(), std::string("hello world-1"));
    ASSERT_EQ(mmp->getable_count("queue1"), 4);
    ASSERT_EQ(mmp->waitack_count("queue1"), 1);
    mmp->ack("queue1", msg1->paylaod().properties().id());
    ASSERT_EQ(mmp->waitack_count("queue1"), 0);
    ASSERT_EQ(mmp->valid_count("queue1"), 3);
    ASSERT_EQ(mmp->total_count("queue1"), 4);
}


int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new MessageTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;

    // UtilFile f("./data/message/queue1.mqd");
    // std::string body;
    // f.read(body);
    // std::cout << body << std::endl;

    return 0;
}