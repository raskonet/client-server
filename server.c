// Actions:
//   0 = CHECK
//   1 = REQUEST_REGISTER
//   2 = REQUEST_RENEW
//   3 = ACTIVATE_REGISTER (admin)
//   4 = ACTIVATE_RENEW (admin)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define TOKEN_SIZE 16

#define S_OK 0
#define S_EXPIRED 1
#define S_NOT_FOUND 2
#define S_DB_FULL 3
#define S_AUTH_FAIL 4
#define S_NOT_PENDING 5
#define S_NOT_ACTIVE 6
#define S_ALREADY_PENDING 7
#define S_UNKNOWN -1

typedef struct {
    unsigned char token[TOKEN_SIZE];
    int is_active;
    int pending;
    time_t expiry;
} ClientRecord;

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

const unsigned char ADMIN_KEY[TOKEN_SIZE] =
    {0xde,0xad,0xbe,0xef,1,2,3,4,5,6,7,8,9,10,11,12};

ClientRecord database[MAX_CLIENTS];
int db_count = 0;
pthread_mutex_t db_lock;

static FILE *log_file = NULL;
static pthread_mutex_t log_lock;

static void log_init(void) {
    log_file = fopen("server.log", "a");
    if (!log_file) {
        perror("log open");
    } else {
        setvbuf(log_file, NULL, _IOLBF, 0);
    }
}

static void log_event(const char *fmt, ...) {
    pthread_mutex_lock(&log_lock);
    if (log_file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        if (t) {
            fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                    t->tm_year+1900, t->tm_mon+1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
        } else {
            fprintf(log_file, "[unknown-time] ");
        }

        va_list ap;
        va_start(ap, fmt);
        vfprintf(log_file, fmt, ap);
        va_end(ap);
        fprintf(log_file, "\n");
    }
    pthread_mutex_unlock(&log_lock);
}


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

static void secure_random_bytes(unsigned char *buf, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t got = read_all(fd, buf, n);
        close(fd);
        if (got == (ssize_t)n) return; 
    }
    for (size_t i = 0; i < n; ++i) {
        buf[i] = (unsigned char)(rand() & 0xFF);
    }
}

static void token_hex(const unsigned char *t, char *out) {
    for (int i=0;i<TOKEN_SIZE;i++) sprintf(out+2*i, "%02x", t[i]);
}

static int token_equal(const unsigned char *a, const unsigned char *b) {
    return memcmp(a, b, TOKEN_SIZE) == 0;
}

static ClientRecord* find_record(const unsigned char *token) {
    for (int i = 0; i < db_count; ++i) {
        if (token_equal(database[i].token, token)) return &database[i];
    }
    return NULL;
}

