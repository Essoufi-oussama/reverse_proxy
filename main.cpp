
#include "Server.hpp"


//  python3 -m http.server 8081 --bind 127.0.0.1 --directory ~/server8081
// curl -v "http://[::1]:8080/"
int main()
{
    try
    {
        Server server;

        server.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    
}