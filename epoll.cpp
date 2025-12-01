#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>

// --- 定数定義 ---
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PORT 8080

// --- 前回の設計に基づいた簡易クラス ---

enum ConnState
{
    WAIT_REQUEST,
    READING_REQUEST,
    WRITING_RESPONSE,
    CLOSE_CONNECTION
};

// モック: HttpRequest
class HttpRequest
{
public:
    std::string buffer;

    // データを受け取り、リクエスト終了(\r\n\r\n)を検知したらtrueを返す
    bool feed(const char *data, size_t size)
    {
        buffer.append(data, size);
        if (buffer.find("\r\n\r\n") != std::string::npos)
        {
            return true;
        }
        return false;
    }
};

// モック: HttpResponse
class HttpResponse
{
public:
    std::string responseData;
    size_t sentBytes;

    HttpResponse() : sentBytes(0) {}

    void makeSimpleResponse()
    {
        std::string body = "Hello from epoll server!\n";
        char buf[128];
        sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n", body.size());
        responseData = std::string(buf) + body;
    }

    const char *getData() const { return responseData.c_str() + sentBytes; }
    size_t getRemaining() const { return responseData.size() - sentBytes; }
    void advance(size_t n) { sentBytes += n; }
    bool isDone() const { return sentBytes >= responseData.size(); }
};

// モック: Client
class Client
{
public:
    int fd;
    ConnState state;
    HttpRequest req;
    HttpResponse res;

    Client(int fd) : fd(fd), state(WAIT_REQUEST) {}
    ~Client() { close(fd); }
};

// --- ユーティリティ ---

void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// --- Main Loop ---

int main()
{
    // 1. Listen Socketの作成
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 128) < 0)
    {
        perror("listen");
        return 1;
    }

    // ノンブロッキング設定は必須
    setNonBlocking(listen_fd);

    // 2. epollインスタンス作成
    int epoll_fd = epoll_create(1); // 引数は無視されるが正の値である必要あり
    if (epoll_fd < 0)
    {
        perror("epoll_create");
        return 1;
    }

    struct epoll_event ev, events[MAX_EVENTS];

    // Listen Socketを監視対象に追加
    ev.events = EPOLLIN; // 読み込み(新規接続)を監視
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0)
    {
        perror("epoll_ctl: listen_sock");
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // 3. イベントループ
    while (true)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // -1: 無限待ち
        if (nfds < 0)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i)
        {

            // --- ケースA: 新規接続 (Listen Socket) ---
            if (events[i].data.fd == listen_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
                if (conn_fd < 0)
                {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("accept");
                    continue;
                }

                setNonBlocking(conn_fd);

                // Clientオブジェクト生成
                Client *client = new Client(conn_fd);
                client->state = READING_REQUEST;

                // epollに追加 (ptrにClientオブジェクトを持たせるのが定石)
                ev.events = EPOLLIN | EPOLLET; // Edge Trigger推奨だが今回は簡単のためLevel Triggerでも可
                ev.data.ptr = client;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) < 0)
                {
                    perror("epoll_ctl: add client");
                    delete client;
                }
                std::cout << "[Connect] FD: " << conn_fd << std::endl;
            }
            // --- ケースB: 既存クライアントのI/O ---
            else
            {
                Client *client = static_cast<Client *>(events[i].data.ptr);

                // 読み込みイベント
                if (events[i].events & EPOLLIN)
                {
                    char buf[BUFFER_SIZE];
                    ssize_t n = recv(client->fd, buf, sizeof(buf), 0);

                    if (n <= 0)
                    {
                        // 切断またはエラー
                        std::cout << "[Disconnect] FD: " << client->fd << std::endl;
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                        delete client;
                    }
                    else
                    {
                        // データ受信 -> Requestへ
                        if (client->req.feed(buf, n))
                        {
                            std::cout << "[Request Complete] FD: " << client->fd << std::endl;

                            // リクエスト完了 -> レスポンス作成
                            client->state = WRITING_RESPONSE;
                            client->res.makeSimpleResponse();

                            // 監視イベントを EPOLLOUT (書き込み可能) に変更
                            ev.events = EPOLLOUT;
                            ev.data.ptr = client;
                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev);
                        }
                    }
                }
                // 書き込みイベント
                else if (events[i].events & EPOLLOUT)
                {
                    ssize_t n = send(client->fd, client->res.getData(), client->res.getRemaining(), 0);

                    if (n < 0)
                    {
                        perror("send");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                        delete client;
                    }
                    else
                    {
                        client->res.advance(n);
                        if (client->res.isDone())
                        {
                            std::cout << "[Response Sent] FD: " << client->fd << std::endl;
                            // 送信完了 -> 切断 (Keep-AliveならここでEPOLLINに戻してReset)
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                            delete client;
                        }
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}