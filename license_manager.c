#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define TOKEN_SIZE 16
#define SERVER_IP "127.0.0.1"
#define RENEWAL_THRESHOLD_DAYS 30

// DPI Export
#ifdef __cplusplus
extern "C" {
#endif 

int check_license_dpi();

#ifdef __cplusplus
}
#endif

// ============================================================================
// DATA STRUCTURES
// ============================================================================

#pragma pack(push, 1)
typedef struct {
    unsigned char token[TOKEN_SIZE];
    int action; // 1=Register, 2=Renew, 0=Check
    unsigned char admin_key[TOKEN_SIZE];
} RequestPacket;

typedef struct {
    int status;
    long seconds_left;
    unsigned char token[TOKEN_SIZE];
    char message[64];
} ResponsePacket;
#pragma pack(pop)

static unsigned char my_token[TOKEN_SIZE];
static int has_token = 0; 
static const char *LICENSE_FILE = "license.key";

static void wait_for_enter() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static int get_user_selection() {
    char buffer[64];
    printf("   Enter option > ");
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        return atoi(buffer);
    }
    return 0;
}

static ssize_t read_all(int fd, void *buf, size_t count) {
    unsigned char *p = (unsigned char*)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

static ssize_t write_all(int fd, const void *buf, size_t count) {
    const unsigned char *p = (const unsigned char*)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t n = write(fd, p + total, count - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

void save_token() {
    FILE *f = fopen(LICENSE_FILE, "wb");
    if (!f) return;
    fwrite(my_token, 1, TOKEN_SIZE, f);
    fclose(f);
    has_token = 1;
}

void load_token() {
    FILE *f = fopen(LICENSE_FILE, "rb");
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

static int connect_to_server(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return -1;
    
    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);
  
    if(connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0){
        close(sock);
        return -1;
    }
    return sock;
}


static int handle_register(){
    printf("\n[Register] > Connecting to server..\n");
    int sock = connect_to_server();
    if(sock < 0){
        printf("[Error] > Could not connect to server.\n");
        return 0;
    }

    RequestPacket req = {0};
    ResponsePacket res = {0};
    req.action = 1; 

    if (write_all(sock, &req, sizeof(req)) != sizeof(req) ||
        read_all(sock, &res, sizeof(res)) != sizeof(res)) {
        printf("[Error] > Communication failed.\n");
        close(sock);
        return 0;
    }
    close(sock);

    printf("[Server] > %s\n", res.message);
    if (res.status == 0) {
        memcpy(my_token, res.token, TOKEN_SIZE);
        save_token();
        printf("[Success] > New license registered and saved.\n");
        return 1;
    }
    return 0;
}

static int handle_renew(long current_seconds_left){
    int days_left = current_seconds_left / 86400;

    if (current_seconds_left > 0 && days_left > RENEWAL_THRESHOLD_DAYS) {
        printf("\n[Info] > Your license is valid for %d more days.\n", days_left);
        printf("       > You cannot renew until you have less than %d days remaining.\n", RENEWAL_THRESHOLD_DAYS);
        printf("       > Press Enter to return...");
        wait_for_enter();
        return 0;
    }

    printf("\n[Renew] > Requesting renewal from server...\n");
    int sock = connect_to_server();
    if (sock < 0) {
        printf("[Error] > Server unreachable.\n");
        return 0;
    }

    RequestPacket req = {0};
    ResponsePacket res = {0};
    req.action = 2; // RENEW
    memcpy(req.token, my_token, TOKEN_SIZE);

    if (write_all(sock, &req, sizeof(req)) != sizeof(req) ||
        read_all(sock, &res, sizeof(res)) != sizeof(res)) {
        printf("[Error] > Communication failed.\n");
        close(sock);
        return 0;
    }
    close(sock);

    printf("[Server] > %s\n", res.message);
    if (res.status == 0) {
        printf("[Success] > Renewal request submitted. Contact admin for activation.\n");
        printf("          > Press Enter to return...");
        wait_for_enter();
        return 1;
    }
    return 0;
}


int check_license_dpi() {
    load_token();
    
    while (1) {
        int license_valid = 0;
        int license_expired = 0;
        long seconds_remaining = 0;

        if (has_token) {
            int sock = connect_to_server();
            if (sock >= 0) {
                RequestPacket req = {0};
                ResponsePacket res = {0};
                req.action = 0; 
                memcpy(req.token, my_token, TOKEN_SIZE);

                if (write_all(sock, &req, sizeof(req)) == sizeof(req) &&
                    read_all(sock, &res, sizeof(res)) == sizeof(res)) {
                    
                    if (res.status == 0) { 
                        license_valid = 1;
                        seconds_remaining = res.seconds_left;
                    } else if (res.status == 1) {
                        license_expired = 1;
                        seconds_remaining = 0;
                    } else {
                        has_token = 0; 
                    }
                }
                close(sock);
            } else {
                printf("[Warning] > Cannot reach server to verify license.\n");
            }
        }

        if (license_valid) {
            int days = seconds_remaining / 86400;
            if (days > RENEWAL_THRESHOLD_DAYS) {
                printf("[License] > \033[0;32mVALID\033[0m (%d days remaining). Simulation starting.\n", days);
                return 1;
            }
        }

        printf("\n");
        printf("╔════════════════════════════════════════════════════════╗\n");
        if (!has_token) {
            printf("║   ERROR: NO LICENSE FOUND                              ║\n");
            printf("║   Please register to obtain a license key.             ║\n");
        } else if (license_expired) {
            printf("║   ERROR: LICENSE EXPIRED                               ║\n");
            printf("║   Your license has expired. Please renew.              ║\n");
        } else if (license_valid) { 
            int days = seconds_remaining / 86400;
            printf("║   WARNING: LICENSE EXPIRING SOON (%d days left)      ║\n", days);
            printf("║   You may continue or request renewal now.             ║\n");
        }
        printf("╚════════════════════════════════════════════════════════╝\n");
        printf("\nSelect Action:\n");

        if (!has_token) {
            printf("   [1] Register New License\n");
        } else {
            printf("   [1] Request License Renewal\n"); 
        }

        if (license_valid) {
            printf("   [2] Continue Simulation (Ignore Warning)\n");
        }
        
        printf("   [3] Abort Simulation\n");
        printf("\n");

        int choice = get_user_selection();

        if (choice == 1) {
            if (!has_token) {
                if (handle_register()) {
                    continue; 
                }
            } else {
                handle_renew(seconds_remaining);
                continue; 
            }
        } 
        else if (choice == 2 && license_valid) {
            printf("[License] > Proceeding with simulation.\n");
            return 1;
        } 
        else if (choice == 3) {
            printf("[Abort] > Stopping simulation.\n");
            return 0;
        } 
        else {
            printf("[Error] > Invalid selection.\n");
        }
    }
}
