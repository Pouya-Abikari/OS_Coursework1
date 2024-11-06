#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 2200

typedef struct query_node {
    char ip_address[32];
    unsigned short int port;
    struct query_node* next;
} Query;

typedef struct rule_node {
    char ip_range[32];
    char port_range[16];
    Query* queries;
    struct rule_node* next;
} Rule;

typedef struct command_history {
    char* command;
    struct command_history* next;
} CommandHistory;

CommandHistory* command_head = NULL;
Rule* rule_list = NULL;

typedef struct _CmdArg {
    bool is_interactive;
} CmdArg;

void parse_ip(const char *ip_str, int ip_array[4]) {
    char ip_copy[16];
    strcpy(ip_copy, ip_str);
    
    char *token = strtok(ip_copy, ".");
    int i = 0;
    
    while (token != NULL) {
        ip_array[i] = atoi(token);
        token = strtok(NULL, ".");
        i++;
    }
}

void process_ip_range(const char *ip_range, int start_ip[4], int end_ip[4]) {
    char *dash_pos = strchr(ip_range, '-');
    
    if (dash_pos != NULL) {
        char start_ip_str[16], end_ip_str[16];
        
        strncpy(start_ip_str, ip_range, dash_pos - ip_range);
        start_ip_str[dash_pos - ip_range] = '\0';
        
        strcpy(end_ip_str, dash_pos + 1);
        
        parse_ip(start_ip_str, start_ip);
        parse_ip(end_ip_str, end_ip);
    } else {
        parse_ip(ip_range, start_ip);
        memset(end_ip, 0, sizeof(int) * 4);  // No end IP, so we set it to 0
    }
}

void add_command_history(CommandHistory** head, const char* command) {
    CommandHistory* new_command = malloc(sizeof(CommandHistory));
    if (!new_command) {
        fprintf(stderr, "Memory allocation failed for new command.\n");
        exit(EXIT_FAILURE);
    }
    new_command->command = strdup(command);  
    new_command->next = NULL;               

    if (*head == NULL) {
        *head = new_command;
    } else {
        CommandHistory* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_command;
    }
}

void print_commands(CommandHistory* head) {
    for (CommandHistory* current = head; current; current = current->next) {
        printf("%s", current->command);
    }
}

int is_valid_ip_part(const char* part) {
    int num = atoi(part);
    return strlen(part) > 0 && strlen(part) <= 3 && num >= 0 && num <= 255;
}

int is_valid_ip(const char* ip) {
    char ip_copy[64];  // Adjusted size to handle potential range (e.g., 255.255.255.255-255.255.255.255)
    strncpy(ip_copy, ip, sizeof(ip_copy) - 1);
    ip_copy[63] = '\0';  // Ensure null termination

    // Check for an IP range
    char *dash_pos = strchr(ip_copy, '-');
    if (dash_pos) {
        *dash_pos = '\0';  // Split into two parts: start IP and end IP
        char* start_ip = ip_copy;
        char* end_ip = dash_pos + 1;
        return is_valid_ip(start_ip) && is_valid_ip(end_ip);  // Validate both parts separately
    }

    // Validate a single IP
    char* token;
    int count = 0;
    for (token = strtok(ip_copy, "."); token; token = strtok(NULL, "."), count++) {
        if (!is_valid_ip_part(token) || count >= 4) return 0;
    }
    return count == 4;
}

int is_valid_port_range(const char* port_range) {
    int port1, port2;
    if (sscanf(port_range, "%d-%d", &port1, &port2) == 2) {
        // Ensure port1 <= port2 and both within valid range
        return (port1 >= 0 && port1 <= 65535 && port2 >= port1 && port2 <= 65535);
    }
    // Handle single port
    return (sscanf(port_range, "%d", &port1) == 1 && port1 >= 0 && port1 <= 65535);
}

int compare_ip(int ip1[4], int ip2[4]) {
    for (int i = 0; i < 4; i++) {
        if (ip1[i] < ip2[i]) return -1;
        if (ip1[i] > ip2[i]) return 1;
    }
    return 0;
}

int is_ip_in_range(const char *ip_range, const char *ip) {
    int start_ip[4], end_ip[4], target_ip[4];
    process_ip_range(ip_range, start_ip, end_ip);
    parse_ip(ip, target_ip);

    if (compare_ip(start_ip, target_ip) <= 0 && (end_ip[0] == 0 || compare_ip(target_ip, end_ip) <= 0)) {
        return 1;  
    }
    return 0;  
}

int is_port_in_range(const char *port_range, int port) {
    int port_start, port_end;
    char *dash_pos = strchr(port_range, '-');
    
    if (dash_pos != NULL) {
        port_start = atoi(port_range);
        port_end = atoi(dash_pos + 1);
        return port >= port_start && port <= port_end;
    } else {
        return atoi(port_range) == port;
    }
}

