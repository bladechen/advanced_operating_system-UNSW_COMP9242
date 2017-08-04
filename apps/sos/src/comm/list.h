/*
    copied from linux kernel, doubly linked list

    modified for intrusive list, which used in kernel global open file table

    Copyright (C) Shenglong Chen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Shenglong Chen
email: chenshenglong1990@gmail.com

*/

#include "comm/comm.h"
#ifndef _LIST_H_
#define _LIST_H_
// #include <types.h>
// #include <kern/errno.h>
// #include <lib.h>
#define __builtin_prefetch(x,y,z) (void)1


/*
linked meta structure
*/
struct list_head {
    struct list_head *next, *prev;
    // struct list* owner; // the list belong to
};

/*
the link head of the list
*/
struct list {
    struct list_head head;
    int offset; /* used to locate the list_head in linked struct, should be init before calling any other functions */

};


#if 0
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)
#endif

#define INIT_LIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a p entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *p,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = p;
    p->next = next;
    p->prev = prev;
    prev->next = p;
}

/**
 * list_add - add a p entry
 * @p: p entry to be added
 * @head: list head to add it after
 *
 * Insert a p entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *p, struct list_head *head)
{
    __list_add(p, head, head->next);
}
/**
 * list_add_tail - add a p entry
 * @p: p entry to be added
 * @head: list head to add it before
 *
 * Insert a p entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *p, struct list_head *head)
{
    __list_add(p, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}
// void __list_del(struct list_head *prev, struct list_head *next);

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
void list_reset(struct list_head *link);

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}
/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

static inline void __list_splice(struct list_head *list,
                                 struct list_head *head)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;
    struct list_head *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the p list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the p list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
                                    struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head);
        INIT_LIST_HEAD(list);
    }
}

#ifndef offsetof
#define offsetof(type, member) (unsigned long)(&((type *)0)->member)
#endif



/**
 * list_entry - get the struct for this entry
 * @ptr:    the &struct list_head pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-offsetof(type, member)))
/*
// container_of is same as list_entry
// #ifndef container_of
// #define container_of(ptr, type, member) ({ \
//         const typeof( ((type *)0)->member ) *__mptr = (ptr); \
//         (type *)( (char *)__mptr - offsetof(type,member) );})
// #endif
*/

/**
 * list_for_each    -   iterate over a list
 * @pos:    the &struct list_head to use as a loop counter.
 * @head:   the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next, __builtin_prefetch(pos->next,0,1); \
     pos != (head); \
     pos = pos->next, __builtin_prefetch(pos->next,0,1)) \

#define __list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)
/**
 * list_for_each_prev   -   iterate over a list backwards
 * @pos:    the &struct list_head to use as a loop counter.
 * @head:   the head for your list.
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev, __builtin_prefetch(pos->prev,0,1); \
     pos != (head); \
     pos = pos->prev, __builtin_prefetch(pos->prev,0,1))

#define __list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); \
            pos = pos->prev)

/**
 * list_for_each_safe   -   iterate over a list safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop counter.
 * @n:      another &struct list_head to use as temporary storage
 * @head:   the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

/**
 * list_for_each_entry  -   iterate over list of given type
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member),  \
         __builtin_prefetch(pos->member.next,0,1);          \
         &pos->member != (head);                    \
         pos = list_entry(pos->member.next, typeof(*pos), member),  \
         __builtin_prefetch(pos->member.next,0,1))


// more stupid way than entry->member.
// static inline struct list_head* list_get_link_from_entry(struct list *l, void *entry)
// {
//       return (struct list_head*) ((size_t) entry + l->offset);
// }
//
// // same as list_entry
// static inline void* list_get_entry_from_link(struct list_head* link)
// {
//     if (link == NULL)
//     {
//         return NULL;
//     }
//
//     return (void*) ((size_t) link - link->owner->offset);
// }



/**
 * list_for_each_entry  -   iterate over list of given type, can safely detach the entry
 *                          from list within the loop.
 * @pos:    the pointer of struct list_head* to use as a loop counter.
 * @n:      tm pointer of struct list_head*
 * @l:      the list pointer
 */
#define list_for_each_entry_safe(pos, n, l)\
     for ( pos = ((l)->head.next == &((l)->head) ? NULL:list_get_entry_from_link((l)->head.next)), \
                  (n) = (pos == NULL?n: *list_get_link_from_entry(l, pos)); \
          pos != NULL;\
          pos = ((n).next == &((l)->head)?NULL:list_get_entry_from_link(n.next)),  \
                  (n) = (pos == NULL?n: *list_get_link_from_entry(l, pos)))



/**
 * link_detach
 *
 * @cur:    the entry need to be detached.
 * @member: the name of the list_struct within the struct.
 *
 */
#define link_detach(cur, member) do{\
    list_reset(&((cur)->member));\
} while(0);

struct list* create_list();
void list_init(struct list*);

void destroy_list(struct list* );
bool is_list_empty(const struct list* );
void make_list_empty(struct list *); /*  the caller should destroy all the linked entry, this function is simply detach all the entrys*/

// void list_insert_head(struct list *l, void* entry);
// void list_insert_tail(struct list *l, void* entry);

void* list_head(struct list* l);
// void* list_tail(struct list* l);



/*
 * operations for linked entry
 */
void link_init(struct list_head* );
struct list_head* link_next(struct list_head*);
bool is_linked(struct list_head* );



// void link_detach1(void* entry, struct list* l); //  list * l is only for get the struct list* in void* entry
// void link_detach2(void* entry, int offset); //  lis








#endif


// struct  list my_list;
// int main()
// {
//     INIT_LIST_HEAD(&(my_list.head));
//     my_list.offset = offsetof(struct entry , object);
//
    /* struct entry* p, v; */
    /* attach(&my_list, v); */
    /* dettach(&my_list, v); */
    /* void* s = p + 1; */
    /* size_t ss = (size_t) p + 1; */
//     return 0;
// }
