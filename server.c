#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h> 
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define LICENSE_DAYS 0.001

typedef struct {
    int id;
    char username[32];
    time_t expiry_timestamp;
    int is_active;
} ClientRecord;

typedef struct {
    int id;
    char username[32]; 
    int action;        //0=login or check 1=register
} RequestPacket;

typedef struct {
    int status;        //0=success,1=expired,2=not found,3=already exists
    long seconds_left;
    char message[64];
} ResponsePacket;

// 100*100 peoplw Database
ClientRecord database[MAX_CLIENTS];
//to aviod race conditions adding a Mutex
pthread_mutex_t db_lock; 

void *connection_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc); 

    while(1) {
        RequestPacket req;
        ResponsePacket res;
        memset(&res, 0, sizeof(res));

        int bytes_read = read(sock, &req, sizeof(RequestPacket));
        
        if (bytes_read <= 0) {
            break;
        }

        pthread_mutex_lock(&db_lock);
        time_t now = time(NULL);
        int id = req.id;

        if (id < 0 || id >= MAX_CLIENTS) {
            res.status = 2;
            strcpy(res.message, "Invalid ID Range");
        } 
        else if (req.action == 1) { 
            if (database[id].is_active) {
                res.status = 3;
                strcpy(res.message, "User already registered");
            } else {
                database[id].id = id;
                strcpy(database[id].username, req.username);
                database[id].is_active = 1;
                // expiry set to 20 Days
                database[id].expiry_timestamp = now + (LICENSE_DAYS * 24 * 60 * 60);
                
                res.status = 0;
                strcpy(res.message, "Registration Successful");
                res.seconds_left = LICENSE_DAYS * 24 * 60 * 60;
                
                printf("[LOG] Registered User: %s at ID: %d\n", req.username, id);
            }
        } 
        else { 
            if (!database[id].is_active) {
                res.status = 2;
                strcpy(res.message, "User not found. Please Register.");
            } else if (now > database[id].expiry_timestamp) {
                res.status = 1;
                strcpy(res.message, "License Expired. Please Renew.");
                res.seconds_left = 0;
            } else {
                res.status = 0;
                snprintf(res.message, 64, "Welcome back, %s", database[id].username);
                res.seconds_left = (long)difftime(database[id].expiry_timestamp, now);
            }
        }

        pthread_mutex_unlock(&db_lock);
        
        write(sock, &res, sizeof(ResponsePacket));
    }

    close(sock);
    printf("[LOG]Connection closed\n");
    return 0;
}

int main() {
    int server_fd, *new_sock;
    struct sockaddr_in address;
    //int addrlen = sizeof(address);
    if (pthread_mutex_init(&db_lock, NULL) != 0) {
        printf("\n Mutex init has failed\n");
        return 1;
    }

    //init db
    memset(database, 0, sizeof(database));

    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

  
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on Port %d (Multi-threaded)...\n", PORT);

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[LOG]Connection accepted:\n");

        
        pthread_t sniffer_thread;
        new_sock = malloc(1); 
        *new_sock = client_fd;

        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*) new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
            return 1;
        }
        
      
        pthread_detach(sniffer_thread);
    }

    return 0;
}
