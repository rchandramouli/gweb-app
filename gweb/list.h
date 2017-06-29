#ifndef LIST_H
#define LIST_H

struct list {
    struct list *next, *prev;
};

static inline void list_init (struct list *head)
{
    head->next = head->prev = head;
}

static inline void list_add (struct list *head, struct list *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

static inline void list_remove (struct list *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node->next = node;
}

#endif // LIST_H
