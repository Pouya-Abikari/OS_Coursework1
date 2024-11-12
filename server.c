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

pthread_mutex_t rule_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t command_history_mutex = PTHREAD_MUTEX_INITIALIZER;

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

int is_ip_in_range(const char *ip_range, const char *target_ip) {
    char start_ip[16] = {0};
    char end_ip[16] = {0};

    char *dash_pos = strchr(ip_range, '-');
    if (dash_pos) {
        strncpy(start_ip, ip_range, dash_pos - ip_range);
        start_ip[dash_pos - ip_range] = '\0';
        strcpy(end_ip, dash_pos + 1);
    } else {
        strcpy(start_ip, ip_range);
        strcpy(end_ip, ip_range);
    }

    int start_ip_array[4], end_ip_array[4], target_ip_array[4];
    parse_ip(start_ip, start_ip_array);
    parse_ip(end_ip, end_ip_array);
    parse_ip(target_ip, target_ip_array);

    for (int i = 0; i < 4; i++) {
        if (target_ip_array[i] < start_ip_array[i]) {
            return 0;
        }
        if (target_ip_array[i] > start_ip_array[i]) {
            break; 
        }
    }

    for (int i = 0; i < 4; i++) {
        if (target_ip_array[i] > end_ip_array[i]) {
            return 0; 
        }
        if (target_ip_array[i] < end_ip_array[i]) {
            break;
        }
    }

    return 1;
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
    int result = 0;
    pthread_mutex_lock(&rule_mutex);

    result = process_add_rule(buffer); 

    pthread_mutex_unlock(&rule_mutex);
    return result;
}

void add_command_history_safe(const char* command) {
    pthread_mutex_lock(&command_history_mutex);

    add_command(command); 

    pthread_mutex_unlock(&command_history_mutex);
}

void print_rules_safe(Rule* head) {
    pthread_mutex_lock(&rule_mutex);

    print_rules(head); 

    pthread_mutex_unlock(&rule_mutex);
}

int check_ip_port_safe(Rule* rule_list, char* buffer) {
    int result = 0;
    pthread_mutex_lock(&rule_mutex);

    result = process_check_ip(buffer);

    pthread_mutex_unlock(&rule_mutex);
    return result;
}

int delete_rule_safe(Rule* rule_list, char* buffer) {
    int result = 0;
    pthread_mutex_lock(&rule_mutex);

    result = process_delete_rule(buffer); 

    pthread_mutex_unlock(&rule_mutex);
    return result;
}

int capture_output(void (*func)(void*), void* arg, char* buffer, size_t buffer_size) {
    int saved_stdout = dup(STDOUT_FILENO);
    int out_pipe[2];
    pipe(out_pipe);
    dup2(out_pipe[1], STDOUT_FILENO);
    close(out_pipe[1]);

    func(arg);

    fflush(stdout);
    read(out_pipe[0], buffer, buffer_size - 1);
    buffer[buffer_size - 1] = '\0'; 

    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    close(out_pipe[0]);

    return strlen(buffer);
}

void *handle_client(void *arg) {
    int sock = *((int*)arg);
    free(arg);

    char buffer[1024] = {0};
    char response[4096] = {0};  
    int valread;

    while ((valread = read(sock, buffer, 1024)) > 0) {
        buffer[valread] = '\0';

        char *command = strtok(buffer, "\n");
        while (command != NULL) {
            add_command_history_safe(command);

            char firstChar = command[0];
            switch (firstChar) {
                case 'A':
                    if (add_rule_safe(rule_list, command)) {
                        snprintf(response, sizeof(response), "Rule added\n");
                    } else {
                        snprintf(response, sizeof(response), "Invalid rule\n");
                    }
                    break;
                case 'C':
                    switch (check_ip_port_safe(rule_list, command)) {
                        case 0:
                            snprintf(response, sizeof(response), "Connection rejected\n");
                            break;
                        case 1:
                            snprintf(response, sizeof(response), "Connection accepted\n");
                            break;
                        case 2:
                            snprintf(response, sizeof(response), "Illegal IP address or port specified\n");
                            break;
                    }
                    break;
                case 'R':
                    capture_output((void(*)(void*))print_commands, command_head, response, sizeof(response));
                    break;
                case 'L':
                    capture_output((void(*)(void*))print_rules_safe, rule_list, response, sizeof(response));
                    break;
                case 'D':
                    switch (delete_rule_safe(rule_list, command)) {
                        case 0:
                            snprintf(response, sizeof(response), "Rule invalid\n");
                            break;
                        case 1:
                            snprintf(response, sizeof(response), "Rule deleted\n");
                            break;
                        case 2:
                            snprintf(response, sizeof(response), "Rule not found\n");
                            break;
                    }
                    break;
                default:
                    snprintf(response, sizeof(response), "Illegal request\n");
                    break;
            }
            send(sock, response, strlen(response), 0);
            memset(response, 0, sizeof(response)); 

            command = strtok(NULL, "\n");
        }
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

        add_command(str);

        char firstChar = str[0];
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

        free(str);
        str = NULL;
    }
}

int main(int argc, char ** argv) {
    /* to be written */
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