//int is_valid_port(int port) {
//    return port >= 0 && port <= 65535;
//}

int query_exists(Query* query_list, const char* ip, int port) {
    Query* current = query_list;
    while (current) {
        if (strcmp(current->ip_address, ip) == 0 && current->port == port) {
            return 1;  // Query exists
        }
        current = current->next;
    }
    return 0;  // Query does not exist
}

Query* add_query(Query* query_list, const char* ip_address, unsigned short int port) {
    Query* new_query = malloc(sizeof(Query));
    if (!new_query) {
        fprintf(stderr, "Memory allocation failed for new query.\n");
        exit(EXIT_FAILURE);
    }
    strncpy(new_query->ip_address, ip_address, sizeof(new_query->ip_address) - 1);
    new_query->port = port;
    new_query->next = NULL;

    if (query_list == NULL) {
        return new_query;  // If the list is empty, the new query is the first node
    }

    // Append the new query to the end of the list
    Query* current = query_list;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_query;
    return query_list;
}

Rule* add_rule(Rule* head, const char* ip_range, const char* port_range) {
    // Check if the rule already exists
    // Rule* current = head;
    // while (current) {
    //     if (strcmp(current->ip_range, ip_range) == 0 && strcmp(current->port_range, port_range) == 0) { // Rule exists, add it again
    //         printf("Duplicate rule, not added\n");
    //         return head;  // Rule exists, add it again
    //     }
    //     current = current->next;
    // }

    // Create a new rule if it doesn't already exist
    Rule* new_rule = malloc(sizeof(Rule));
    if (!new_rule) {
        fprintf(stderr, "Memory allocation failed for new rule.\n");
        exit(EXIT_FAILURE);
    }

    strncpy(new_rule->ip_range, ip_range, sizeof(new_rule->ip_range) - 1);
    new_rule->ip_range[sizeof(new_rule->ip_range) - 1] = '\0';  // Ensure null termination
    strncpy(new_rule->port_range, port_range, sizeof(new_rule->port_range) - 1);
    new_rule->port_range[sizeof(new_rule->port_range) - 1] = '\0';  // Ensure null termination
    new_rule->queries = NULL;
    new_rule->next = NULL;

    // Link the new rule to the end of the list
    if (head == NULL) {
        head = new_rule;
    } else {
        Rule* temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_rule;
    }

    printf("Rule added\n");
    return head;
}

void add_command(const char* str) {
    add_command_history(&command_head, str);
}

void print_rules(Rule* head) {
    Rule* current_rule = head;

    while (current_rule != NULL) {
        printf("Rule: %s %s\n", current_rule->ip_range, current_rule->port_range);
        Query* current_query = current_rule->queries;
        while (current_query != NULL) {
            printf("Query: %s %d\n", current_query->ip_address, current_query->port);
            current_query = current_query->next;
        }
        current_rule = current_rule->next;
    }
}

void process_add_rule(char* line) {
    char ip[32], port[16];
    if (sscanf(line, "A %31s %15s", ip, port) != 2) {
        printf("Invalid rule 1\n");
        return;
    }

    if (!is_valid_ip(ip)) {
        printf("Invalid rule 2\n");
        return;
    }

    if (!is_valid_port_range(port)) {
        printf("Invalid rule 3\n");
        return;
    }

    rule_list = add_rule(rule_list, ip, port);
}

void process_args(int argc, char** argv, CmdArg* pCmd) {
    pCmd -> is_interactive = false;
    if (argc == 2 && strcmp(argv[1], "-i") == 0){
        pCmd -> is_interactive = true;
    }
}

int check_ip_port(Rule* rule_list, const char* ip, int port) {
    Rule* current_rule = rule_list;
    int connection_accepted = 0;

    while (current_rule) {
        if (is_ip_in_range(current_rule->ip_range, ip) && is_port_in_range(current_rule->port_range, port)) {
            connection_accepted = 1; 
            if (!query_exists(current_rule->queries, ip, port)) {
                current_rule->queries = add_query(current_rule->queries, ip, port);
                printf("Connection accepted\n");
                return 1;
            }
        }
        current_rule = current_rule->next;
    }

    if (connection_accepted) {
        printf("Connection accepted\n");
    } else {
        printf("Connection rejected\n");
    }

    return connection_accepted;
}

void process_check_ip(char* line) {
    char ip[32];
    int port;
    if (sscanf(line, "C %31s %d", ip, &port) != 2) {
        printf("Illegal IP address or port specified\n");
        return;
    }

    check_ip_port(rule_list, ip, port);
}

