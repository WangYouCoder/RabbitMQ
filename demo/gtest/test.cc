#include <iostream>
#include <gtest/gtest.h>

/*
    ASSERT_  断言失败继续运行
    EXPECT_  断言失败退出

    注意: 断言宏只能在单元测试中进行
*/

// TEST(test/*测试的总名称*/, great_than/*单元测试名称*/)
// {
//     int age = 20;
//     ASSERT_GT(age, 18);
//     printf("OK!\n");
// }

// int main(int argc, char* argv[])
// {
//     testing::InitGoogleTest(&argc, argv);
//     int ret = RUN_ALL_TESTS();
//     if(ret == 1) std::cout << "成功\n";
//     return 0;
// }

class MyEnvironment : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        std::cout << "单元测试前进行初始化\n";
    }

    virtual void TearDown() override
    {
        std::cout << "单元测试结束后进行清理环境\n";
    }
};

TEST(MyEnvironment, test1)
{
    std::cout << "单元测试\n";  
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new MyEnvironment);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "成功\n";
    return 0;
}