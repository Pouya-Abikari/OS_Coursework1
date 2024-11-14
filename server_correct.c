#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>
#include <arpa/inet.h>

pthread_rwlock_t rule_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t command_history_rwlock = PTHREAD_RWLOCK_INITIALIZER;

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

int compare_ip(int ip1[4], int ip2[4]) {
    for (int i = 0; i < 4; i++) {
        if (ip1[i] < ip2[i]) return -1;
        if (ip1[i] > ip2[i]) return 1;
    }
    return 0;
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

        if (compare_ip(start_ip, end_ip) >= 0) {
            memset(start_ip, 0, sizeof(int) * 4);
            memset(end_ip, 0, sizeof(int) * 4);
            return; 
        }
    } else {
        parse_ip(ip_range, start_ip);
        memcpy(end_ip, start_ip, sizeof(int) * 4); 
    }
}

void add_command_history(CommandHistory** head, const char* command) {
    char clean_command[1024];
    int i = 0, j = 0;

    while (isspace((unsigned char)command[i])) i++;

    while (command[i] && command[i] != '\n' && j < sizeof(clean_command) - 1) {
        clean_command[j++] = command[i++];
    }
    clean_command[j] = '\0';

    if (clean_command[0] != '\0') {
        CommandHistory* new_command = malloc(sizeof(CommandHistory));
        if (!new_command) {
            fprintf(stderr, "Memory allocation failed for new command.\n");
            exit(EXIT_FAILURE);
        }
        new_command->command = strdup(clean_command);
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
}

void print_commands(CommandHistory* head) {
    for (CommandHistory* current = head; current; current = current->next) {
        printf("%s\n", current->command);
    }
}

int is_valid_ip_part(const char* part) {
    if (*part == '\0') return 0; 

    for (int i = 0; part[i]; i++) {
        if (!isdigit(part[i])) return 0; 
    }
    int num = atoi(part);
    return strlen(part) > 0 && strlen(part) <= 3 && num >= 0 && num <= 255;
}

int is_valid_ip(const char* ip) {
    char ip_copy[64];
    strncpy(ip_copy, ip, sizeof(ip_copy) - 1);
    ip_copy[sizeof(ip_copy) - 1] = '\0';  

    char *dash_pos = strchr(ip_copy, '-');
    if (dash_pos) {
        *dash_pos = '\0';  
        char* start_ip = ip_copy;
        char* end_ip = dash_pos + 1;
        return is_valid_ip(start_ip) && is_valid_ip(end_ip);  
    }

    char* token;
    int count = 0;
    int period_count = 0;
    
    for (int i = 0; ip_copy[i] != '\0'; ++i) {
        if (ip_copy[i] == '.') {
            period_count++;
            if (i == 0 || ip_copy[i + 1] == '\0' || ip_copy[i + 1] == '.') {
                return 0;  
            }
        }
    }
    if (period_count != 3) return 0;  

    for (token = strtok(ip_copy, "."); token; token = strtok(NULL, ".")) {
        if (!is_valid_ip_part(token)) return 0; 
        count++;
    }
    return count == 4;  
}

int is_valid_port_range(const char* port_range) {
    int port1, port2;
    char *endptr;

    if (sscanf(port_range, "%d-%d", &port1, &port2) == 2) {
        return (port1 >= 0 && port1 <= 65535 && port2 >= port1 && port2 <= 65535 && port1 != port2);
    }

    port1 = strtol(port_range, &endptr, 10);
    if (*endptr == '\0' && port1 >= 0 && port1 <= 65535) {
        return 1;
    }

    return 0;
}

bool is_valid_port_str(const char* port_str) {
    int port = atoi(port_str);
    return (port >= 0 && port <= 65535);
}

int is_ip_in_range(const char *ip_range, const char *target_ip) {
    char start_ip_str[16], end_ip_str[16], *dash_pos;

    dash_pos = strchr(ip_range, '-');
    if (dash_pos) {
        strncpy(start_ip_str, ip_range, dash_pos - ip_range);
        start_ip_str[dash_pos - ip_range] = '\0';
        strcpy(end_ip_str, dash_pos + 1);
    } else {
        strcpy(start_ip_str, ip_range);
        strcpy(end_ip_str, ip_range);
    }

    uint32_t start_ip = ntohl(inet_addr(start_ip_str));
    uint32_t end_ip = ntohl(inet_addr(end_ip_str));
    uint32_t target_ip_num = ntohl(inet_addr(target_ip));

    return target_ip_num >= start_ip && target_ip_num <= end_ip;
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
        return new_query;
    }

    Query* current = query_list;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_query;
    return query_list;
}

