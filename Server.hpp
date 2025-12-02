#pragma once

#include <unordered_map>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <vector>
#include <sys/epoll.h>
#include <string_view>
#include <ctype.h>
#include <unordered_set>
#include <algorithm>
#define BACKLOG 4096
#define MAX_EVENTS 256
#define BUFFER_SIZE 512
#define MAX_HEADER_LENGTH 8000
#define MAX_HEADERS_LENGTH 32000
#define MAX_CLIENTS_AT_TIME 30
struct Data

{
    Data() : headers_done(false), content_length{0}, sockfd{-1}, read_buffer{""}, write_buffer{""}, bytes_sent{0}, backend_connected{false} {};
    int sockfd;
    std::string read_buffer;
    std::string write_buffer;
    int content_length;
    bool headers_done;
    size_t bytes_sent;
    bool backend_connected;
};

struct Backend
{
    sockaddr_storage backennd_addr;
    int sock_type;
    int addr_protocol;
    int addr_family;
    size_t addr_len;
};

class Server
{
    public:
        Server();
        ~Server();
        void run(); 
    private:
        int server_socket;
        int epoll_fd;
        Backend backend;
        std::unordered_map<int, Data> client_map;
        std::unordered_map<int, Data> backend_map;
        bool feed(Data& client_data, bool from_client);
        void open_backend_connection(Data& client_data, int fd);
        void add_new_connection();
        void backend_server_read(int fd, Data& data);
        void client_read(int fd, Data& client_data);
        bool check_body(Data& client_data);
        void send_request_server(int fd, Data& data);
        void check_header(const std::string& header, size_t& headers_length, std::unordered_set <std::string>& elements, bool from_client);
        int validate_headers(const std::string& headers, const std::string& method, bool from_client);
        void send_error_code(int fd, int error_code);
        void send_response_client(int fd, Data& data);
        void initialize_backend();
        void safe_close(int fd, std::unordered_map<int, Data>& m);
        void write_events(int fd);
        void read_events(int fd);
        void disconnect_events(int fd);
};
