#include <iostream>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Response.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

char *input_str = nullptr;
size_t line_len = 0;
char *cmd_argv[4];
const char *default_host_name = "127.0.0.1";
const char *default_client_dir = "client_files";
const char *hostname = default_host_name;
const char *client_dir = default_client_dir;

size_t parse_input()
{
    size_t input_len = 0, index = 0;
    int is_parsing = 0;
    for (size_t i = 0; input_str[i]; i++)
    {
        if (isspace(input_str[i]))
            input_str[i] = 0, is_parsing = 0;
        else if (!is_parsing)
        {
            is_parsing = 1;
            cmd_argv[index++] = input_str + i;
        }
    }
    cmd_argv[index] = 0;
    return index;
}

std::string extract_file_name(const char *path)
{
    std::string str_path(path);
    int last_slash = str_path.find_last_of("/");
    return str_path.substr(last_slash + 1);
}

int init_socket(const char *hostname, int port)
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        printf("ERROR\n");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(hostname);
    server_addr.sin_port = htons(port);

    if (connect(server_socket, (const sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Problem\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Connected to host %s:%d\n", hostname, port);
    return server_socket;
}

int serve_get(int server_socket, const char *uri)
{
    const char *get_request_template = "GET %s HTTP/1.1\r\n\r\n";
    char buffer[1024];

    int buffer_len = sprintf(buffer, get_request_template, uri);
    int num_bytes = send(server_socket, buffer, buffer_len, 0);

    if (num_bytes < 0)
    {
        printf("Server Disconnected.\n");
        exit(EXIT_FAILURE);
    }

    num_bytes = recv(server_socket, buffer, 1024, 0);
    if (num_bytes <= 0)
        return -1;

    std::string response_str;
    response_str.append(buffer, buffer + num_bytes);
    Response response(response_str);
    if (response.status_code == 404)
        return 404;

    uri = strchr(uri, '/') + 1;
    char save_path[150];
    strcpy(save_path, client_dir);
    strcat(save_path, "/");
    strcat(save_path, uri);
    int fd = open(save_path, O_WRONLY | O_CREAT, 0666);
    if (fd < -1)
    {
        printf("Cannot create file\n");
        exit(EXIT_FAILURE);
    }
    if (response.rest.length())
        write(fd, response.rest.c_str(), response.rest.length());

    int remaining = response.file_length - response.rest.length();
    while (remaining && (num_bytes = recv(server_socket, buffer, 1024, 0)) > 0)
    {
        write(fd, buffer, num_bytes);
        remaining -= num_bytes;
    }
    close(fd);
    if (num_bytes <= 0)
        return -1;
    return 200;
}

int serve_post(int server_socket, const char *uri)
{
    int fd = open(uri, O_RDONLY, 0666);
    if (fd < 0)
    {
        std::cout << "File not found\n";
        return -1;
    }
    struct stat stbuf;
    fstat(fd, &stbuf);

    std::string request = "POST /" + std::string(client_dir) + "_" + extract_file_name(uri) + " HTTP/1.1\r\n" + "Content-Length: " + std::to_string(stbuf.st_size) + "\r\n\r\n";
    int bytes_sent = send(server_socket, request.c_str(), request.size(), 0);

    if (bytes_sent < 0)
    {
        std::cout << "Server disconnected\n";
        return -1;
    }

    if (sendfile(server_socket, fd, nullptr, stbuf.st_size) < 0)
    {
        std::cout << "Error sending file\n";
        return -1;
    }
    close(fd);
    return 200;
}

int main(int argc, char **argv)
{
    int port = 80;
    if (argc < 2)
    {
        printf("ERROR: Invalid arguments\n");
        exit(EXIT_FAILURE);
    }
    if (argc >= 3)
        client_dir = argv[2];
    if (argc >= 4)
        hostname = argv[3];
    if (argc >= 5)
        port = atoi(argv[4]);

    // Create Directory for client
    struct stat st = {0};
    if (stat(client_dir, &st) == -1)
    {
        mkdir(client_dir, 0700);
    }
    FILE *request_file = fopen(argv[1], "r");
    printf("host: %d\n", port);
    int server_socket = init_socket(hostname, port);

    while (getline(&input_str, &line_len, request_file) != -1)
    {
        printf("%s", input_str);
        int cmd_argc = parse_input();
        int port = 80;
        if (cmd_argv[3] != nullptr)
            port = atoi(cmd_argv[3]);

        // GET
        int status;
        if (strncmp(cmd_argv[0], "client_get", 20) == 0)
        {
            while ((status = serve_get(server_socket, cmd_argv[1])) == -1)
            {
                printf("Error sending request.. Resending\n");
                close(server_socket);
                server_socket = init_socket(hostname, port);
            }
            if (status == 200)
                printf("File received successfully\n");
            else
                printf("404 Not Found\n");
        }

        else if (strncmp(cmd_argv[0], "client_post", 20) == 0)
        {
            while (serve_post(server_socket, cmd_argv[1]) == -1)
            {
                printf("Error sending request.. Resending\n");
                close(server_socket);
                server_socket = init_socket(hostname, port);
            }
            printf("File sent successfully\n");
        }
        printf("\n\n--------------------------------------------\n");
    }

    if (input_str)
        free(input_str);
    fclose(request_file);
    return 0;
}