#include "../Server.hpp"

void Server::client_read(int fd, Data& client_data)
{
    char buffer[BUFFER_SIZE];
    int bytes = recv(fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        close(fd);
        client_map.erase(fd);
        return ;
    }
    buffer[bytes] = 0;
    client_data.read_buffer += buffer;
    std::cout << "received: " << client_data.read_buffer;
    if (client_data.headers_done)
    {
        if (!check_body(client_data))
            return ; 
    }
    else
    {
        if (feed(client_data))
        {
            if (!check_body(client_data))
                return ; 
        }
        else
        {
            struct epoll_event events;
            memset(&events, 0 , sizeof(events));
            events.events = EPOLLOUT;
            events.data.fd = client_data.sockfd;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_data.sockfd,&events);
            return ;
        }
    }
    open_backend_connection(client_data);
    client_data.sockfd = 1;
}