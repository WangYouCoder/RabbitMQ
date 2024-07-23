#include "../mqserver/queue.hpp"
#include <gtest/gtest.h>

MsgQueueManager::ptr mqmp;

class MsgQueueTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        mqmp = std::make_shared<MsgQueueManager>("./data/meta.db");
    }
    virtual void TearDown() override
    {
        // mqmp->clear();
    }
};

TEST(queue_test, insert_test) 
{
    std::unordered_map<std::string, std::string> map = {{"k1", "v1"}};
    mqmp->declareQueue("queue1", true, false, false, map);
    mqmp->declareQueue("queue2", true, false, false, map);
    mqmp->declareQueue("queue3", true, false, false, map);
    ASSERT_EQ(mqmp->size(), 3);
}

TEST(queue_test, select_test)
{
    MsgQueue::ptr mqp = mqmp->selectQueue("queue1");
    ASSERT_NE(mqmp.get(), nullptr);
    ASSERT_EQ(mqp->name, "queue1");
    ASSERT_EQ(mqp->durable, true);
    ASSERT_EQ(mqp->exclusive, false);
    ASSERT_EQ(mqp->auto_delete, false);
    ASSERT_EQ(mqp->getArgs(), std::string("k1=v1"));
}

// TEST(queue_test, remove_test)
// {
//     mqmp->deleteQueue("queue1");
//     MsgQueue::ptr mqp = mqmp->selectQueue("queue1");
//     ASSERT_EQ(mqp.get(), nullptr);
//     ASSERT_EQ(mqmp->exists("queue1"), false);
// }

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new MsgQueueTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    return 0;
}