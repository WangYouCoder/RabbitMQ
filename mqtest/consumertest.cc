#include "../mqserver/consume.hpp"
#include <gtest/gtest.h>

ConsumerManeger::ptr cmp;

class ConsumerTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        cmp = std::make_shared<ConsumerManeger>();
        cmp->initQueueConsumer("queue1");
    }
    virtual void TearDown() override
    {
        cmp->clear();
    }
};

void cb(const std::string &tag, const BasicProperties *bp, const std::string &body)
{
    std::cout << tag << " 消费了: " << body << std::endl;
}

TEST(consumer_test, insert_test)
{
    cmp->create("consumer1", "queue1", false, cb);
    cmp->create("consumer2", "queue1", false, cb);
    cmp->create("consumer3", "queue1", false, cb);

    std::cout << cmp->size("queue1") << std::endl;

    ASSERT_EQ(cmp->exists("consumer1", "queue1"), true);
    ASSERT_EQ(cmp->exists("consumer2", "queue1"), true);
    ASSERT_EQ(cmp->exists("consumer3", "queue1"), true);
}

TEST(consumer_test, remove_test)
{
    cmp->remove("consumer1", "queue1");

    ASSERT_EQ(cmp->exists("consumer1", "queue1"), false);
    ASSERT_EQ(cmp->exists("consumer2", "queue1"), true);
    ASSERT_EQ(cmp->exists("consumer3", "queue1"), true);
}

TEST(consumer_test, choose_test)
{
    Consumer::ptr cp = cmp->choose("queue1");
    ASSERT_NE(cp.get(), nullptr);
    ASSERT_EQ(cp->tag, "consumer2");
    
    cp = cmp->choose("queue1");
    ASSERT_NE(cp.get(), nullptr);
    ASSERT_EQ(cp->tag, "consumer3");

    cp = cmp->choose("queue1");
    ASSERT_NE(cp.get(), nullptr);
    ASSERT_EQ(cp->tag, "consumer2");
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ConsumerTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    return 0;
}


