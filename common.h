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
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <byteswap.h>

#define BAGSIZE 16
#define QUEUESIZE 16
#define SNDBUFSIZE 65536

#define myperror(err) do { \
    perror(err); exit(1);\
} while(0)

#define mylog_exit(fmt, ...) do { \
    fprintf(stderr, fmt"\n", ##__VA_ARGS__); exit(1); \
} while(0)

#define mylog_note(fmt, ...) do { \
    fprintf(stderr, fmt"\n", ##__VA_ARGS__); \
} while(0)

#define hton16(val) ((endian == LITTLE) ? htons(val) : (val))
#define hton32(val) ((endian == LITTLE) ? htonl(val) : (val))
#define hton64(val) ((endian == LITTLE) ? bswap_64(val) : (val))

#define ntoh16(val) ((endian == LITTLE) ? ntohs(val) : (val))
#define ntoh32(val) ((endian == LITTLE) ? ntohl(val) : (val))
#define ntoh64(val) ((endian == LITTLE) ? bswap_64(val) : (val))

typedef enum {
    ONE,
    TWO,
}MSG_TYPE;
typedef enum {
    LITTLE,
    BIG,
}ENDIAN;
extern int endian;

struct message_t {
    uint16_t size;
    uint8_t type;
    uint64_t id;
    uint64_t data;
} __attribute__((packed));

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

int setendian();
void msgserialize(struct message_t *msg);
void msgdeserialize(struct message_t *msg);
void msgprint(struct message_t *msg);
