
//#define XCAT(a, b) a##b
//#define CAT(a, b) XCAT(a,b)
//
//#ifdef BTREE_VAL
//  #ifndef BTREE_PREFIX
//      #error "BTREE_VAL defined but not BTREE_PREFIX"
//  #endif
//    #define T BTREE_VAL
//    #define BTREE_T       CAT(Hashmap_,T)
//    #define BTREE_NODE_T CAT(hashmap_entry_,T)
//#else
//    #define T void*
//    #define BTREE_T      BTree
//  #define BTREE_PREFIX btree
//    #define BTREE_NODE_T btree_node
//#endif

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "arena.h"

#define STR(x) #x
#define ARRAY_LEN(x) (sizeof x) / sizeof (*(x))

typedef uint64_t Key;
#define KeyFmt PRIu64

#define MAX_CHILDREN (2*(CACHE_LINE_SIZE/(sizeof(void*) + sizeof (Key))))
#define MAX_KEY (MAX_CHILDREN-1)

//#define arena_alloc(arena, size) aligned_alloc(CACHE_LINE_SIZE, size)

#define unlikely(expr) __builtin_expect(expr, 0)
#define likely(expr) __builtin_expect(expr, 1)

typedef struct BTree_node {
    uint8_t degree;
    uint8_t is_leaf;
    Key keys[MAX_KEY];
    void* children[MAX_CHILDREN];
} __attribute__((aligned(CACHE_LINE_SIZE))) BTree_node;

//static_assert(sizeof (BTree_node) <= CACHE_LINE_SIZE);

typedef struct BTree {
    struct BTree_node* root;
    size_t             depth;
    size_t             node_count;
    struct arena*      arena;
} BTree;

static inline size_t node_key_count(BTree_node* node)
{
    return node->degree;
}

static inline size_t node_children_count(BTree_node* node)
{
    return node->degree + 1;
}

static void print_indent(int n)
{
    for (int i = 0; i < n; i++) {
        putchar(' ');
    }
}

static void print_node_(BTree_node* node, int depth)
{
    print_indent(depth);
    if (node == NULL) {
        printf("NULL");
        return;
    }

    printf("{");
    for (size_t i = 0; i < node_key_count(node) - 1; i++) {
        printf("%"KeyFmt", ", node->keys[i]);
    }
    printf("%zu}", node->keys[node_key_count(node) - 1]);

    if (!node->is_leaf) {
        printf("{");
        for (unsigned int i = 0; i < node_children_count(node) - 1U; i++) {
            printf("\n");
            print_node_((BTree_node*)node->children[i], depth+4);
            printf(",");
        }
        printf("\n");
        print_node_((BTree_node*)node->children[node_children_count(node) - 1], depth+4);
        printf("\n");
        print_indent(depth);
        printf("}");
    }
}

static inline void print_node(BTree_node* node)
{
    print_node_(node, 0);
}

static void split_child(BTree* btree, BTree_node* parent, size_t i, BTree_node* child)
{
    BTree_node* new_child = arena_alloc(btree->arena, sizeof *new_child);
    if (unlikely(!new_child)) {
        abort();
    }

    memcpy(new_child->keys, &(child->keys[MAX_KEY/2+1]), (MAX_KEY/2) * sizeof *new_child->keys);
    if (!child->is_leaf) {
        memcpy(new_child->children, &(child->children[MAX_CHILDREN/2]), (MAX_CHILDREN/2) * sizeof *new_child->children);
    }

    new_child->degree = child->degree = MAX_CHILDREN/2 - 1;
    new_child->is_leaf = child->is_leaf;

    /* insert new child to this parent */
    memmove(&(parent->children[i+2]), &(parent->children[i+1]), (node_children_count(parent) - i - 1) * sizeof parent->children[0]);
    memmove(&(parent->keys[i+1]), &(parent->keys[i]), (node_key_count(parent) - i) * sizeof parent->keys[0]);
    parent->keys[i] = child->keys[MAX_KEY/2];
    parent->children[i+1] = new_child;
    parent->degree++;

    btree->node_count++;
}

