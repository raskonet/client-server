#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define TOKEN_SIZE 16

typedef struct {
    unsigned char token[TOKEN_SIZE];
    int action;
    unsigned char admin_key[TOKEN_SIZE];
} RequestPacket;

typedef struct {
    int status;
    long seconds_left;
    unsigned char token[TOKEN_SIZE];
    char message[64];
} ResponsePacket;

unsigned char my_token[TOKEN_SIZE];
int has_token = 0; 

void save_token() {
    FILE *f = fopen("license.key", "wb");
    if (!f) return;
    fwrite(my_token, 1, TOKEN_SIZE, f);
    fclose(f);
    has_token = 1;
}

void load_token() {
    FILE *f = fopen("license.key", "rb");
    if (!f) {
        has_token = 0;
        memset(my_token, 0, TOKEN_SIZE);
        return;
    }
    if (fread(my_token, 1, TOKEN_SIZE, f) == TOKEN_SIZE) {
        has_token = 1;
    } else {
        has_token = 0;
    }
    fclose(f);
}

void print_hex_token() {
    if (!has_token) {
        printf("(No token found)\n");
        return;
    }
    for(int i=0; i<TOKEN_SIZE; i++) printf("%02x", my_token[i]);
    printf("\n");
}

int main() {
    load_token();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { 
        perror("Connection Failed. Is the server running?"); 
        return 1; 
    }

    printf("--- Connected to License Server ---\n");
    if (has_token) {
        printf("License File Found. Token: ");
        print_hex_token();
    } else {
        printf("No License File Found. Please Register.\n");
    }

    while (1) {
        printf("\n1. Register (Request New License)\n");
        printf("2. Check License Status\n");
        printf("3. Request Renewal\n");
        printf("4. Exit\n");
        printf("Select: ");
        
        int c; 
        if (scanf("%d", &c) != 1) break;

        RequestPacket req;
        ResponsePacket res;
        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));

        if (c == 1) {
            req.action = 1;
            if (write(sock, &req, sizeof(req)) <= 0) break;
            if (read(sock, &res, sizeof(res)) <= 0) break;
            
            printf("[Server] %s (Status: %d)\n", res.message, res.status);
            
            if (res.status == 0) {
                memcpy(my_token, res.token, TOKEN_SIZE);
                save_token();
                printf(">> Token saved to disk. Share this with Admin to activate(this would actually be sent to admin by payment service in actual imeplmentation):\n>> ");
                print_hex_token();
            }
        } 
        else if (c == 2) {
            if (!has_token) {
                printf("Error: No license token found. Register first.\n");
                continue;
            }
            req.action = 0;
            memcpy(req.token, my_token, TOKEN_SIZE);
            
            if (write(sock, &req, sizeof(req)) <= 0) break;
            if (read(sock, &res, sizeof(res)) <= 0) break;
            
            printf("[Server] %s (Status: %d)\n", res.message, res.status);
            if (res.status == 0) {
                int days = res.seconds_left / (24*3600);
                printf(">> Validity: %d days remaining.\n", days);
            }
        } 
        else if (c == 3) {
            if (!has_token) {
                printf("Error: No license token found.\n");
                continue;
            }
            req.action = 2;
            memcpy(req.token, my_token, TOKEN_SIZE);
            
            if (write(sock, &req, sizeof(req)) <= 0) break;
            if (read(sock, &res, sizeof(res)) <= 0) break;
            
            printf("[Server] %s (Status: %d)\n", res.message, res.status);
        } 
        else {
            break;
        }
    }

    close(sock);
    return 0;
}
