#include "common.h"

#define he_idx(i) \
    (((struct hash_entry*)hashtbl->entries)+i)

uint32_t hash64(uint64_t a) 
{
    a = a ^ (a>>32);
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return (uint32_t)a; 
}

int hashtbl_init(struct hash_table *hashtbl, size_t size)
{
    pthread_mutex_init(&hashtbl->hmutex, NULL);

    hashtbl->size = size;

    if ((hashtbl->table = calloc(size, sizeof(struct hash_entry*))) == NULL)
        return -1;

    if ((hashtbl->entries = calloc(size, sizeof(struct hash_entry))) == NULL)
        return -1;

    hashtbl->empty_entry = hashtbl->entries;

    int i;
    for (i = 0; i < size-1; ++i) {
        he_idx(i)->next = he_idx(i+1);
    }

    return 0;
}

int hashtbl_put(struct hash_table *hashtbl, struct message_t *msg)
{
    pthread_mutex_lock(&hashtbl->hmutex);
    if (!hashtbl->empty_entry) {
        pthread_mutex_unlock(&hashtbl->hmutex);
        return 0;
    }

    struct hash_entry *hentry, *phe, **pphe;
    size_t idx;

    hentry = hashtbl->empty_entry;
    hentry->hash = hash64(msg->id);
    hashtbl->empty_entry = hashtbl->empty_entry->next;
    hentry->next = NULL;
    memcpy(&hentry->value, msg, sizeof(*msg));
   
    idx = hentry->hash % hashtbl->size;
    for (pphe = hashtbl->table+idx; (phe = *pphe);) {
        if (hentry->hash == phe->hash) {
            *pphe = phe->next;
            phe->next = hashtbl->empty_entry;
            hashtbl->empty_entry = phe;
        } else {
            pphe = &phe->next;
        }
    }
    hentry->next = hashtbl->table[idx];
    hashtbl->table[idx] = hentry;

    pthread_mutex_unlock(&hashtbl->hmutex);
    return 1;
}

int hashtbl_get(struct hash_table *hashtbl, uint64_t id, struct message_t *val)
{
    struct hash_entry **pphe, *phe;
    uint32_t hash;
    size_t idx;

    hash = hash64(id);
    idx = hash % hashtbl->size;

    pthread_mutex_lock(&hashtbl->hmutex);
    for (pphe = hashtbl->table+idx; (phe = *pphe);) {
        if (phe->hash == hash) {
            memcpy(val, &phe->value, sizeof(*val));
            pthread_mutex_unlock(&hashtbl->hmutex);
            return 1;
        } else {
            pphe = &phe->next;
        }
    }
    pthread_mutex_unlock(&hashtbl->hmutex);
    return 0;
}

void hashtbl_print(struct hash_table *hashtbl)
{
    int i;
    struct hash_entry *entry;
    struct message_t *msg;

    printf("table size: %lu\n", hashtbl->size);
    for (i = 0; i < hashtbl->size; ++i){
        entry = hashtbl->table[i];
        if (entry){
            //printf("bucket %d:\n", i);
            while (entry) {
                msg = &entry->value;
                printf("msg| size:%d, type:%d, id:%llu, data:%llu\n", 
                        msg->size,msg->type,(unsigned long long)msg->id,(unsigned long long)msg->data);
                entry=entry->next;
            }
        }
    }
}
#undef he_idx
