#include "../mqserver/binding.hpp"
#include <gtest/gtest.h>
BindingManager::ptr bmp;

class QueueTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        bmp = std::make_shared<BindingManager>("./data/meta.db");
    }
    virtual void TearDown() override
    {
        bmp->clear();
    }
};

TEST(binding_test, insert_test)
{
    bmp->bind("exchange1", "queue1", "news.music.#", true);
    bmp->bind("exchange1", "queue2", "news.sport.#", true);
    bmp->bind("exchange2", "queue1", "news.music.pop", true);
    bmp->bind("exchange2", "queue2", "news.sport.football", true);
    ASSERT_EQ(bmp->size(),4);
}

TEST(binding_test, select_test)
{
    ASSERT_EQ(bmp->exists("exchange1", "queue1"), true);
    ASSERT_EQ(bmp->exists("exchange1", "queue2"), true);
    ASSERT_EQ(bmp->exists("exchange2", "queue1"), true);
    ASSERT_EQ(bmp->exists("exchange2", "queue2"), true);

    Binding::ptr bp = bmp->getBinding("exchange1", "queue1");
    ASSERT_NE(bp.get(), nullptr);
    ASSERT_EQ(bp->exchange_name, std::string("exchange1"));
    ASSERT_EQ(bp->msgqueue_name, std::string("queue1"));
    ASSERT_EQ(bp->binding_key, std::string("news.music.#"));
}

TEST(binding_test, select_exchange_test)
{
    MagQueueBindingMap mqbp = bmp->getExchangeBinding("exchange1");
    ASSERT_EQ(mqbp.size(), 2);
    ASSERT_NE(mqbp.find("queue1"), mqbp.end());
    ASSERT_NE(mqbp.find("queue2"), mqbp.end());
}

// TEST(binding_test, remove_queue_test)
// {
//     bmp->removeMasQueueBinding("queue1");
//     ASSERT_EQ(bmp->exists("exchange1", "queue1"), false);
//     ASSERT_EQ(bmp->exists("exchange2", "queue1"), false);
// }

// TEST(binding_test, remove_exchange_test)
// {
//     bmp->removeExchangeBinding("exchange1");
//     ASSERT_EQ(bmp->exists("exchange1", "queue1"), false);
//     ASSERT_EQ(bmp->exists("exchange1", "queue2"), false);
// }

TEST(binding_test, remove_exchange_test)
{
    ASSERT_EQ(bmp->exists("exchange1", "queue1"), true);
    bmp->unBind("exchange1", "queue1");
    ASSERT_EQ(bmp->exists("exchange1", "queue1"), false);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new QueueTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    return 0;
}