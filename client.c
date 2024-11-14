#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 4096

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <command...>\n", argv[0]);
        return -1;
    }

    struct addrinfo hints, *res;
    int sock;
    char buffer[BUFSIZE] = {0};

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("Socket creation error");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Connection Failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    char command[BUFSIZE] = {0};
    for (int i = 3; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) strcat(command, " ");
    }

    if (send(sock, command, strlen(command), 0) < 0) {
        perror("Failed to send command");
        close(sock);
        return -1;
    }

    int bytes_read;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';  
        printf("%s", buffer);
    }

    if (bytes_read < 0) {
        perror("Read error");
    }

    close(sock);
    return 0;
}