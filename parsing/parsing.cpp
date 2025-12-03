#include "../Server.hpp"

bool Server::check_body(Data& client_data)
{
    auto finished_body = [] (const std::string& body_start, size_t body_length) {
        return body_start.size() >= body_length;
    };
    if (client_data.content_length > 0)
    {
        std::string& buffer = client_data.read_buffer;
        size_t body_start = buffer.find("\r\n\r\n") + 4;
        if (!finished_body(buffer.c_str() + body_start, client_data.content_length))
            return false;
    }
    return true;
}

std::string valid_request_line(const std::string& buffer)
{
    auto valid_method = [] (const std::string& method, size_t method_end)
    {
        std::string_view valid[6] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS"};
        for(int i = 0; i < 6 ; i++)
        {
            if (method.compare(0, method_end, valid[i]) == 0)
                return true;
        }
        return false;
    };
    auto check_version = [] (const std::string& buffer)
    {
        std::string_view valid[] = {"HTTP/1.1", "HTTP/2", "HTTP/3"};

        for (auto& x : valid)
        {
            if (buffer.compare(0, x.length(), x) == 0 && buffer[x.length()] == '\r')
                return;
        }
        throw 400;
    };

    int method_end = buffer.find(' ');
    if (method_end == std::string::npos)
            throw 400;
    if (!valid_method(buffer, method_end))
        throw 400;
    int path_pos = buffer.find(' ', method_end + 1);
    if (path_pos == std::string::npos)
        throw 400;
    int version_end = buffer.find("\r\n", path_pos + 1) ;
    check_version(buffer.c_str() + path_pos + 1);
    return buffer.substr(0, method_end); 
}

void valid_response_line(const std::string& buffer)
{
    auto check_version = [] (const std::string& buffer)
    {
        std::string_view valid[] = {"HTTP/1.0", "HTTP/1.1", "HTTP/2", "HTTP/3"};

        for (auto& x : valid)
        {
            if (buffer.compare(0, x.length(), x) == 0)
                return;
        }
        throw 502;
    };
    auto valid_status_code = [] (const std::string& buffer) {

        if(buffer.size() != 3)
            return false;
        return (std::all_of(buffer.begin(), buffer.end(), [] (unsigned char c) {return std::isdigit(c);}));
    };
    size_t version_end = buffer.find(' ');
    if (version_end == std::string::npos)
        throw 502;
    size_t status_code_end = buffer.find(' ', version_end + 1);
    if (status_code_end == std::string::npos)
    {
        status_code_end = buffer.find("\r\n", version_end + 1);
        if( status_code_end == std::string::npos)
            throw 502;
    }
    std::string version = buffer.substr(0, version_end);
    std::string status  = buffer.substr(version_end + 1, status_code_end - (version_end + 1));
    check_version(version);
    if(!valid_status_code(status))
        throw 502;
    // there is parsing fro response phrase ? maybe later
}

int extract_content_length(const std::string& content_length)
{
    size_t header_end = content_length.find("\r\n");

    size_t semicolon = content_length.find(':') + 1;
    std::string value = content_length.substr(semicolon, header_end - semicolon);
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
        start++;
    size_t stop = value.size();
    while (stop > start && (value[stop - 1] == ' ' || value[stop - 1] == '\t'))
        stop--;
    for (size_t i = start; i < stop; i++)
    {
        if (!std::isdigit((unsigned char)value[i]))
            throw 400; //invalid header
    }
    value = value.substr(start, stop - start);
    return std::stoi(value);
}


void Server::check_header(const std::string& header, size_t& headers_length, std::unordered_set <std::string>& elements, bool from_client )
{
    size_t line_end = header.find("\r\n");
    headers_length += line_end;
    if (line_end > MAX_HEADER_LENGTH)
        throw 431; //header_too_large
    if (headers_length > MAX_HEADERS_LENGTH)
        throw 431; //headers too large
    size_t semicolon_pos = header.find(':');
    if (semicolon_pos == 0 || semicolon_pos == std::string::npos || semicolon_pos > line_end)
        throw 400; //invalid header
    if (from_client)
    {
        for (size_t i = 0; i < semicolon_pos; i++) // key should not have spaces
        {
            if (std::isspace(header[i]))
                throw 400; // same
        }
    }
    std::string key = header.substr(0, semicolon_pos);
    std::transform(key.begin(), key.end(), key.begin(), [] (unsigned char c){return std::tolower(c);});
    if (elements.find(key) == elements.end())
        elements.insert(key);
    else
    {
        auto unique_header = [] (const std::string& key){
            return (
                key == "host" ||
                key == "content-length" ||
                key == "authorization" ||
                key == "proxy-authorization" ||
                key == "content-type" ||
                key == "expect" ||
                key == "upgrade" ||
                key == "connection"
            );
        };
        if (unique_header(key))
            throw 400;
    }
    //check if value has control characters
    auto is_control_char = [] (int c) {
        return((c < 32 && c != 9) || c == 127);
    };
    for (size_t i = semicolon_pos; i < line_end; i++)
    {
        if (is_control_char(header[i]))
            throw 400; //error
    }
}

int Server::validate_headers(const std::string& headers, const std::string& method, bool from_client)
{
    size_t end_pos = headers.find("\r\n\r\n");
    if (end_pos == std::string::npos) // empty header
        throw 400;
    size_t line_end = 0;
    size_t headers_length = 0;

    std::unordered_set <std::string> elements;
    while(line_end < end_pos)
    {
        size_t previous = line_end;
        line_end = headers.find( "\r\n", previous);
        if (line_end == std::string::npos)
            break;
        check_header(headers.c_str() + previous, headers_length, elements, from_client);
        line_end += 2;
    }
    if (from_client)
    {
        if (elements.find("host") == elements.end())
            throw 400; //invalid
        if(method == "POST" || method == "PUT")
        {
            if (elements.find("content-length") == elements.end())
                throw 411; //invalid 411 error
            size_t content_location = headers.find("content-length");
            return (extract_content_length(headers.c_str() + content_location));
        }
    }
    else
    {
        
        if (elements.find("content-length") != elements.end())
        {
            size_t content_location = headers.find("Content-Length");
            return (extract_content_length(headers.c_str() + content_location));
        }
    }
    return (0);
}

bool Server::feed(Data& data, bool from_client)
{
    std::string& buffer = data.read_buffer;
    if (!data.end_headers_found)
    {
        size_t start_index = (data.parsed_offset > 3) ? data.parsed_offset - 3 : 0;
        size_t tmp = buffer.find("\r\n\r\n", start_index);
        if (tmp == std::string::npos)
        {
  
            if (buffer.size() > MAX_HEADERS_LENGTH)
                throw 431 ;
            data.parsed_offset = (buffer.size() > 3) ? buffer.size() - 3 : 0;
            return false;
        }
        else
        {
            data.parsed_offset = tmp + 4;
            data.end_headers_found = true;
        }
    }
    // we know all headers are here this will only be done once
    std::string method {""};
    // can tolerate only 1 CRLF
    size_t line_start {0};
    if ( buffer.size() >= 2 && buffer[0] == '\r' && buffer[1] == '\n')
        line_start = 2;
    if (from_client)
        method = valid_request_line(buffer.c_str() + line_start);
    else
        valid_response_line(buffer.c_str() + line_start);
    size_t headers_start = buffer.find("\r\n", line_start);
    data.content_length = validate_headers(buffer.c_str() + headers_start + 2, method, from_client);
    data.headers_done = true;
    return true;
}