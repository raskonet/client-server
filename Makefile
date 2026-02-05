CC = gcc

CFLAGS  = -Wall -Wextra -Werror -std=c99 -g -pthread
LDFLAGS = -pthread

BIN_DIR = bin

TARGETS = server client admin

all: $(BIN_DIR) $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

server: server.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/server server.c $(LDFLAGS)

client: client.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/client client.c $(LDFLAGS)

admin: admin.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/admin admin.c $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

run-server: server
	./$(BIN_DIR)/server

run-client: client
	./$(BIN_DIR)/client

run-admin: admin
	./$(BIN_DIR)/admin

.PHONY: all clean run-server run-client run-admin

