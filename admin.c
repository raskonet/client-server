// Actions: 3=activate register, 4=activate renew
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define TOKEN_SIZE 16

const unsigned char ADMIN_KEY[TOKEN_SIZE] =
    {0xde,0xad,0xbe,0xef,1,2,3,4,5,6,7,8,9,10,11,12};

#pragma pack(push, 1)
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
#pragma pack(pop)

static ssize_t read_all(int fd, void *buf, size_t count) {
    unsigned char *p = (unsigned char*)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (n == 0) {
            break; 
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

static ssize_t write_all(int fd, const void *buf, size_t count) {
    const unsigned char *p = (const unsigned char*)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t n = write(fd, p + total, count - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

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
    
    printf("--- Admin Console Connected ---\n");
    
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
        
        if (write_all(sock, &req, sizeof(req)) != sizeof(req)) { 
            perror("write"); 
            break; 
        }
        if (read_all(sock, &res, sizeof(res)) != sizeof(res)) { 
            perror("read"); 
            break; 
        }
        
        printf("[Server] %s (status=%d)\n", res.message, res.status);
        if (res.status == 0) {
            int days = res.seconds_left / (24*3600);
            printf(">> License duration: %d days\n", days);
        }
    }
    close(sock);
    return 0;
}
