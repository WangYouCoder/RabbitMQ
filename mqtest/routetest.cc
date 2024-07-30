#include "../mqserver/route.hpp"
#include <gtest/gtest.h>

class RouteTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {

    }
    virtual void TearDown() override
    {

    }
};

// TEST(route_test, legal_routing_key)
// {
//     std::string rkey1 = "new.music.pop";
//     std::string rkey2 = "new..music.pop";
//     std::string rkey3 = "new.,music.pop";
//     std::string rkey4 = "new.music_123.pop";
//     ASSERT_EQ(Route::isLegalRoutingKey(rkey1), true);
//     ASSERT_EQ(Route::isLegalRoutingKey(rkey2), true);
//     ASSERT_EQ(Route::isLegalRoutingKey(rkey3), false);
//     ASSERT_EQ(Route::isLegalRoutingKey(rkey4), true);
// }

// TEST(route_test, legal_binding_key)
// {
//     std::string bkey1 = "new.music.pop";
//     std::string bkey2 = "new.#.music.pop";
//     std::string bkey3 = "new.#.*.music.pop";
//     std::string bkey4 = "new.*.#.music.pop";
//     std::string bkey5 = "new.#.#.music.pop";
//     std::string bkey6 = "new.*.*.music.pop";
//     std::string bkey7 = "new.,music_123.pop";
//     ASSERT_EQ(Route::isLegalBindingKey(bkey1), true);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey2), true);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey3), false);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey4), false);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey5), false);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey6), true);
//     ASSERT_EQ(Route::isLegalBindingKey(bkey7), false);
// }
TEST(route_test, route)
{
    std::vector<std::string> bkeys = {
            "aaa",
            "aaa.bbb",
            "aaa.bbb",
            "aaa.bbb",
            "aaa.#.bbb",
            "aaa.bbb.#",
            "#.bbb.ccc",
            "aaa.bbb.ccc",
            "aaa.*",
            "aaa.*.bbb",
            "*.aaa.bbb",
            "#",
            "aaa.#",
            "aaa.#",
            "aaa.#.ccc",
            "aaa.#.ccc",
            "aaa.#.ccc",
            "#.ccc",
            "#.ccc",
            "aaa.#.ccc.ccc",
            "aaa.#.bbb.*.bbb"
        };
        std::vector<std::string> rkeys = {
            "aaa",
            "aaa.bbb",
            "aaa.bbb.ccc",
            "aaa.ccc",
            "aaa.bbb.ccc",
            "aaa.ccc.bbb",
            "aaa.bbb.ccc.ddd",
            "aaa.bbb.ccc",
            "aaa.bbb",
            "aaa.bbb.ccc",
            "aaa.bbb",
            "aaa.bbb.ccc",
            "aaa.bbb",
            "aaa.bbb.ccc",
            "aaa.ccc",
            "aaa.bbb.ccc",
            "aaa.aaa.bbb.ccc",
            "ccc",
            "aaa.bbb.ccc",
            "aaa.bbb.ccc.ccc.ccc",
            "aaa.ddd.ccc.bbb.eee.bbb"
        };
        std::vector<bool> result = {
            true,
            true,
            false,
            false,
            false,
            false,
            false,
            true,
            true,
            false,
            false,
            true,
            true,
            true,
            true,
            true,
            true,
            true,
            true,
            true,
            true
        };
        for (int i = 0; i < 6; i++) {
            // std::cout << "i: " << i << std::endl;
            ASSERT_EQ(Route::route(ExchangeType::TOPIC, rkeys[i], bkeys[i]), result[i]);
        }
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new RouteTest);
    int ret = RUN_ALL_TESTS();
    if(ret == 1) std::cout << "success" <<std::endl;
    // std::cout << Route::route(ExchangeType::TOPIC, std::string("aaa.bbb.ccc.ddd"), std::string("#.bbb.ccc")) << std::endl;
    return 0;
}