Rule* add_rule(Rule* head, const char* ip_range, const char* port_range) {
    Rule* new_rule = malloc(sizeof(Rule));
    if (!new_rule) {
        fprintf(stderr, "Memory allocation failed for new rule.\n");
        exit(EXIT_FAILURE);
    }

    strncpy(new_rule->ip_range, ip_range, sizeof(new_rule->ip_range) - 1);
    new_rule->ip_range[sizeof(new_rule->ip_range) - 1] = '\0';  
    strncpy(new_rule->port_range, port_range, sizeof(new_rule->port_range) - 1);
    new_rule->port_range[sizeof(new_rule->port_range) - 1] = '\0';
    new_rule->queries = NULL;
    new_rule->next = NULL;

    if (head == NULL) {
        head = new_rule;
    } else {
        Rule* temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_rule;
    }

    return head;
}

void add_command(const char* str) {
    add_command_history(&command_head, str);
}

void print_rules(Rule* head) {
    Rule* current_rule = head;

    if (current_rule == NULL) {
        return;
    }

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

int process_add_rule(char* line) {
    char ip[32], port[16], extra[32];
    if (sscanf(line, "A %31s %15s %31s", ip, port, extra) == 3) {
        return 0; 
    }

    if (sscanf(line, "A %31s %15s", ip, port) != 2) {
        return 0; 
    }

    if (!is_valid_ip(ip)) {
        return 0;
    }

    if (!is_valid_port_range(port)) {
        return 0; 
    }

    int start_ip[4], end_ip[4];
    process_ip_range(ip, start_ip, end_ip);

    if (memcmp(end_ip, (int[]){0, 0, 0, 0}, sizeof(int) * 4) == 0) {
        memcpy(end_ip, start_ip, sizeof(int) * 4);
    }

    if (compare_ip(start_ip, end_ip) > 0) {
        return 0; 
    }

    if (compare_ip(start_ip, end_ip) == 0 && strchr(ip, '-') != NULL) {
        return 0; 
    }

    rule_list = add_rule(rule_list, ip, port);
    return 1; 
}

void process_args(int argc, char** argv, CmdArg* pCmd, int *port) {
    pCmd->is_interactive = false;
    if (argc == 2) {
        if (strcmp(argv[1], "-i") == 0) {
            pCmd->is_interactive = true;
        } else {
            *port = atoi(argv[1]);
            if (*port <= 0 || *port > 65535) {
                fprintf(stderr, "Invalid port number. Please enter a port number between 1 and 65535.\n");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        fprintf(stderr, "Usage: %s -i for interactive mode or %s <port> to start server\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
}

int check_ip_port(Rule* rule_list, const char* ip, int port) {
    Rule* current_rule = rule_list;
    while (current_rule) {
        if (is_ip_in_range(current_rule->ip_range, ip) && is_port_in_range(current_rule->port_range, port)) {
            current_rule->queries = add_query(current_rule->queries, ip, port);
            return 1;
        }
        current_rule = current_rule->next;
    }
    return 0;
}

int is_valid_port(int port) {
    return (port >= 0 && port <= 65535);
}

int process_check_ip(char* line) {
    char ip[32];
    int port;
    char extra[32];  

    if (strchr(line, '-') != NULL) {
        return 2; 
    }

    if (sscanf(line, "C %31s %d %31s", ip, &port, extra) == 3) {
        return 2; 
    }

    if (sscanf(line, "C %31s %d", ip, &port) != 2) {
        return 2;
    }

    if (!is_valid_ip(ip) || !is_valid_port(port)) {
        return 2; 
    }

    if (!check_ip_port(rule_list, ip, port)) {
        return 0; 
    }

    return 1;
}

int process_delete_rule(char *rule) {
    char ip[32], port[16], extra[32];
    
    if (sscanf(rule, "D %31s %15s %31s", ip, port, extra) == 3) {
        return 0; 
    }

    if (sscanf(rule, "D %31s %15s", ip, port) != 2) {
        return 0;
    }

    if (!is_valid_ip(ip) || !is_valid_port_range(port)) {
        return 0; 
    }

    Rule* current = rule_list;
    Rule* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->ip_range, ip) == 0 && strcmp(current->port_range, port) == 0) {
            if (prev == NULL) {
                rule_list = current->next; 
            } else {
                prev->next = current->next;
            }
            free(current);
            return 1; 
        }
        prev = current;
        current = current->next;
    }

    return 2; 
}

int add_rule_safe(Rule* head, char* buffer) {
    pthread_rwlock_wrlock(&rule_rwlock);

    int result = process_add_rule(buffer); 

    pthread_rwlock_unlock(&rule_rwlock);
    return result;
}

void add_command_history_safe(const char* command) {
    pthread_rwlock_wrlock(&command_history_rwlock);

    add_command(command); 

    pthread_rwlock_unlock(&command_history_rwlock);
}

void print_commands_safe(CommandHistory* head) {
    pthread_rwlock_rdlock(&command_history_rwlock);

    print_commands(head); 

    pthread_rwlock_unlock(&command_history_rwlock);
}

void print_rules_safe(Rule* head) {
    pthread_rwlock_rdlock(&rule_rwlock);

    print_rules(head); 

    pthread_rwlock_unlock(&rule_rwlock);
}

int check_ip_port_safe(Rule* rule_list, char* buffer) {
    pthread_rwlock_wrlock(&rule_rwlock);

    int result = process_check_ip(buffer);

    pthread_rwlock_unlock(&rule_rwlock);
    return result;
}

int delete_rule_safe(Rule* rule_list, char* buffer) {
    pthread_rwlock_wrlock(&rule_rwlock);

    int result = process_delete_rule(buffer); 

    pthread_rwlock_unlock(&rule_rwlock);
    return result;
}

char* get_rules_string(Rule* head) {
    if (!head) return strdup("");  

    size_t buffer_size = 1024;
    size_t total_length = 0;
    char* result = malloc(buffer_size);
    if (!result) {
        perror("malloc failed");
        return NULL;
    }
    result[0] = '\0';  // Initialize with an empty string

    Rule* current_rule = head;
    while (current_rule) {
        // Create a string for the current rule
        char rule_line[128];  // Larger buffer for each line to avoid truncation
        int rule_length = snprintf(rule_line, sizeof(rule_line), "Rule: %s %s\n",
                                   current_rule->ip_range, current_rule->port_range);

        if (total_length + rule_length >= buffer_size) {
            buffer_size *= 2;
            char* temp = realloc(result, buffer_size);
            if (!temp) {
                perror("realloc failed");
                free(result);
                return NULL;
            }
            result = temp;
        }

        strcat(result, rule_line);
        total_length += rule_length;

        Query* current_query = current_rule->queries;
        while (current_query) {
            char query_line[64];
            int query_length = snprintf(query_line, sizeof(query_line), "Query: %s %d\n",
                                        current_query->ip_address, current_query->port);

            if (total_length + query_length >= buffer_size) {
                buffer_size *= 2;
                char* temp = realloc(result, buffer_size);
                if (!temp) {
                    perror("realloc failed");
                    free(result);
                    return NULL;
                }
                result = temp;
            }

            strcat(result, query_line);
            total_length += query_length;
            current_query = current_query->next;
        }

        current_rule = current_rule->next;
    }

    return result;
}

char* get_command_history_string(CommandHistory* head) {
    if (!head) return strdup("");

    size_t buffer_size = 1024;
    size_t total_length = 0;
    char* result = malloc(buffer_size);
    if (!result) {
        perror("malloc failed");
        return NULL;
    }
    result[0] = '\0';

    CommandHistory* current = head;
    while (current) {
        size_t command_length = strlen(current->command) + 2;

        if (total_length + command_length >= buffer_size) {
            buffer_size *= 2;
            char* temp = realloc(result, buffer_size);
            if (!temp) {
                perror("realloc failed");
                free(result);
                return NULL;
            }
            result = temp;
        }

        strcat(result, current->command);
        strcat(result, "\n");

        total_length += command_length;
        current = current->next;
    }

    return result;
}

char* get_rules_string_safe() {
    pthread_rwlock_rdlock(&rule_rwlock);
    char* rules = get_rules_string(rule_list);
    pthread_rwlock_unlock(&rule_rwlock);
    return rules;
}

void *handle_client(void *arg) {
    int sock = *((int*)arg);
    free(arg);

    char buffer[4096] = {0};
    int valread;

    while ((valread = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[valread] = '\0';

        char *command = strtok(buffer, "\n");
        while (command) {
            command[strcspn(command, "\r\n")] = 0;

            if (command[0] == '\0') {
                command = strtok(NULL, "\n");
                continue;
            }

            add_command_history_safe(command);

            char firstChar = command[0];
            switch (firstChar) {
            case 'A':
                if (add_rule_safe(rule_list, command)) {
                    send(sock, "Rule added\n", strlen("Rule added\n"), 0);
                } else {
                    send(sock, "Invalid rule\n", strlen("Invalid rule\n"), 0);
                }
                break;
            case 'C':
                switch (check_ip_port_safe(rule_list, command)) {
                    case 0:
                        send(sock, "Connection rejected\n", strlen("Connection rejected\n"), 0);
                        break;
                    case 1:
                        send(sock, "Connection accepted\n", strlen("Connection accepted\n"), 0);
                        break;
                    case 2:
                        send(sock, "Illegal IP address or port specified\n", strlen("Illegal IP address or port specified\n"), 0);
                        break;
                }
                break;
            case 'R': {
                char *command_history = get_command_history_string(command_head);
                send(sock, command_history, strlen(command_history), 0);
                free(command_history);  // Free memory after sending
                break;
            }
            case 'L': {
                if (rule_list == NULL) {
                    send(sock, "\n", strlen("\n"), 0);
                } else {
                    char *rules = get_rules_string_safe();
                    send(sock, rules, strlen(rules), 0);
                    free(rules);  // Free memory after sending
                }
                break;
            }
            case 'D':
                switch (delete_rule_safe(rule_list, command)) {
                    case 0:
                        send(sock, "Rule invalid\n", strlen("Rule invalid\n"), 0);
                        break;
                    case 1:
                        send(sock, "Rule deleted\n", strlen("Rule deleted\n"), 0);
                        break;
                    case 2:
                        send(sock, "Rule not found\n", strlen("Rule not found\n"), 0);
                        break;
                }
                break;
            default:
                send(sock, "Illegal request\n", strlen("Illegal request\n"), 0);
                break;
            }

            command = strtok(NULL, "\n");
        }
        
        shutdown(sock, SHUT_WR);
        memset(buffer, 0, sizeof(buffer));
    }

    close(sock); 
    return NULL;
}

void run_server(int port) {
    int sock = 0, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port); 

    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        new_socket = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            perror("Failed to create thread");
        }

        pthread_detach(tid);
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

        str[res - 1] = '\0';
        add_command(str);

        char extra[32];
        char firstChar = str[0];
        if ((firstChar == 'L' || firstChar == 'R') && sscanf(str, "%c %31s", &firstChar, extra) == 2) {
            printf("Illegal request\n");
        } else {
            switch (firstChar) {
                case 'A':
                    if (process_add_rule(str)) {
                        printf("Rule added\n");
                    } else {
                        printf("Invalid rule\n");
                    }
                    break;
                case 'C':
                    switch (process_check_ip(str)) {
                        case 0:
                            printf("Connection rejected\n");
                            break;
                        case 1:
                            printf("Connection accepted\n");
                            break;
                        case 2:
                            printf("Illegal IP address or port specified\n");
                            break;
                    }
                    break;
                case 'R':
                    print_commands(command_head);
                    break;
                case 'L':
                    print_rules(rule_list);
                    break;
                case 'D':
                    switch (process_delete_rule(str)) {
                        case 0:
                            printf("Rule invalid\n");
                            break;
                        case 1:
                            printf("Rule deleted\n");
                            break;
                        case 2:
                            printf("Rule not found\n");
                            break;
                    }
                    break;
                default:
                    printf("Illegal request\n");
                    break;
            }
        }
        free(str);
        str = NULL;
    }
}

int main(int argc, char ** argv) {
    CmdArg cmd;
    int port = 0;
    
    process_args(argc, argv, &cmd, &port);
    
    if (cmd.is_interactive) {
        run_interactive();
    } else {
        run_server(port);
    }
    return 0;
}