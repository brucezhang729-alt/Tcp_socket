// tcp_server.cpp
// 代码的核心流程：
// socket -> bind -> listen -> accept -> read/write
//
// 本版本使用“每个客户端一个线程”，因此不依赖压缩包中的 ThreadPool.hpp。
// 为了处理 TCP 没有消息边界的问题，客户端和服务器约定：一行是一条消息，\n 是结束标志。
//
// 编译：g++ -std=c++17 -Wall -Wextra -pedantic -pthread tcp_server.cpp -o tcp_server
// 运行：./tcp_server 8888

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{
constexpr uint16_t kDefaultPort = 8888;
constexpr int kBacklog = 16;

void Die(const std::string &message)
{
    std::cerr << message << ": " << std::strerror(errno) << '\n';
    std::exit(EXIT_FAILURE);
}

bool ParsePort(const char *text, uint16_t *port)
{
    try
    {
        const unsigned long value = std::stoul(text);
        if (value > 65535)
        {
            return false;
        }

        *port = static_cast<uint16_t>(value);
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

std::string PeerToString(const sockaddr_in &peer)
{
    char ip[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr)
    {
        return "<invalid-address>";
    }

    return std::string(ip) + ":" +
           std::to_string(ntohs(peer.sin_port));
}

// TCP 的 send/write 可能只发送一部分数据，因此必须循环发送。
bool SendAll(int sockfd, const std::string &data)
{
    std::size_t sent_total = 0;

    while (sent_total < data.size())
    {
        const ssize_t n = send(sockfd,
                               data.data() + sent_total,
                               data.size() - sent_total,
                               0);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }

        if (n == 0)
        {
            return false;
        }

        sent_total += static_cast<std::size_t>(n);
    }

    return true;
}

// 从 TCP 字节流中读取一整行。遇到 '\n' 才认为一条消息结束。
// 这样可以处理一次 recv 只收到半行的情况。
bool RecvLine(int sockfd, std::string *line)
{
    line->clear();

    while (true)
    {
        char ch = 0;
        const ssize_t n = recv(sockfd, &ch, 1, 0);

        if (n == 0)
        {
            // 客户端正常关闭连接。
            return !line->empty();
        }

        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }

        if (ch == '\n')
        {
            return true;
        }

        // 兼容 Windows 风格的 "\r\n" 行尾。
        if (ch != '\r')
        {
            line->push_back(ch);
        }
    }
}

void ServiceClient(int client_fd, sockaddr_in peer)
{
    const std::string peer_name = PeerToString(peer);
    std::cout << "accepted client: " << peer_name << '\n';

    while (true)
    {
        std::string line;
        if (!RecvLine(client_fd, &line))
        {
            break;
        }

        std::cout << peer_name << " says: " << line << '\n';

        const std::string response = "server echo# " + line + "\n";
        if (!SendAll(client_fd, response))
        {
            std::cerr << "send response to " << peer_name << " failed\n";
            break;
        }
    }

    close(client_fd);
    std::cout << "client closed: " << peer_name << '\n';
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc != 1 && argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " [port]\n";
        return EXIT_FAILURE;
    }

    uint16_t port = kDefaultPort;
    if (argc == 2 && !ParsePort(argv[1], &port))
    {
        std::cerr << "invalid port\n";
        return EXIT_FAILURE;
    }

    // SOCK_STREAM 表示 TCP 字节流 socket。
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        Die("socket");
    }

    int reuse = 1;
    if (setsockopt(listen_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse,
                   sizeof(reuse)) < 0)
    {
        const int saved_errno = errno;
        close(listen_fd);
        errno = saved_errno;
        Die("setsockopt");
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind 给服务器设置固定的本地 IP 和端口。
    if (bind(listen_fd,
             reinterpret_cast<sockaddr *>(&local),
             sizeof(local)) < 0)
    {
        const int saved_errno = errno;
        close(listen_fd);
        errno = saved_errno;
        Die("bind");
    }

    // listen 把 socket 设置为监听状态，等待客户端连接。
    if (listen(listen_fd, kBacklog) < 0)
    {
        const int saved_errno = errno;
        close(listen_fd);
        errno = saved_errno;
        Die("listen");
    }

    std::cout << "TCP server listening on 0.0.0.0:" << port << '\n';

    while (true)
    {
        sockaddr_in peer{};
        socklen_t peer_length = sizeof(peer);

        // accept 返回一个新的已连接 socket。
        // listen_fd 继续负责接收后续客户端连接。
        const int client_fd = accept(
            listen_fd,
            reinterpret_cast<sockaddr *>(&peer),
            &peer_length);

        if (client_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "accept: "
                      << std::strerror(errno)
                      << '\n';
            continue;
        }

        // detach 后，线程独立处理当前客户端连接。
        std::thread(ServiceClient, client_fd, peer).detach();
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}
