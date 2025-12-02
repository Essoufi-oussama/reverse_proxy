#include "../Server.hpp"

void Server::send_request_server(int fd, Data& data)
{
    int bytes_sent = send(fd, data.write_buffer.c_str() + data.bytes_sent, data.write_buffer.size() - data.bytes_sent, 0);
    std::cout << "Backend fd=" << fd << " sent " << bytes_sent << " bytes.\n";
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
    data.bytes_sent += bytes_sent;
    if (data.bytes_sent < data.write_buffer.size())
        return ;
    else
    {
        data.write_buffer.clear();
        struct epoll_event events;
        events.data.fd = fd;
        events.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
    }
}

void Server::backend_server_read(int fd, Data& data)
{
    char buffer[BUFFER_SIZE];

    auto make_client_socket_writable = [] (int fd, int epoll_fd) {
        epoll_event events;
        memset(&events, 0, sizeof(epoll_event));
        events.data.fd = fd;
        events.events = EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
    };
    int bytes = recv(fd, buffer, BUFFER_SIZE -1, 0);
    int client_fd = data.sockfd;
    if (bytes < 0)
    {
        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
          return;
        send_error_code(data.sockfd, 502);
        safe_close(fd, backend_map);
        return ;
    }
    if (bytes == 0)
    {
        if (data.headers_done && check_body(data))
        {
            auto it = client_map.find(client_fd);
            if (it != client_map.end()) 
            {
                it->second.write_buffer = data.read_buffer;
                make_client_socket_writable(client_fd, epoll_fd);
            }
        }
        else if (data.read_buffer.empty())
            send_error_code(client_fd, 502);
        safe_close(fd, backend_map);
        return; 
    }
    buffer[bytes] = 0;
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
            std::cerr << "Error code " << error_code << " received while processing backend fd=" << fd << "\n";
            send_error_code(data.sockfd, error_code);
            safe_close(fd, backend_map);
            return;
        } 
    }
    auto it = client_map.find(client_fd);
    if (it == client_map.end()) 
    {
        std::cerr << "Client fd=" << client_fd << " not found for backend fd=" << fd << "\n";
        safe_close(fd, backend_map);
        return;
    }
    auto& client_data = client_map.find(client_fd)->second;
    client_data.write_buffer = data.read_buffer;

    safe_close(fd, backend_map);
    make_client_socket_writable(client_fd, epoll_fd);
}
