#include "../Server.hpp"


void Server::open_backend_connection(Data& client_data, int fd)
{
    addrinfo *p, *res, hints;

    memset(&hints, 0 , sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("localhost", "8081", &hints, &res) == -1)
    {
        throw std::runtime_error("get addrinfo fail");
    }
    int backenfd;
    for (p = res; p != NULL; p = p->ai_next)
    {
        int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            continue;
        }
        backenfd = sockfd;
        break;
    }
    freeaddrinfo(res);
    if (p == NULL)
        throw 503;

    int opts = fcntl(backenfd, F_GETFL);
    fcntl(backenfd, F_SETFL, opts | O_NONBLOCK);
    backend_map.emplace(backenfd, Data{});
    auto &it = backend_map.find(backenfd)->second;
    it.write_buffer = client_data.read_buffer;
    it.sockfd = fd;

    struct   epoll_event server_events;
    memset(&server_events, 0, sizeof(server_events));
    server_events.data.fd = backenfd;
    server_events.events = EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, backenfd, &server_events);

    // for client remove epollIN
    client_data.sockfd = backenfd;
    client_data.read_buffer.clear();
    struct   epoll_event client_events;
    memset(&client_events, 0, sizeof(client_events));
    client_events.data.fd = fd;
    client_events.events = EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &client_events);
}

void Server::add_new_connection()
{
    while(true)
    {
        int client_fd = accept(server_socket, NULL, NULL);
        if (client_fd == -1)
            return;
        int opts = fcntl(client_fd, F_GETFL);
        fcntl(client_fd, F_SETFL, opts | O_NONBLOCK);
        client_map.emplace(client_fd, Data());
        struct   epoll_event server_events;
        server_events.data.fd = client_fd;
        server_events.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &server_events);
    }
}