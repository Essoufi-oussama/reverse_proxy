
#include "Server.hpp"

int main()
{
    try
    {
        Server server;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    
}