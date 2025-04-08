#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 8080

char *url_decode(const char *src)
{
    char *result = malloc(strlen(src) + 1);
    char *dst = result;
    const char *pos = src;

    while (*pos)
    {
        if (*pos == '%' && pos[1] && pos[2])
        {
            int value;
            sscanf(pos + 1, "%2x", &value);
            *dst++ = (char)value;
            pos += 3;
        }
        else if (*pos == '+')
        {
            *dst++ = ' ';
            pos++;
        }
        else
        {
            *dst++ = *pos++;
        }
    }

    *dst = '\0';
    return result;
}

int main(int argc, char const* argv[])
{
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char* filename = "a.jpg";
    int img_fd;
    char buffer[1024] = { 0 };

    // Открыть файл с изображением
    if ((img_fd = open(filename, O_RDONLY)) < 0)
    {
        perror("open image file");
        exit(EXIT_FAILURE);
    }

    // Создание файлового дескриптора сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Принудительно привязать сокет к порту 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Принудительно привязать сокет к порту 8080
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Прочтение запроса от клиента
        char request[1024] = { 0 };
        valread = read(new_socket, request, sizeof(request));
        if (valread < 0)
        {
            perror("read");
            close(new_socket);
            continue;
        }
        if (strstr(request, "GET /image") != NULL)
        {
            char* response_header = "HTTP/1.1 200 OK\nContent-Type: image/jpeg\n\n";
            send(new_socket, response_header, strlen(response_header), 0);

            // Reset file pointer to the beginning of the image file
            lseek(img_fd, 0, SEEK_SET);

            // Отправка файла с изображением
            while ((valread = read(img_fd, buffer, sizeof(buffer))) > 0)
            {
                send(new_socket, buffer, valread, 0);
            }
        }
        else
        {
            char* message_start = strstr(request, "message=");
            if (message_start != NULL)
            {
                // Extract message from the request
                char* message_end = strchr(message_start, ' ');
                if (message_end != NULL)
                {
                    char message[1024];
                    strncpy(message, message_start + strlen("message="), message_end - message_start - strlen("message="));
                    message[message_end - message_start - strlen("message=")] = '\0';

                    // Decode URL-encoded message
                    char* decoded_message = url_decode(message);

                    // Create HTML response with the message above the image
                    char html_response[2048];
                    snprintf(html_response, sizeof(html_response), "HTTP/1.1 200 OK\nContent-Type: text/html; charset=UTF-8\n\n<!DOCTYPE html><html><head><title>Image Viewer</title></head><body><div>%s</div><img src=\"/image\" alt=\"Image\"></body></html>", decoded_message);

                    // Send HTML response to the client
                    send(new_socket, html_response, strlen(html_response), 0);

                    // Free dynamically allocated memory
                    free(decoded_message);
                }
            }
            else
            {
                // If the message is not found in the request, send error response
                char* error_response = "HTTP/1.1 400 Bad Request\nContent-Type: text/html\n\n<!DOCTYPE html><html><head><title>Error</title></head><body><h1>Error</h1><p>Message not found in request</p></body></html>";
                send(new_socket, error_response, strlen(error_response), 0);
            }
        }

        // Закрытие подключеного сокета
        close(new_socket);
    }
    // Закрытие прослушивающего сокета
    close(server_fd);
    // Closing the image file
    close(img_fd);
    return 0;
}
