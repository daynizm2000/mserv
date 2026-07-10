#include <mserv.h>
#include <sys/mman.h>

#define OBJ_SIZE_ALIGNMENT 8
#define OBJ_COUNT_DEFAULT 32

int mobj_pool_add_chunk(struct mobject_pool *pool)
{
        size_t obj_count;
        size_t offset;
        struct mobject_chunk *chunk;
        struct mobject_node *mnode;
        struct mobject_node *old_first_free;
        
        chunk = mmap(NULL, pool->chunk_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (chunk == MAP_FAILED)
                mserv_err_ret(-1, "Memory mapped error\n");

        if (pool->chunks_head)
                mlist_add(&pool->chunks_head->list, &chunk->list);
        else {
                pool->chunks_head = chunk;
                mlist_head_init(&chunk->list);
        }

        chunk->area = (unsigned char*)chunk + sizeof(struct mobject_chunk);
        old_first_free = pool->first_free;
        pool->first_free = chunk->area;

        obj_count = (pool->chunk_size - sizeof(struct mobject_chunk)) / pool->obj_size;
        offset = pool->obj_size;
        mnode = pool->first_free;

        for (size_t i = 1; i < obj_count; i++) {
                struct mobject_node *node = (struct mobject_node*)((unsigned char*)chunk->area + offset);
                
                mnode->next = node;
                mnode = node;

                offset += pool->obj_size;
        }

        mnode->next = old_first_free;
        pool->total_alloc += pool->chunk_size;
        
        return 0;
}

int mobj_pool_init(struct mobject_pool *pool, size_t obj_size, size_t obj_count)
{
        if (!obj_count)
                obj_count = OBJ_COUNT_DEFAULT;

        memset(pool, 0, sizeof(struct mobject_pool));
        pool->obj_size = ((obj_size + OBJ_SIZE_ALIGNMENT - 1) / OBJ_SIZE_ALIGNMENT) * OBJ_SIZE_ALIGNMENT;
        pool->chunk_size = obj_count * pool->obj_size + sizeof(struct mobject_chunk);

        return mobj_pool_add_chunk(pool);
}

void *mobj_pool_alloc(struct mobject_pool *pool)
{
        void *retval;

        if (!pool->first_free)
                if (mobj_pool_add_chunk(pool) < 0)
                        return NULL;

        retval = pool->first_free;
        pool->first_free = pool->first_free->next;

        return retval;
}

void mobj_pool_free(struct mobject_pool *pool, void *ptr)
{
        struct mobject_node *node;

        memset(ptr, 0, pool->obj_size);
        node = ptr;

        node->next = pool->first_free;
        pool->first_free = node;
}

void mobj_pool_destroy(struct mobject_pool *pool)
{
        if (!pool->chunks_head)
                return;

        struct mlist_head *l = pool->chunks_head->list.next;

        do {
                struct mobject_chunk *chunk = container_of(l, struct mobject_chunk, list);
                l = chunk->list.next;
                
                munmap(chunk, pool->chunk_size);
        } while (l != &pool->chunks_head->list);

        munmap(pool->chunks_head, pool->chunk_size);
        memset(pool, 0, sizeof(struct mobject_pool));
}