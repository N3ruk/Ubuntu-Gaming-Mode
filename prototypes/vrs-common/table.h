/* Intrusive, separately chained handle map. Caller holds its device lock.
 * Reserve tracking before changing Vulkan state: OOM must never lose an
 * injected dynamic state. Growth failure preserves the valid old table. */
#ifndef UGM_VRS_TABLE_H
#define UGM_VRS_TABLE_H
#include <stdint.h>
typedef struct Node { uint64_t key; struct Node *next; } Node;
typedef struct Table { Node **buckets; size_t capacity, count; } Table;
static size_t handle_hash(uint64_t key) {
    key ^= key >> 33; key *= UINT64_C(0xff51afd7ed558ccd);
    key ^= key >> 33; key *= UINT64_C(0xc4ceb9fe1a85ec53);
    return (size_t)(key ^ (key >> 33));
}
static int table_init(Table *t) {
    t->capacity=64; t->count=0; t->buckets=calloc(t->capacity,sizeof(*t->buckets));
    return t->buckets != NULL;
}
static Node *table_find(const Table *t, uint64_t key) {
    if (!t->buckets) return NULL;
    for(Node *n=t->buckets[handle_hash(key)&(t->capacity-1)]; n; n=n->next)
        if(n->key==key) return n;
    return NULL;
}
static void table_insert(Table *t, Node *node) {
    if(t->count >= t->capacity*2 && t->capacity < (1u<<20)) {
        size_t capacity=t->capacity*2;
        Node **b=calloc(capacity,sizeof(*b));
        if(b) {
            for(size_t i=0;i<t->capacity;++i) for(Node *n=t->buckets[i],*next;n;n=next) {
                next=n->next; size_t j=handle_hash(n->key)&(capacity-1); n->next=b[j]; b[j]=n;
            }
            free(t->buckets); t->buckets=b; t->capacity=capacity;
        }
    }
    size_t j=handle_hash(node->key)&(t->capacity-1);
    node->next=t->buckets[j]; t->buckets[j]=node; ++t->count;
}
static void table_remove(Table *t, uint64_t key) {
    if(!t->buckets) return;
    Node **p=&t->buckets[handle_hash(key)&(t->capacity-1)];
    while(*p) {
        if((*p)->key==key) { Node *old=*p; *p=old->next; free(old); --t->count; return; }
        p=&(*p)->next;
    }
}
static void table_clear(Table *t) {
    for(size_t i=0;t->buckets && i<t->capacity;++i) {
        Node *p=t->buckets[i]; while(p) { Node *next=p->next; free(p); p=next; }
    }
    free(t->buckets); *t=(Table){0};
}
#define HANDLE_KEY(h) ((uint64_t)(uintptr_t)(h))
/* Non-dispatchable handles are uint64_t on 32-bit, not uintptr_t! */
#if !VK_USE_64_BIT_PTR_DEFINES
#undef HANDLE_KEY
#define HANDLE_KEY(h) ((uint64_t)(h))
#endif
#endif
