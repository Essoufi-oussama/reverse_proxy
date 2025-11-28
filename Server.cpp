#include "Server.hpp"

Server::Server()
{
    //get backend socket
    epoll_fd = epoll_create(3);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create");
    int yes = 1;
    addrinfo *p,*res, events;
    memset(&events, 0, sizeof(events));
    events.ai_socktype = SOCK_STREAM;
    events.ai_family = AF_UNSPEC;
    if (getaddrinfo(NULL, "8080", &events, &res) == -1)
        throw std::runtime_error("getaddrinfo fail");
    for (p = res; p != NULL ;p = p->ai_next)
    {
        int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        {
            close(sockfd);
            perror("setosockopt");
            throw std::runtime_error("setsockopt failed");
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            continue;
        }
        server_socket = sockfd;
        break;
    }
    freeaddrinfo(res);
    if (p == NULL)
    {
        throw std::runtime_error("Error couldn't bind to any ip address");
    }
    int opts = fcntl(server_socket, F_GETFL);
    fcntl(server_socket, F_SETFL, opts | O_NONBLOCK);
    if (listen(server_socket, BACKLOG) == -1)
    {
        perror("listen");
        throw std::runtime_error("Listen failed");
    }
    struct epoll_event server_event;
    server_event.data.fd = server_socket;
    server_event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &server_event);
    std::cout << "Server listning on Port 8080\n" ;
}


Server::~Server()
{
    close(epoll_fd);
    close(server_socket);
    for(auto& x: backend_map)
    {
        close(x.second.sockfd);
        close(x.first);
    }
}


void Server::run()
{
    struct epoll_event events[MAX_EVENTS];
    while(1)
    {
       int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
       for (int i = 0; i < n ; i++)
       {
           int fd = events[i].data.fd;
           auto it_client = client_map.find(fd);
           auto it_server = backend_map.find(fd);
            if (events[i].events & EPOLLIN)
            {

                if (fd == server_socket)
                {
                    std::cout << "received new connection\n";
                    add_new_connection();
                }
                // else if (backend_map.find(fd) != backend_map.end())
                //     backend_server_read(fd, backend_map[fd]);
                else if (it_client != client_map.end())
                {
                    if (it_client->second.sockfd == - 1)
                        client_read(fd, it_client->second);
                }
            }
            // else if (events[i].events & EPOLLOUT)
            // {
            //     if (backend_map.find(fd) != backend_map.end())
            //         send_request_server(fd, backend_map[fd]);
            // }
       } 
    }
}