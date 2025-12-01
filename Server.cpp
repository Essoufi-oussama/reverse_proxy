#include "Server.hpp"


void Server::safe_close(int fd, std::unordered_map<int, Data>& m) {
    if (fd == -1) return;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    m.erase(fd);
}


void Server::initialize_backend()
{
    addrinfo *p, *res, hints;

    memset(&hints, 0 , sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("localhost", "8081", &hints, &res) == -1)
    {
        throw std::runtime_error("get addrinfo fail");
    }
    for (p = res; p != NULL; p = p->ai_next)
    {
        int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
            continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            close(sockfd);
            // This one works, save it
            backend.addr_family = p->ai_family;
            backend.addr_len = p->ai_addrlen;
            backend.addr_protocol = p->ai_protocol;
            backend.sock_type = p->ai_socktype;
            std::memcpy(&backend.backennd_addr, p->ai_addr, p->ai_addrlen);
            freeaddrinfo(res);
            return;
        }
        close(sockfd);
    }
    throw "couldn't connect to a backend";
}

Server::Server()
{
    //get backend socket
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create");
    int yes = 1;
    addrinfo *p,*res, hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(NULL, "8080", &hints, &res) == -1)
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
    initialize_backend();
    std::cout << "Server listning on Port 8080\n" ;
}


Server::~Server()
{
    close(epoll_fd);
    close(server_socket);
    for(auto& x: backend_map)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, x.second.sockfd, NULL);
        close(x.second.sockfd);
        close(x.first);
    }
}

void Server::read_events(int fd)
{
    auto it_client = client_map.find(fd);
    auto it_server = backend_map.find(fd);
    if (fd == server_socket)
    {
        add_new_connection();
    }
    else if (it_server != backend_map.end())
        backend_server_read(fd, it_server->second);
    else if (it_client != client_map.end())
        client_read(fd, it_client->second);
}

void Server::write_events(int fd)
{
    auto it_server = backend_map.find(fd);
    auto it_client = client_map.find(fd);
    if (it_server != backend_map.end())
    {
        if(!it_server->second.backend_connected)
        {
            int error;
            socklen_t sock_len = sizeof(error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &sock_len);
            if (error == 0)
                it_server->second.backend_connected = true;
            else    
                it_server->second.backend_connected = false;
        }
        if (it_server->second.backend_connected)
            send_request_server(fd, it_server->second);
        else
        {
            send_error_code(it_server->second.sockfd, 500);
            safe_close(fd, backend_map); 
        }
    }
    else if (it_client != client_map.end())
    {
          
        send_response_client(fd, it_client->second);
    }
}

void Server::disconnect_events(int fd)
{
    auto it_server = backend_map.find(fd);
    auto it_client = client_map.find(fd);
    if (it_client != client_map.end())
    {
        // std::cout << "client disconnected.\n";
        if(it_client->second.sockfd != -1)
        {
            int backend_fd = it_client->second.sockfd;
            auto backend_it = backend_map.find(backend_fd);

            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, backend_fd, NULL);
            close(it_client->second.sockfd);
            backend_map.erase(it_client->second.sockfd);
        }
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        client_map.erase(fd);
    }
    else if (it_server != backend_map.end())
    {
        // std::cout << "backend disconnected.\n";
        int client_fd = it_server->second.sockfd;
        send_error_code(client_fd, 502);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        backend_map.erase(fd);
    }
}

void Server::run()
{
    struct epoll_event events[MAX_EVENTS];
    while(1)
    {
       int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    //    std::cout << "Active connections: client=" << client_map.size() 
    //               << " backend=" << backend_map.size() << std::endl;
       for (int i = 0; i < n ; i++)
       {
            int fd = events[i].data.fd;
            // std::cout << "[EVENT] fd=" << events[i].data.fd 
            // << " events=" << events[i].events << "\n";
  
            if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                disconnect_events(fd);
                continue ;
            }
            if (events[i].events & EPOLLIN)
                read_events(fd);
            if (events[i].events & EPOLLOUT)
                write_events(fd);
       } 
    }
}