#include <mserv.h>
#include <stdlib.h>
#include <string.h>

#define HASH_NUM 5381
#define DEFAULT_CAPACITY 32

static size_t mserv_hm_hash(const char *key, size_t ksize)
{
        size_t hash = HASH_NUM;
    
        for (size_t i = 0; i < ksize; i++) {
                hash = ((hash << 5) + hash) + (size_t)(unsigned char)key[i];
        }
        
        return hash;
}

int mserv_hm_init(struct mserv_hashmap *map, size_t capacity)
{
        if (!capacity)
                capacity = DEFAULT_CAPACITY;

        map->buckets = calloc(capacity, sizeof(mserv_hm_node_t*));
        if (!map->buckets)
                mserv_err_ret(-1, "Memory allocation error\n");

        map->capacity = capacity;
        map->count = 0;

        return 0;
}

static int mserv_hm_rehash(struct mserv_hashmap *map, size_t newcap)
{
        mserv_hm_node_t **new_buckets = calloc(newcap, sizeof(mserv_hm_node_t*));
        if (!new_buckets)
                mserv_err_ret(-1, "Memory allocation error\n");
        
        for (size_t i = 0; i < map->capacity; i++) {
                struct mlist_head *l;
                struct mlist_head *end;
                mserv_hm_node_t *node;
                
                node = map->buckets[i];
                if (!node)
                        continue;

                l = &node->list;
                end = &node->list;

                do {
                        struct mlist_head *next = l->next;
                        mserv_hm_node_t *el = container_of(l, mserv_hm_node_t, list);
                        size_t new_idx = mserv_hm_hash(el->key, el->ksize) % newcap;

                        if (!new_buckets[new_idx]) {
                                mlist_head_init(&el->list);
                                new_buckets[new_idx] = el;
                        }
                        else {
                                mlist_add(&new_buckets[new_idx]->list, &el->list);
                        }

                        l = next;
                } while (l != end);
        }

        free(map->buckets);
        map->buckets = new_buckets;
        map->capacity = newcap;

        return 0;
}

static mserv_hm_node_t *mserv_hm_node_create(void *key, size_t ksize, void *val,
        void (*free_callback)(void *key, size_t ksize, void *val))
{
        mserv_hm_node_t *node = malloc(sizeof(mserv_hm_node_t));
        if (!node)
                mserv_err_ret(NULL, "Memory allocation error\n");

        node->key = malloc(ksize);
        if (!node->key) {
                free(node);
                mserv_err_ret(NULL, "Memory allocation error\n");
        }

        memcpy(node->key, key, ksize);
        node->ksize = ksize;
        node->value = val;
        node->free_callback = free_callback;
        mlist_head_init(&node->list);

        return node;
}

int mserv_hm_add(struct mserv_hashmap *map, void *key, size_t ksize, void *val,
        void (*free_callback)(void *key, size_t ksize, void *val))
{
        mserv_hm_node_t *node;
        size_t idx;

        if (map->count > map->capacity * 0.75) {
                if (mserv_hm_rehash(map, map->capacity * 2) < 0)
                        return -1;
        }

        idx = mserv_hm_hash(key, ksize) % map->capacity;
        node = map->buckets[idx];

        if (!node) {
                node = mserv_hm_node_create(key, ksize, val, free_callback);
                if (!node)
                        return -1;

                map->buckets[idx] = node;
                map->count++;
        }
        else {
                struct mlist_head *l = &node->list;
                mserv_hm_node_t *el;
                int found = 0;

                do {
                        el = container_of(l, mserv_hm_node_t, list);

                        if (el->ksize == ksize && memcmp(el->key, key, ksize) == 0) {
                                el->value = val;
                                el->free_callback = free_callback;

                                found = 1;
                                break;
                        }

                        l = l->next;
                } while (l != &node->list);

                if (!found) {
                        el = mserv_hm_node_create(key, ksize, val, free_callback);
                        if (!el)
                                return -1;

                        mlist_add(&node->list, &el->list);
                        map->count++;
                }
        }

        return 0;
}

void mserv_hm_node_free(mserv_hm_node_t *node)
{
        mlist_del(&node->list);

        if (node->free_callback)
                node->free_callback(node->key, node->ksize, node->value);

        free(node);
        node = NULL;
}

void mserv_hm_delete(struct mserv_hashmap *map, void *key, size_t ksize)
{
        mserv_hm_node_t *node;
        struct mlist_head *l;
        size_t idx;

        if (!map || !key || !ksize)
                return;

        idx = mserv_hm_hash(key, ksize) % map->capacity;
        node = map->buckets[idx];
        if (!node)
                return;

        l = &node->list;

        do {
                mserv_hm_node_t *el = container_of(l, mserv_hm_node_t, list);

                if (ksize == el->ksize && memcmp(key, el->key, ksize) == 0) {
                        mserv_hm_node_free(el);
                        map->count--;
                        map->buckets[idx] = NULL;
                        break;
                }

                l = l->next;
        } while (l != &node->list);
}

void *mserv_hm_find(struct mserv_hashmap *map, void *key, size_t ksize)
{
        mserv_hm_node_t *node;
        struct mlist_head *l;

        node = map->buckets[mserv_hm_hash(key, ksize) % map->capacity];
        if (!node)
                return NULL;

        l = &node->list;

        do {
                mserv_hm_node_t *el = container_of(l, mserv_hm_node_t, list);

                if (ksize == el->ksize && memcmp(key, el->key, ksize) == 0)
                        return el->value;

                l = l->next;
        } while (l != &node->list);

        return NULL;
}

void mserv_hm_destroy(struct mserv_hashmap *map)
{
        if (map->count) {
                for (size_t i = 0; i < map->capacity; i++) {
                        mserv_hm_node_t *node = map->buckets[i];

                        if (!node)
                                continue;

                        for (struct mlist_head *l = node->list.next; l != &node->list; l = l->next) {
                                mserv_hm_node_t *el = container_of(l, mserv_hm_node_t, list);
                                l = l->next;

                                mserv_hm_node_free(el);
                        }
                }
        }

        free(map->buckets);
        map->capacity = 0;
        map->count = 0;
}