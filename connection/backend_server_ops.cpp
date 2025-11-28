#include "../Server.hpp"

void Server::send_request_server(int fd, Data& data)
{
    int bytes_sent = send(fd, data.write_buffer.c_str(), data.write_buffer.length(), 0);
    if (bytes_sent < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
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
        std::cout << "sent full request to server: \n" << data.write_buffer;
        data.write_buffer = "";
        struct epoll_event events;
        events.data.fd = fd;
        events.events = EPOLLIN;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &events);
    }
}