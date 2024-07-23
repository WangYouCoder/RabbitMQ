#include "../mqserver/exchange.hpp"
#include <gtest/gtest.h>
ExchangeManager::ptr emp;

class ExchangeTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        emp = std::make_shared<ExchangeManager>("./data/meta.db");
    }
    virtual void TearDown() override
    {
        emp->clear();
    }
};

TEST(exchange_test, insert_test)
{
    std::unordered_map<std::string, std::string> map = {{"k1", "v1"}};
    emp->declareExchange("exchange1", ExchangeType::DIRECT, true, false, map);
    emp->declareExchange("exchange2", ExchangeType::DIRECT, true, false, map);
    emp->declareExchange("exchange3", ExchangeType::DIRECT, true, false, map);
    ASSERT_EQ(emp->size(), 3);
}

TEST(exchange_test, select_test)
{
    ASSERT_EQ(emp->exists("exchange3"), true);
    Exchange::ptr exp = emp->selectExchange("exchange3");
    ASSERT_NE(emp.get(), nullptr);
    ASSERT_EQ(exp->name, "exchange3");
    ASSERT_EQ(exp->durable, true);
    ASSERT_EQ(exp->auto_delete, false);
    ASSERT_EQ(exp->type, ExchangeType::DIRECT);
    ASSERT_EQ(exp->getArgs(), std::string("k1=v1"));
}


TEST(exchange_test, delete_test)
{
    emp->deleteExchange("exchange3");
    Exchange::ptr exp = emp->selectExchange("exchange3");
    ASSERT_NE(emp.get(), nullptr);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ExchangeTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    return 0;
}