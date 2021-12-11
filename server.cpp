#include <iostream>
#include <sstream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <vector>
#include <string>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Request.h"


class Server
{
private:
    int server_socket_fd;
    sockaddr_in socket_addr;
    int address_len;
    static const int MAX_NUM_THREADS = 30;
    int PHYSICAL_THREADS;
    std::queue<int> clients_queue;
    std::vector<std::thread> thread_pool;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    bool close_server = false;
    static const int MAX_SECONDS = 30;

public:
    void Listen()
    {
        while (true)
        {
            acceptClient();
        }
    }

    Server(int port_number) : PHYSICAL_THREADS(std::thread::hardware_concurrency())
    {
        socket_addr.sin_family = AF_INET;
        socket_addr.sin_addr.s_addr = INADDR_ANY;
        socket_addr.sin_port = htons(port_number);

        server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        address_len = sizeof(socket_addr);
        if (server_socket_fd == -1)
        {
            std::cout << "Failed to create socket";
            exit(EXIT_FAILURE);
        }

        if (bind(server_socket_fd, (sockaddr *)&socket_addr, sizeof(socket_addr)) < 0)
        {
            std::cout << "failed to bind";
            exit(EXIT_FAILURE);
        }

        if (listen(server_socket_fd, 30) < 0)
        {
            std::cout << "Failed to listen";
            exit(EXIT_FAILURE);
        }
        init_server_pool();
    }

    void init_server_pool()
    {
        for (int i = 0; i < MAX_NUM_THREADS; i++)
            thread_pool.push_back(std::thread(&Server::worker_thread, this));
    }

    ~Server()
    {
        closeServer();
    }

    void closeServer()
    {
        {
            std::unique_lock<std::mutex> mtx_lock(queue_mutex);
            close_server = true;
        }
        queue_cv.notify_all();

        for (auto &thread : thread_pool)
            thread.join();
        thread_pool.clear();
        close(server_socket_fd);
    }

    void acceptClient()
    {
        int client_socket = accept(server_socket_fd, (sockaddr *)&socket_addr, (socklen_t *)&address_len);
        if (client_socket < 0)
        {
            std::cout << "Failed to accept";
            exit(EXIT_FAILURE);
        }
        timeval timeout;
        {
            std::unique_lock<std::mutex> mtx_lock(queue_mutex);
            int num_groups = clients_queue.size() / PHYSICAL_THREADS;
            timeout.tv_sec = MAX_SECONDS / (1 << num_groups);
            if (timeout.tv_sec == 0)
                timeout.tv_usec = 1000 * 1000 / 2;
        }

        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const void *)&timeout, sizeof(timeout));
        enqueue(client_socket);
    }

    void enqueue(int client_socket)
    {
        {
            std::unique_lock<std::mutex> mtx_lock(queue_mutex);
            clients_queue.push(client_socket);
        }
        queue_cv.notify_one();
    }

    int dequeue()
    {
        std::unique_lock<std::mutex> mtx_lock(queue_mutex);
        std::cout << "thread: " << std::this_thread::get_id() << " is waiting" << std::endl;
        queue_cv.wait(mtx_lock, [this]()
                      { return !clients_queue.empty() || close_server; });
        std::cout << "thread: " << std::this_thread::get_id() << " is working" << std::endl;

        if (close_server)
            return -1;

        int client_socket = clients_queue.front();
        clients_queue.pop();
        return client_socket;
    }

    int serve_get(int client_socket, Request &request)
    {
        struct stat stbuf;
        std::string file_path = "./server_files" + request.uri;
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            std::cout << "File does not exist\n";
            std::string response = request.http_version + " 404 Not Found\r\n" +
                                   "\r\n";
            send(client_socket, (const void *)response.c_str(), response.length(), 0);
            return -1;
        }
        fstat(fd, &stbuf);

        std::string ext = file_path.substr(file_path.find_last_of(".") + 1);
        std::string content_type;

        std::string response = request.http_version + " 200 OK\r\n" +
                               "Content-Type: text/" + ext + "; charset=UTF-8\r\n" +
                               "Content-Length: " + std::to_string(stbuf.st_size) + "\r\n\r\n";
        send(client_socket, (const void *)response.c_str(), response.length(), 0);
        if (sendfile(client_socket, fd, nullptr, stbuf.st_size) < 0)
        {
            std::cout << "Error sending file\n";
            close(fd);
            return -1;
        }
        close(fd);
        return 0;
    }

    int serve_post(int client_socket, Request &request)
    {
        std::string save_path = "./server_files" + request.uri;
        int fd = open(save_path.c_str(), O_CREAT | O_WRONLY, 0666);
        if (fd < 0)
        {
            std::cout << "File does not exist\n";
            return -1;
        }

        if (request.rest.length())
            write(fd, request.rest.c_str(), request.rest.length());
        int recv_bytes;

        int remaining = request.length - request.rest.length();
        char buffer[BUFFER_SIZE];
        while (remaining && (recv_bytes = recv(client_socket, buffer, std::min(BUFFER_SIZE, remaining), 0)) > 0)
        {
            write(fd, buffer, recv_bytes);
            remaining -= recv_bytes;
        }

        close(fd);
        if (remaining)
            return -1;
        return 0;
    }

    void worker_thread()
    {
        while (true)
        {
            // poll a connection from the queue
            int client_socket = dequeue();
            if (client_socket == -1)
                return;
            while (true)
            {
                std::string request_str;
                int num_bytes;
                char buffer[BUFFER_SIZE];
                bool connection_error = false;

                // receive http headers from client
                num_bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
                if (num_bytes < 0)
                {
                    printf("%d\n", errno);
                    printf("coonection timeout\n");
                    connection_error = true;
                    break;
                }
                else if (num_bytes == 0)
                {
                    printf("Client disconnected\n");
                    connection_error = true;
                    break;
                }
                request_str.append(buffer, buffer + num_bytes);

                if (connection_error)
                    break;

                // parse http request
                Request request(request_str);

                if (request.method == Method::GET)
                    serve_get(client_socket, request);
                else if (serve_post(client_socket, request) == -1)
                    break;
            }
            close(client_socket);
        }
    }
};

int main()
{
    int port_number = 9999;
    Server server(port_number);

    server.Listen();

    return 0;
}