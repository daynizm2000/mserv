#include <mserv.h>

void mlist_head_init(struct mlist_head *head)
{
        head->next = head;
        head->prev = head;
}

void mlist_add(struct mlist_head *head, struct mlist_head *el)
{
        el->next = head->next->next;
        el->prev = head;
        head->next->next->prev = el;
        head->next = el;
}

void mlist_del(struct mlist_head *el)
{
        el->prev->next = el->next;
        el->next->prev = el->prev;
        el->next = NULL;
        el->prev = NULL;
}