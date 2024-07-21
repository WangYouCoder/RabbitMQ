#include "logger.hpp"
#include "util.hpp"

int main()
{
    // ILOG("hello world");
    // std::string str = "asd...13.###.ccc";
    // std::vector<std::string> result;
    // int n = Util::split(str, ".", result);
    // for(int i = 0; i < n; i++)
    // {
    //     std::cout << result[i] << std::endl;
    // }
    // size_t num = 1;
    // std::cout << (num>>(7*8) & 0xff);

    std::cout << Util::uuid() << std::endl;
    return 0;
}