void *connection_handler(void *arg) {
    int sock = *(int*)arg;
    free(arg);

    while (1) {
        RequestPacket req;
        ResponsePacket res;
        memset(&res, 0, sizeof(res));
        res.status = S_UNKNOWN;
        strncpy(res.message, "Unhandled action or unauthorized", sizeof(res.message)-1);

        ssize_t rd = read(sock, &req, sizeof(req));
        if (rd <= 0) break;

        char tokhex[33] = {0};
        token_hex(req.token, tokhex);

        pthread_mutex_lock(&db_lock);
        time_t now = time(NULL);
        ClientRecord *rec = find_record(req.token);

        if (req.action == 0) {
            log_event("CHECK token=%s", tokhex);

            if (!rec) {
                res.status = S_NOT_FOUND;
                strncpy(res.message, "Not found", sizeof(res.message)-1);
            } else if (now > rec->expiry) {
                res.status = S_EXPIRED;
                strncpy(res.message, "Expired", sizeof(res.message)-1);
            } else {
                res.status = S_OK;
                res.seconds_left = (long)difftime(rec->expiry, now);
                strncpy(res.message, "Valid", sizeof(res.message)-1);
            }

            pthread_mutex_unlock(&db_lock);
            if (write_all(sock, &res, sizeof(res)) < 0) {
                log_event("write failed during CHECK for token=%s: %s", tokhex, strerror(errno));
                break;
            }
            continue;
        }

        if (req.action == 1) {
            log_event("REQUEST_REGISTER (no token provided)");

            if (db_count >= MAX_CLIENTS) {
                res.status = S_DB_FULL;
                strncpy(res.message, "DB full", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during REQUEST_REGISTER(DB full): %s", strerror(errno));
                    break;
                }
                continue;
            }

            ClientRecord *r = &database[db_count++];
            memset(r, 0, sizeof(*r));
            secure_random_bytes(r->token, TOKEN_SIZE);
            r->pending = 1;
            r->is_active = 0;
            r->expiry = 0;

            res.status = S_OK;
            strncpy(res.message, "Pending admin approval", sizeof(res.message)-1);
            memcpy(res.token, r->token, TOKEN_SIZE);

            char newhex[33] = {0};
            token_hex(r->token, newhex);
            log_event("REQUEST_REGISTER created token=%s", newhex);

            pthread_mutex_unlock(&db_lock);
            if (write_all(sock, &res, sizeof(res)) < 0) {
                log_event("write failed during REQUEST_REGISTER for token=%s: %s", newhex, strerror(errno));
                break;
            }
            continue;
        }

        if (req.action == 2) {
            log_event("REQUEST_RENEW token=%s", tokhex);

            if (!rec) {
                res.status = S_NOT_FOUND;
                strncpy(res.message, "Not found", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during REQUEST_RENEW(not found) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec->is_active) {
                res.status = S_NOT_ACTIVE;
                strncpy(res.message, "Not active; cannot renew", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during REQUEST_RENEW(not active) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (rec->pending) {
                res.status = S_ALREADY_PENDING;
                strncpy(res.message, "Already pending", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during REQUEST_RENEW(already pending) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }

            rec->pending = 1;
            res.status = S_OK;
            strncpy(res.message, "Renewal pending admin approval", sizeof(res.message)-1);
            log_event("REQUEST_RENEW pending for token=%s", tokhex);

            pthread_mutex_unlock(&db_lock);
            if (write_all(sock, &res, sizeof(res)) < 0) {
                log_event("write failed during REQUEST_RENEW(token=%s): %s", tokhex, strerror(errno));
                break;
            }
            continue;
        }

        if (req.action == 3) {
            log_event("ACTIVATE_REGISTER token=%s", tokhex);

            if (!token_equal(req.admin_key, ADMIN_KEY)) {
                res.status = S_AUTH_FAIL;
                strncpy(res.message, "Admin auth failed", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_REGISTER(auth fail) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec) {
                res.status = S_NOT_FOUND;
                strncpy(res.message, "Token not found", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_REGISTER(not found) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec->pending) {
                res.status = S_NOT_PENDING;
                strncpy(res.message, "Nothing pending to activate", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_REGISTER(not pending) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }

            rec->pending = 0;
            rec->is_active = 1;
            int days = 20 + (rand() % 11);
            rec->expiry = now + (time_t)days * 86400;

            res.status = S_OK;
            res.seconds_left = (long)days * 86400;
            strncpy(res.message, "Activated", sizeof(res.message)-1);

            log_event("ACTIVATE_REGISTER success token=%s days=%d", tokhex, days);

            pthread_mutex_unlock(&db_lock);
            if (write_all(sock, &res, sizeof(res)) < 0) {
                log_event("write failed during ACTIVATE_REGISTER(token=%s): %s", tokhex, strerror(errno));
                break;
            }
            continue;
        }

        if (req.action == 4) {
            log_event("ACTIVATE_RENEW token=%s", tokhex);

            if (!token_equal(req.admin_key, ADMIN_KEY)) {
                res.status = S_AUTH_FAIL;
                strncpy(res.message, "Admin auth failed", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_RENEW(auth fail) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec) {
                res.status = S_NOT_FOUND;
                strncpy(res.message, "Token not found", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_RENEW(not found) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec->pending) {
                res.status = S_NOT_PENDING;
                strncpy(res.message, "Nothing pending to renew", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_RENEW(not pending) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }
            if (!rec->is_active) {
                res.status = S_NOT_ACTIVE;
                strncpy(res.message, "Not active; cannot renew", sizeof(res.message)-1);
                pthread_mutex_unlock(&db_lock);
                if (write_all(sock, &res, sizeof(res)) < 0) {
                    log_event("write failed during ACTIVATE_RENEW(not active) token=%s: %s", tokhex, strerror(errno));
                    break;
                }
                continue;
            }

            rec->pending = 0;
            int days = 20 + (rand() % 11);
            rec->expiry = now + (time_t)days * 86400;

            res.status = S_OK;
            res.seconds_left = (long)days * 86400;
            strncpy(res.message, "Renewed and activated", sizeof(res.message)-1);

            log_event("ACTIVATE_RENEW success token=%s days=%d", tokhex, days);

            pthread_mutex_unlock(&db_lock);
            if (write_all(sock, &res, sizeof(res)) < 0) {
                log_event("write failed during ACTIVATE_RENEW(token=%s): %s", tokhex, strerror(errno));
                break;
            }
            continue;
        }

        log_event("UNKNOWN action=%d token=%s", req.action, tokhex);
        pthread_mutex_unlock(&db_lock);
        if (write_all(sock, &res, sizeof(res)) < 0) {
            log_event("write failed during UNKNOWN action write: %s", strerror(errno));
            break;
        }
    }

    close(sock);
    log_event("Connection closed");
    return NULL;
}

int main() {
    srand((unsigned)time(NULL));

    if (pthread_mutex_init(&db_lock, NULL) != 0) {
        fprintf(stderr, "Mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&log_lock, NULL) != 0) {
        fprintf(stderr, "Log mutex init failed\n");
        return 1;
    }

    log_init();
    log_event("Server starting on port %d", PORT);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

    printf("Server listening on %d\n", PORT);
    log_event("Server listening on %d", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            log_event("accept failed: %s", strerror(errno));
            continue;
        }

        int *pclient = malloc(sizeof(int));
        if (!pclient) { close(client_fd); log_event("malloc failed for client fd"); continue; }
        *pclient = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, connection_handler, pclient) != 0) {
            log_event("pthread_create failed: %s", strerror(errno));
            free(pclient);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    pthread_mutex_destroy(&db_lock);
    pthread_mutex_destroy(&log_lock);
    if (log_file) fclose(log_file);
    return 0;
}