void process_delete_rule(char *rule) {
    char ip[32], port[16];
    if (sscanf(rule, "D %31s %15s", ip, port) != 2) {
        printf("Invalid delete command\n");
        return;
    }

    if (!is_valid_ip(ip) || !is_valid_port_range(port)) {
        printf("Invalid rule\n");
        return;
    }

    Rule* current = rule_list;
    Rule* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->ip_range, ip) == 0 && strcmp(current->port_range, port) == 0) {
            if (prev == NULL) {
                rule_list = current->next;  // Head case
            } else {
                prev->next = current->next;
            }
            free(current);
            printf("Rule deleted\n");
            return;
        }
        prev = current;
        current = current->next;
    }
    printf("Rule not found\n");
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Forward declaration of existing functions to test
void process_add_rule(char* line);
void process_check_ip(char* line);
void print_rules(Rule* head);
void process_delete_rule(char* line);

// Helper function to simulate a command input
void simulate_command(char* command) {
    printf("\nSimulating command: %s\n", command);
    if (command[0] == 'A') {
        process_add_rule(command);
    } else if (command[0] == 'C') {
        process_check_ip(command);
    } else if (command[0] == 'L') {
        print_rules(rule_list);
    } else if (command[0] == 'D') {
        process_delete_rule(command);
    } else {
        printf("Invalid command\n");
    }
}

void run_tests() {
    printf("\n--- Running Enhanced Automated Tests ---\n\n");

    // Test rule addition
    printf("Simulating command: A 1.1.1.1-1.1.1.20 1-20\n");
    process_add_rule("A 1.1.1.1-1.1.1.20 1-20");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    printf("Simulating command: A 1.1.1.1-1.1.1.30 1-30\n");
    process_add_rule("A 1.1.1.1-1.1.1.30 1-30");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test invalid rule addition
    printf("Simulating command: A 1.1.1.1-1.1.1.20 abc\n");
    process_add_rule("A 1.1.1.1-1.1.1.20 abc");

    printf("Simulating command: A 1.1.1.300-1.1.1.20 1-20\n");
    process_add_rule("A 1.1.1.300-1.1.1.20 1-20");

    // Add duplicate connections
    printf("Simulating command: C 1.1.1.15 10\n");
    process_check_ip("C 1.1.1.15 10");

    printf("Simulating command: C 1.1.1.15 10\n");
    process_check_ip("C 1.1.1.15 10");  // Should not add again

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test overlapping rules (both should accept)
    printf("Simulating command: C 1.1.1.19 19\n");
    process_check_ip("C 1.1.1.19 19");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test with boundary ports
    printf("Simulating command: A 1.1.1.40-1.1.1.50 0-65535\n");
    process_add_rule("A 1.1.1.40-1.1.1.50 0-65535");

    printf("Simulating command: C 1.1.1.45 65535\n");
    process_check_ip("C 1.1.1.45 65535");

    printf("Simulating command: C 1.1.1.45 0\n");
    process_check_ip("C 1.1.1.45 0");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Add a rule with a single IP and test connections
    printf("Simulating command: A 1.1.1.50 100\n");
    process_add_rule("A 1.1.1.50 100");

    printf("Simulating command: C 1.1.1.50 100\n");
    process_check_ip("C 1.1.1.50 100");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test deletion
    printf("Simulating command: D 1.1.1.1-1.1.1.20 1-20\n");
    process_delete_rule("D 1.1.1.1-1.1.1.20 1-20");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test deletion of non-existing rule
    printf("Simulating command: D 2.2.2.2-2.2.2.5 5\n");
    process_delete_rule("D 2.2.2.2-2.2.2.5 5");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test invalid deletion command format
    printf("Simulating command: D 1.1.1.1\n");
    process_delete_rule("D 1.1.1.1");

    printf("Simulating command: L\n");
    print_rules(rule_list);

    // Test query logging order consistency with multiple commands in between
    printf("Simulating command: R\n");
    print_commands(command_head);

    printf("\n--- End of Enhanced Automated Tests ---\n");
}

void run_server() {
    int sock = 0;
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((new_socket = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        int argc_client;
        recv(new_socket, &argc_client, sizeof(argc_client), 0);

        //send();
        printf("Hello message sent to client\n");

        close(new_socket);
    }

    close(sock); 
}

void run_interactive() {
    size_t n = 0;
    char *str = NULL;
    int res;

    while (1) {
        res = getline(&str, &n, stdin);

        if (res == -1) {
            free(str);
            break;
        }

        add_command(str);

        char firstChar = str[0];
        switch (firstChar) {
            case 'A':
                process_add_rule(str);
                break;
            case 'C':
                process_check_ip(str);
                break;
            case 'R':
                print_commands(command_head);
                break;
            case 'L':
                print_rules(rule_list);
                break;
            case 'D':
                process_delete_rule(str);
                break;
            default:
                printf("Illegal request\n");
                break;
        }

        free(str);
        str = NULL;
    }
}

int main1(int argc, char** argv) {
    run_tests(); // Run all automated tests
    return 0;
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

/*int main (int argc, char ** argv) {
    printf ("Server to be written\n");
    return 0;
}
*/
