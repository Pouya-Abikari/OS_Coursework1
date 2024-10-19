#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return -1;
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        printf("Invalid address or Address not supported\n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    //send argc and argv to server, argv should not include ./client
    send(sock, &argc, sizeof(argc), 0);
    for (int i = 0; i < argc; i++) {
        send(sock, argv[i], strlen(argv[i]), 0);
    }
    printf("%d\n", argc);

    // Read message from server
    read(sock, buffer, 1024);
    printf("Message from server: %s\n", buffer);
    for (int i = 0; i < argc; i++) {
            printf("Argument %d: %s\n", i, argv[i]);
        }

    // Close the socket
    close(sock);
    return 0;
}

//int main (int argc, char ** argv) {
//    /* to be written */
//    printf ("Client to be written\n");
//    return 0;
//}
