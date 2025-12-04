#include "../Server.hpp"

void  rewrite_data(Data& client_data, std::string& buffer)
{
    buffer = client_data.request;
    std::string crlf {"\r\n"};

    for (auto &x : client_data.headers)
    {
        buffer += (x.first + ':' + x.second + crlf);
    }
    for (auto &x : client_data.same_field_headers)
    {
        buffer += (x + crlf);
    }
    buffer += "\r\n\r\n";
    buffer += client_data.read_buffer.substr(client_data.parsed_offset + 4, client_data.content_length);
}

void Server::open_backend_connection(Data& client_data, int client_fd)
{
    std::cout << "Opening backend connection for client fd=" << client_fd << "\n";
    int backendfd = socket(backend.addr_family, backend.sock_type, backend.addr_protocol);
    bool success = false;
    if (backendfd == -1)
        throw 503;
    int opts = fcntl(backendfd, F_GETFL);
    fcntl(backendfd, F_SETFL, opts | O_NONBLOCK);

    int result = connect(backendfd,(sockaddr *) &backend.backennd_addr, backend.addr_len);
    if(result == -1)
    {
        if (errno != EINPROGRESS)
        {
            close(backendfd);
            throw 503;
        }
    }
    else
        success = true;
    struct   epoll_event backend_event {};

    backend_event.data.fd = backendfd;
    backend_event.events = EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, backendfd, &backend_event) == -1)
    {
        perror("epoll_ctl");
        close(backendfd);
        throw std::runtime_error("epoll_ctl ADD backend failed");
    }
    
    Data backend_data {};
    rewrite_data(client_data, backend_data.write_buffer);
    backend_data.sockfd = client_fd;
    backend_data.backend_connected = success;
    backend_map[backendfd] = backend_data;

    client_data.sockfd = backendfd;
    struct   epoll_event client_events;
    memset(&client_events, 0, sizeof(client_events));
    client_events.data.fd = client_fd;
    client_events.events = EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &client_events) == -1)
    {
        perror("epoll_ctl");
        close(client_fd);
        throw std::runtime_error("epoll_ctl ADD backend failed");
    }
    size_t leftover_start = (client_data.parsed_offset + 4) + client_data.content_length;
    client_data.read_buffer = client_data.read_buffer.substr(leftover_start,  client_data.read_buffer.size() - leftover_start);
}

void Server::add_new_connection()
{
    int count = 0;
    while(count++ < MAX_CLIENTS_AT_TIME)
    {
        int client_fd = accept(server_socket, NULL, NULL);
        if (client_fd == -1)
            break ;
        std::cout << "New client connection fd=" << client_fd << "\n";
        int opts = fcntl(client_fd, F_GETFL);
        fcntl(client_fd, F_SETFL, opts | O_NONBLOCK);
        client_map.emplace(client_fd, Data());
        struct   epoll_event server_events;
        server_events.data.fd = client_fd;
        server_events.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &server_events);
    }
}