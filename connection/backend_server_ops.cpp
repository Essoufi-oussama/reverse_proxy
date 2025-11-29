#include "../Server.hpp"

void Server::send_request_server(int fd, Data& data)
{
    int bytes_sent = send(fd, data.write_buffer.c_str(), data.write_buffer.length(), 0);
    if (bytes_sent < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            send_error_code(data.sockfd, 503);
            backend_map.erase(fd);
        }
        return;
    }
    if (bytes_sent < data.write_buffer.size())
        data.write_buffer = data.write_buffer.substr(bytes_sent, data.write_buffer.size() - bytes_sent);
    else
    {
        data.write_buffer = "";
        struct epoll_event events;
        events.data.fd = fd;
        events.events = EPOLLIN;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
    }
}


void Server::backend_server_read(int fd, Data& data)
{
    char buffer[BUFFER_SIZE];

    int bytes = recv(fd, buffer, BUFFER_SIZE -1, 0);
    if (bytes <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
        send_error_code(data.sockfd, 502);
        close(fd);
        backend_map.erase(fd);
        std::cout << "Error receiving response from backend server\n";
        return ;
    }

    data.read_buffer += buffer;
    if (data.headers_done)
    {
        if (!check_body(data))
            return ; 
    }
    else
    {
        try
        {
            if (feed(data, false))
            {
                if (!check_body(data))
                    return ; 
            }
            else
                return ;
        }
        catch(int error_code)
        {
            send_error_code(data.sockfd, error_code);
            close(fd);
            backend_map.erase(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            return;
        } 
    }
    //
    auto& client_data = client_map.find(data.sockfd)->second;
    client_data.write_buffer = data.read_buffer;
    close(fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    backend_map.erase(fd);
    epoll_event events;
    memset(&events, 0, sizeof(epoll_event));
    events.data.fd = data.sockfd;
    events.events = EPOLLOUT;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
}