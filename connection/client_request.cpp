#include "../Server.hpp"

void Server::send_error_code(int fd, int error_code)
{
    auto error_reason = [] (int error_code) {
        
        switch (error_code)
        {
            case 400:
                return std::string("Bad Request");
            case 431:
                return std::string("Request Header Fields Too Large");
            case 414:
                return std::string("URI Too Long");
            case 503:
                return std::string("Service Unavailable");
            case 502:
                return std::string("Bad Gateway");
            default:
                break;
        }
        return std::string("");
    };
    Data& client_data = client_map.find(fd)->second;

    client_data.write_buffer = "HTTP/1.1 " + std::to_string(error_code) + ' ' + error_reason(error_code) + "\r\n";
    client_data.write_buffer += "Content-Length: 0\r\nConnection: close\r\n\r\n";
    int bytes = send(fd, client_data.write_buffer.c_str(), client_data.write_buffer.size(), 0);
    if (bytes < client_data.write_buffer.size() || (bytes <= 0 && (errno == EWOULDBLOCK || errno == EAGAIN)))
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLOUT;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
        if (bytes > 0)
            client_data.write_buffer = client_data.write_buffer.substr(bytes, client_data.write_buffer.size() - bytes);
        return; 
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    client_map.erase(fd);
    std::cout << "client disconnected because of bad request\n";
}

void Server::client_read(int fd, Data& client_data)
{
    char buffer[BUFFER_SIZE];
    int bytes = recv(fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        client_map.erase(fd);
        std::cout << "client Disconnected\n";
        return ;
    }
    buffer[bytes] = 0;
    client_data.read_buffer += buffer;
    if (client_data.headers_done)
    {
        if (!check_body(client_data))
            return ; 
    }
    else
    {
        try
        {
            if (feed(client_data, true))
            {
                if (!check_body(client_data))
                    return ; 
            }
            else
                return ;
        }
        catch(int error_code)
        {
            send_error_code(fd, error_code);
        }   
    }
    open_backend_connection(client_data, fd);
}


void Server::send_response_client(int fd, Data& data)
{
    std::string& buffer = data.write_buffer;
    int bytes = send(fd, buffer.c_str() + data.bytes_sent, buffer.size() - data.bytes_sent, 0);
    if (bytes <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        client_map.erase(fd);
        std::cout << "client_disconnected.\n";
        return ;
    }
    data.bytes_sent += bytes;
    if (data.bytes_sent < data.write_buffer.size())
        return;

    // everything sent properly make fd able to read again

    epoll_event events;
    memset(&events, 0, sizeof(events));
    events.data.fd = -1;
    events.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);

    data.write_buffer.clear();
    data.read_buffer.clear();
    data.bytes_sent = 0;
    data.sockfd = fd;

    //delete tests
    // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    // close(fd);
    // client_map.erase(fd);
};
