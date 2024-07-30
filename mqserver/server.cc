#include "broken.hpp"

int main()
{
    Server server(8888, "./data/");
    server.start();
    return 0;
}