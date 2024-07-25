#pragma once
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/logger.hpp"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
using namespace WY;
class Route
{
public:
    static bool isLegalRoutingKey(const std::string &routing_key)
    {
        for(auto &ch : routing_key)
        {
            if((ch >= 'a' && ch <= 'z') ||
               (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') ||
               ch == '_' || ch == '.')
            {
                continue;
            }
            return false;
        }
        return true;
    }
    static bool isLegalBindingKey(const std::string &binding_key)
    {
        for(auto &ch : binding_key)
        {
            if((ch >= 'a' && ch <= 'z') ||
               (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') ||
               ch == '_' || ch == '.'   ||
               ch == '*' || ch == '#')
            {
                continue;
            }
            return false;
        }

        // "new.#.music.pop"
        // * 和 # 必须独立存在，可能存在的方式: new.music.*.#
        std::vector<std::string> sub_str;
        Util::split(binding_key, ".", sub_str);
        for(std::string &str : sub_str)
        {
            if(str.size() > 1 &&
               (str.find("*") != std::string::npos ||
               str.find("#") != std::string::npos))
            {
                return false;
            }
        }

        // 不能连续出现
        for(int i = 1; i < sub_str.size(); i++)
        {
            if(sub_str[i] == "#" && sub_str[i-1] == "#")
                return false;

            if(sub_str[i] == "#" && sub_str[i-1] == "*")
                return false;

            if(sub_str[i] == "*" && sub_str[i-1] == "#")
                return false;
        }
        return true;
    }

    static bool route(ExchangeType type, const std::string &routing_key, const std::string &binding_key)
    {
        if(type == ExchangeType::DIRECT)
            return (routing_key == binding_key);
        else if(type == ExchangeType::FANOUT)
            return true;

        std::vector<std::string> bkey;
        std::vector<std::string> rkey;

        size_t bn = Util::split(binding_key, ".", bkey);
        size_t rn = Util::split(routing_key, ".", rkey);
        std::vector<std::vector<bool>> dp(bn + 1, std::vector<bool>(rn + 1, false));


        dp[0][0] = true;
        if(bkey[0] == "#")
            dp[1][0] = true;
        // for (int i = 1; i <= bn; i++) 
        // {
        //     if (bkey[i - 1] != "#") break;
        //     dp[i][0] = true;
        // }
        for(auto &s : bkey)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;
        for(auto &s : rkey)
        {
            std::cout << s << " ";
        }
        std::cout << std::endl;

        for(int i = 1; i <= bn; i++)
            for(int j = 1; j <= rn; j++)    
            {
                if(bkey[i-1] == rkey[j-1] || bkey[i-1] == "*")
                    dp[i][j] = dp[i-1][j-1];
                else if(bkey[i-1] == "#")
                    dp[i][j] = dp[i-1][j-1] || dp[i-1][j] || dp[i][j-1];
                
                // printf("dp[%d][%d]: ", i, j);
                // std::cout << dp[i][j] << " " << bkey[i-1] << " " << rkey[j-1] << std::endl;
            }
        return dp[bn][rn];
    }
};