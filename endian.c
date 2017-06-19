#include "common.h"

int endian;

int setendian()
{
    short val = 0x0001;
    char c = *(char*)&val;
    if (c == 0x01)
        endian = LITTLE;
    else
        endian = BIG;
}

void msgserialize(struct message_t *msg)
{
    msg->size = hton16(msg->size);
    msg->id = hton64(msg->id);
    msg->data = hton64(msg->data);
}

void msgdeserialize(struct message_t *msg)
{
    msg->size = ntoh16(msg->size);
    msg->id = ntoh64(msg->id);
    msg->data = ntoh64(msg->data);
}

void msgprint(struct message_t *msg)
{
    printf("size: %u, type: %u, id: %llu, data: %llu\n",
            msg->size, msg->type, (unsigned long long)msg->id, (unsigned long long)msg->data);
}
