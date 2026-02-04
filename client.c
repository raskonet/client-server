#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

typedef struct {
    int id;
    char username[32];
    int action;        //0 =check,1=register,3=quit
} RequestPacket;

typedef struct {
    int status;
    long seconds_left;
    char message[64];
} ResponsePacket;

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    //ipv4 to ipv6
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to License Manager Server\n");
    printf("{---------------------------------------}\n");

    while(1) {
        RequestPacket req;
        ResponsePacket res;

        printf("\nLicense Manager\n");
        printf("1.Register New User\n");
        printf("2. Check License Status\n");
        printf("3. Exit\n");
        printf("Select Action(0==>login,1==>register,3==>quit): ");
        
        int choice;
        scanf("%d", &choice);

        if (choice == 3) {
            printf("Goodbye!\n");
            break;
        }

        printf("Enter ID (0-99): ");
        scanf("%d", &req.id);

        if (choice == 1) {
            req.action = 1;
            printf("Enter Username: ");
            scanf("%s", req.username);
        } else {
            req.action = 0;
            strcpy(req.username, "");
        }

        send(sock, &req, sizeof(RequestPacket), 0);
        read(sock, &res, sizeof(ResponsePacket));

        printf("\nServer Response----\n");
        printf("Message: %s\n", res.message);
        if (res.status == 0) {
            int days = res.seconds_left / (24 * 3600);
            printf("Time Remaining: %ld seconds (%d days)\n", res.seconds_left, days);
        }
    }

    close(sock);
    return 0;
}
