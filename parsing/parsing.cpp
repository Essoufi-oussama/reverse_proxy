#include "../Server.hpp"

bool Server::feed(Data& data, bool from_client)
{
    bool done = false;
    
    std::string buffer = data.read_buffer;
    size_t tmp = buffer.find("\r\n\r\n");
    if (tmp == std::string::npos)
        return false;
    try
    {
        std::string method {""};
        if (from_client)
            method = valid_request_line(buffer);
        else
            valid_response_line(buffer);
        size_t headers_start = buffer.find("\r\n");
        int content_length = validate_headers(buffer.c_str() + headers_start + 2, method, from_client);
        if (content_length > 0)
        {
            auto isbody_done = [] (const std::string& buffer, int length) {return buffer.size() < length;};
            isbody_done(buffer.c_str() + tmp, content_length);
        }
        data.content_length = content_length;
        data.headers_done = true;
    }
    catch(bool error)
    {
        std::cerr << e.what() << '\n';
    }
    
    return done;
}