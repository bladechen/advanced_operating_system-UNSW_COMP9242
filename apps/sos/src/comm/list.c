#include "list.h"

struct list* create_list()
{
    struct list* l = (struct list*)malloc(sizeof(struct list));
    if (l == NULL)
    {
        return NULL;
    }
    link_init(&(l->head));
    return l;
}


void list_init(struct list* l)
{
    link_init(&(l->head));
    return;
}

bool is_list_empty(const struct list* l)
{
    assert(l != NULL);
    return list_empty(&(l->head));
}
void destroy_list(struct list* l)
{
    assert(is_list_empty(l));
    free(l);
    return;
}
void make_list_empty(struct list * l)
{
    struct list_head* pos = NULL;
    struct list_head* tmp = NULL;
    list_for_each_safe(pos, tmp, &(l->head))
    {
        if (pos == NULL)
        {
            break;
        }
        list_reset(pos);
    }
    return;
}
struct list_head* link_next(struct list_head* link)
{
    assert(link != NULL);
    return link->next;
}


void link_init(struct list_head* link)
{
    INIT_LIST_HEAD(link);
    return;
}
bool is_linked(struct list_head* link)
{
    return !(link->next == link->prev && (link->next == link ||link->next == NULL) );

}
void list_reset(struct list_head *entry)
{
    list_del_init(entry);
}
