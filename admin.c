// Actions: 3=activate register, 4=activate renew

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define TOKEN_SIZE 16

const unsigned char ADMIN_KEY[TOKEN_SIZE] =
    {0xde,0xad,0xbe,0xef,1,2,3,4,5,6,7,8,9,10,11,12};

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

int hexchar2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

int parse_hex_token(const char *hex, unsigned char *out) {
    int len = strlen(hex);
    if (len != TOKEN_SIZE*2) return -1;
    for (int i = 0; i < TOKEN_SIZE; ++i) {
        int hi = hexchar2int(hex[2*i]);
        int lo = hexchar2int(hex[2*i+1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) { perror("connect"); return 1; }

    while (1) {
        printf("\nEnter token hex (32 hex chars) or 'q' to quit: ");
        char hex[64];
        if (scanf("%63s", hex) != 1) break;
        if (hex[0] == 'q') break;

        unsigned char token[TOKEN_SIZE];
        if (parse_hex_token(hex, token) != 0) {
            printf("Invalid hex token\n");
            continue;
        }

        printf("1 = Activate registration, 2 = Activate renewal: ");
        int choice; if (scanf("%d", &choice) != 1) break;

        RequestPacket req;
        ResponsePacket res;
        memset(&req, 0, sizeof(req));
        memset(&res, 0, sizeof(res));

        memcpy(req.token, token, TOKEN_SIZE);
        memcpy(req.admin_key, ADMIN_KEY, TOKEN_SIZE);
        if (choice == 1) req.action = 3;
        else req.action = 4;

        if (write(sock, &req, sizeof(req)) <= 0) { perror("write"); break; }
        if (read(sock, &res, sizeof(res)) <= 0) { perror("read"); break; }

        printf("[Server] %s (status=%d)\n", res.message, res.status);
    }

    close(sock);
    return 0;
}

