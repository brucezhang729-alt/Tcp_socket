// tcp_client.cpp
//
// 基于压缩包 lesson52/4.EchoTcpServer/EchoTcpClient.cc 的独立 TCP 客户端。
// 通信流程：socket -> connect -> send/recv
//
// 客户端和服务器约定一行是一条消息，'\n' 是消息结束标志。
//
// 编译：g++ -std=c++17 -Wall -Wextra -pedantic tcp_client.cpp -o tcp_client
// 运行：./tcp_client 127.0.0.1 8888

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
#include <unistd.h>

namespace
{
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

bool RecvLine(int sockfd, std::string *line)
{
    line->clear();

    while (true)
    {
        char ch = 0;
        const ssize_t n = recv(sockfd, &ch, 1, 0);

        if (n == 0)
        {
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

        if (ch != '\r')
        {
            line->push_back(ch);
        }
    }
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: "
                  << argv[0]
                  << " server_ip server_port\n";
        return EXIT_FAILURE;
    }

    uint16_t server_port = 0;
    if (!ParsePort(argv[2], &server_port))
    {
        std::cerr << "invalid server port\n";
        return EXIT_FAILURE;
    }

    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        Die("socket");
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);

    // 把命令行中的点分十进制 IPv4 地址转换成网络地址。
    if (inet_pton(AF_INET,
                  argv[1],
                  &server.sin_addr) != 1)
    {
        std::cerr << "invalid IPv4 address\n";
        close(sockfd);
        return EXIT_FAILURE;
    }

    // connect 发起 TCP 连接。
    // 客户端没有显式 bind，操作系统会自动分配本地临时端口。
    if (connect(sockfd,
                reinterpret_cast<sockaddr *>(&server),
                sizeof(server)) < 0)
    {
        std::cerr << "connect to "
                  << argv[1]
                  << ":"
                  << server_port
                  << " failed: "
                  << std::strerror(errno)
                  << '\n';

        close(sockfd);
        return EXIT_FAILURE;
    }

    std::cout << "connect to "
              << argv[1]
              << ":"
              << server_port
              << " success\n";

    while (true)
    {
        std::cout << "please Enter# ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            break;
        }

        if (line == "q")
        {
            break;
        }

        // 添加 '\n'，告诉服务器这一条 TCP 应用层消息结束。
        line.push_back('\n');

        if (!SendAll(sockfd, line))
        {
            std::cerr << "send failed\n";
            break;
        }

        std::string response;
        if (!RecvLine(sockfd, &response))
        {
            std::cout << "server closed the connection\n";
            break;
        }

        std::cout << response << '\n';
    }

    close(sockfd);
    return EXIT_SUCCESS;
}