/* TODO: SIMD optimize*/
static inline size_t lower_bound(BTree_node* node, Key k)
{
    size_t i;
    for (i = 0; i < node_key_count(node) && node->keys[i] < k; i++)
        /*noop*/;
    return i;
}

static bool _BTree_insert(BTree* btree, BTree_node* node, Key key)
{
    size_t i = lower_bound(node, key);

    if (unlikely(i < node_key_count(node) && node->keys[i] == key)) {
        return false;
    }

    if (unlikely(node->is_leaf)) {
        if (i <= node_key_count(node)) {
            const size_t key_move_size = (node_key_count(node) - i) * sizeof node->keys[0];
            const size_t children_move_size = (node_children_count(node) - i) * sizeof node->children[0];
            memmove(&(node->keys[i+1]), &(node->keys[i]), key_move_size);
            memmove(&(node->children[i+1]), &(node->children[i]), children_move_size);
            node->keys[i] = key;
        }

        node->degree += 1;
    } else {
        BTree_node* child = node->children[i];
        // we are about to descend to a child, split the child if it's full before descending
        if (unlikely(node_children_count(child) == MAX_CHILDREN)) {
            split_child(btree, node, i, child);
            if (key > node->keys[i]) {
                i++;
                child = node->children[i];
            }
        }
        return _BTree_insert(btree, child, key);
    }

    return true;
}

bool BTree_insert(BTree* b, const Key key)
{
    if (unlikely(node_children_count(b->root) == MAX_CHILDREN)) {
        BTree_node* new_root = arena_alloc(b->arena, sizeof *new_root);

        if (unlikely(!new_root)) {
            abort();
        }

        *new_root = (BTree_node) {
            .degree = 0,
            .is_leaf = false,
            .children[0] = b->root,
        };

        split_child(b, new_root, 0, b->root);
        b->root = new_root;
        b->depth += 1;
    }
    return _BTree_insert(b, b->root, key);
}

Key random_key(uint64_t n) {
    const Key PRIME = 0x9e3779b97f4a7c15ULL;  // a 64-bit odd constant (golden ratio scaled)
    n = (n ^ (n >> 30)) * PRIME;
    n = (n ^ (n >> 27)) * PRIME;
    n = n ^ (n >> 31);
    return (Key)n;
}

int main()
{
    printf("sizeof(BTree_node): %zu\n", sizeof(BTree_node));
    printf("CACHE_LINE_SIZE: %zu\n", CACHE_LINE_SIZE);
    printf("MAX_CHILDREN: %"KeyFmt"\n", MAX_CHILDREN);

    {
        struct arena a = arena_new();
        BTree_node* node = arena_alloc(&a, sizeof *node);
        if (unlikely(!node)) {
            abort();
        }
        *node = (BTree_node){
            .keys = {0},
            .is_leaf = true,
            .degree = 0,
        };
        BTree btree = {
            .root = node,
            .arena = &a,
            .node_count = 0,
            .depth = 0,
        };

        volatile bool ok;
        const uint64_t insert_ceil = 16 * 1024 * 1024;
        int64_t insert_count = 0;
        for (uint64_t n = 0; n < insert_ceil; n++) {
            insert_count += BTree_insert(&btree, random_key(n));
            (void)ok;
        }

        //print_node(btree.root);

        printf("arena allocated:   %zu\n", a.size);
        printf("items inserted:    %zu\n", insert_count);
        printf("depth:             %zu\n", btree.depth);
        printf("nodes:             %zu\n", btree.node_count);
        printf("items per node:    %.1lf\n", (double)insert_count / (double)btree.node_count);
        printf("overhead per item: %.1lf%%\n", 100*((double)a.size / (double)insert_count) / (double)sizeof(Key));
    }

    return EXIT_SUCCESS;
}
