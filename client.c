#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 4 || argc == 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <command>\n", argv[0]);
        return -1;
    }

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sock);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return -1;
    }

    // Prepare and send the command
    char command[1024] = {0};
    int i;
    for (i = 3; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) strcat(command, " "); // Add space between command parts
    }

    if (send(sock, command, strlen(command), 0) < 0) {
        perror("Failed to send command");
        close(sock);
        return -1;
    }

    // Receive and print the response from the server
    if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("Failed to receive response");
        close(sock);
        return -1;
    }

    printf("%s", buffer);

    // Close the socket
    close(sock);
    return 0;
}
