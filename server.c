#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 2200

typedef struct rule_node {
    char ip_range[32];
    char port_range[16];
    struct rule_node* next;
} Rule;

typedef struct command_history {
    char* command;
    char* query;
    struct command_history* next;
} CommandHistory;

typedef struct request {
    // ???
} Request;

typedef struct firewall_rule_node_list {
    Rule *rule;
    struct firewall_rule_node_list *next;
} FirewallRuleNodeList;

typedef struct _CmdArg {
    bool is_interactive;
} CmdArg;

void process_args(int argc, char** argv, CmdArg* pCmd) {
    pCmd -> is_interactive = false;
    if (argc == 2 && strcmp(argv[1], "-i") == 0){
        pCmd -> is_interactive = true;
    }
}

void parse_rule(char* pchrule, Rule* rule){
}

bool is_firewall_rule_valid(char *rule) {
    return 0;
}

void add_firewall_rule(char *rule) {
}

void process_requests_list() {
    //shows all requests in order they were given

}

void process_add_rule(char *rule) {
    if (is_firewall_rule_valid(rule)) {
        add_firewall_rule(rule);
    } else {
        printf("Invalid rule\n");
    }
}

void process_rule_check(char *rule) {
    //Checks whether a given IP address and Port are allowed according to the rules
}

void process_delete_rule(char *rule) {
    //Deletes a rule from stored rules, which also deletes the list of IP addresses and ports stored for this rule.
    //The rule should only be deleted if the stores exactly the same rule.
}

void process_rule_list() {
    //Lists all rules in the order they were added with their query (IP address and port)
}

void run_server() {
    printf("Running in server mode\n");
    //Rule* pRuleHead = NULL;
    //Request* pRequestHead = NULL;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char *message = "Hello from server";

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Server runs forever, accepting new connections in an infinite loop
    while (1) {
        // Accept an incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        //receive argc and argv from client
        int argc_client;
        recv(new_socket, &argc_client, sizeof(argc_client), 0);


        // Send a simple message to the client
        send(new_socket, message, strlen(message), 0);
        printf("Hello message sent to client\n");

        // Close the connection with the current client
        close(new_socket);
    }

    close(server_fd); // This will never be reached, but good practice to have
}

void run_interactive() {
    printf("Running in interactive mode\n");
    size_t n; 
    int res;
    while (1) {
        char *str = NULL;
        printf("Enter command: \n");
        scanf("Enter command: \n");
        res = getline (&str, &n, stdin);
        
        if (res == -1)
        {
            printf("Error reading input\n");
        }

        //get the command and get the first character
        char line = str[0]; //to be removed

        switch (line) {
            case 'A':
                //process_add_rule(line);
                printf("Command A\n");
                break;
            case 'D':
                //process_delete_rule(line);
                printf("Command D\n");
                break;
            case 'R':
                //process_requests_list(line);
                printf("Command R\n");
                break;
            case 'L':
                //process_rule_list(line);
                printf("Command L\n");
                break;
            case 'C':
                //process_rule_check(line);
                printf("Command C\n");
                break;
            default:
                printf("Invalid command\n");
                break;
        }
        free(str);
    }
}

int main(int argc, char ** argv) {
    /* to be written */
    
    CmdArg cmd;
    process_args(argc, argv, &cmd);
    if (cmd.is_interactive) {
        run_interactive();
    } else {
        run_server();
    }
    return 0;
}

//int main (int argc, char ** argv) {
//    /* to be written */
//    printf ("Server to be written\n");
//    return 0;
//}

