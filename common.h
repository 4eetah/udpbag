#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define BUFSIZE 4096
#define BAGSIZE 16

#define myperror(err) do { \
    perror(err); exit(1);\
} while(0)

#define mylog_exit(fmt, ...) do { \
    fprintf(stderr, fmt"\n", ##__VA_ARGS__); exit(1); \
} while(0)

#define mylog_note(fmt, ...) do { \
    fprintf(stderr, fmt"\n", ##__VA_ARGS__); \
} while(0)

typedef enum {
    ONE,
    TWO,
}MSG_TYPE;

struct message_t {
    uint16_t size;
    uint8_t type;
    uint64_t id;
    uint64_t data;
};

struct hash_entry {
    uint32_t hash;
    struct hash_entry *next;
    struct message_t value;
};

struct hash_table {
    pthread_mutex_t hmutex;
    size_t size;
    struct hash_entry **table;
    void *entries;
    struct hash_entry *empty_entry;
};
