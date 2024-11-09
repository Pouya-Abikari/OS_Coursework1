#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ifaddrs.h>

int string_to_ip(const char *ip_str, struct in_addr *addr) {
    unsigned int byte1, byte2, byte3, byte4;
    if (sscanf(ip_str, "%u.%u.%u.%u", &byte1, &byte2, &byte3, &byte4) != 4) {
        fprintf(stderr, "Invalid IP address format\n");
        return -1;
    }
    addr->s_addr = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4;
    return 0;
}

char *ip_to_string(struct in_addr addr, char *buffer, size_t len) {
    snprintf(buffer, len, "%u.%u.%u.%u",
             (addr.s_addr >> 24) & 0xFF,
             (addr.s_addr >> 16) & 0xFF,
             (addr.s_addr >> 8) & 0xFF,
             addr.s_addr & 0xFF);
    return buffer;
}

int resolve_hostname_to_ip(const char *hostname, char *ip, size_t ip_len) {
    struct ifaddrs *ifaddr, *ifa;
    int family;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {  
            if (strncmp(ifa->ifa_name, "lo", 2) == 0) {  
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                ip_to_string(addr->sin_addr, ip, ip_len);
                freeifaddrs(ifaddr);
                return 0;
            }
        }
    }

    freeifaddrs(ifaddr);
    fprintf(stderr, "Failed to resolve hostname\n");
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <command...>\n", argv[0]);
        return -1;
    }

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char ip[INET_ADDRSTRLEN] = {0};

    if (resolve_hostname_to_ip(argv[1], ip, sizeof(ip)) != 0) {
        fprintf(stderr, "Error resolving hostname: %s\n", argv[1]);
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (string_to_ip(ip, &serv_addr.sin_addr) != 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return -1;
    }

    char command[1024] = {0};
    for (int i = 3; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) strcat(command, " ");
    }

    if (send(sock, command, strlen(command), 0) < 0) {
        perror("Failed to send command");
        close(sock);
        return -1;
    }

    if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("Failed to receive response");
        close(sock);
        return -1;
    }

    printf("%s", buffer);

    close(sock);
    return 0;
}
