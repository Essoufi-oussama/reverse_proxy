#include "../Server.hpp"

void Server::add_new_connection()
{
    int client_fd = accept(server_socket, NULL, NULL);
    if (client_fd == -1)
        return;
    int opts = fcntl(client_fd, F_GETFL);
    fcntl(client_fd, F_SETFL, opts | O_NONBLOCK);
    client_map.emplace(client_fd, Data(client_fd));
    struct   epoll_event server_events;
    server_events.data.fd = client_fd;
    server_events.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &server_events);
}