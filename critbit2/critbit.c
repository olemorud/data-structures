
#include "critbit.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* CRITBIT NODES
 * =============
 * A critbit node is either an internal node or a leaf (which contains raw data ,e.g. strings).
 * The least significant bit is used to tag internal nodes, and allocations must therefore be
 * guaranteed to be aligned for portability.
 * `is_leaf()`, `is_internal_node()`, `untag_critbit_node_ptr()` and
 * `tag_critbit_node_ptr()` are helper functions for this purpose.
 * */
union critbit_node {

    struct critbit_internal_node {
        union critbit_node* child[2];
        uint32_t            crit_byte;
        uint8_t             otherbits;
    } node;

    struct critbit_leaf {
        size_t  size;
        uint8_t data[];
    } leaf;
};

enum : uintptr_t {
    CRITBIT_NODE_BIT = 1ULL,
    CRITBIT_TAG_BITS = CRITBIT_NODE_BIT,
};

static inline bool is_leaf(const union critbit_node* n)
{
    return !((uintptr_t)n & CRITBIT_NODE_BIT);
}

static inline bool is_internal_node(const union critbit_node* n)
{
    return !!((uintptr_t)n & CRITBIT_NODE_BIT);
}

static inline union critbit_node* untag_critbit_node_ptr(const union critbit_node* tagged_ptr) {
    return (union critbit_node*)((uintptr_t)tagged_ptr & ~(uintptr_t)CRITBIT_TAG_BITS);
}

static inline union critbit_node* tag_critbit_node_ptr(const union critbit_node* tagged_ptr, uintptr_t tags) {
    return (union critbit_node*)((uintptr_t)tagged_ptr | (uintptr_t)tags);
}

/* For consistency with unix syscalls, zero is used to indicate success and
 * non-zero is used to indicate failure */
enum : int {
    OK = 0,
    FAIL = 1,
};

/* Wrapper for memory allocations. Allocation backends must return aligned
 * pointers */
static void* cbt_alloc(struct critbit_tree* cbt, size_t size)
{
    assert(cbt);
    return arena_alloc(cbt->arena, size);
}

/* Recursively prints contents of a critbit node */
void print_node_data(int (*printf_function)(const char*, ...), union critbit_node* node, int depth, int indent)
{
#define print_indent(n) for (int i = 0; i < n; i++) {printf_function(" ");}
#define newline() do {printf_function("\n"); print_indent(indent);} while (0)
    newline();
    if (node == NULL) {
        printf_function("NULL");
        return;
    }
    if (is_leaf(node)) {
        printf_function("\"%s\"", node->leaf.data);
    } else {
        struct critbit_internal_node* inode = &(untag_critbit_node_ptr(node)->node);
        printf_function("{");
        indent += 4;
        newline();

        printf_function(".crit_byte: %zu,", inode->crit_byte);
        newline();
        printf_function(".otherbits: 0x%x,", inode->otherbits);
        newline();
        printf_function(".children: [");
        if (depth > 0) {
            if (inode->child[0]) {
                print_node_data(printf_function, inode->child[0], depth-1, indent+4);
            }
            printf_function(",");

            if (inode->child[1]) {
                print_node_data(printf_function, inode->child[1], depth-1, indent+4);
            }
            printf_function("");

            newline();
            printf_function("]");

            indent -= 4;
            newline();
            printf_function("}");
        } else {
            printf_function("...}}");
        }
    }
#undef print_indent
#undef newline
}

/* finds leaf node that best matches (data, size)  */
static struct critbit_leaf* critbit_walk(union critbit_node* cb, const void* data, size_t size)
{
    const uint8_t* udata = data;
    while ((uintptr_t)cb & CRITBIT_NODE_BIT) {
        // cb is node from here on
        struct critbit_internal_node* node = &(untag_critbit_node_ptr(cb)->node);

        // calculate direction
        uint8_t ch = 0;
        if (node->crit_byte < size) {
            ch = udata[node->crit_byte];
        }
        const size_t direction = (1 + (node->otherbits | ch)) >> 8;

        assert(direction == 0 || direction == 1);

        cb = node->child[direction];
    }

    return &(cb->leaf);
}

/* checks if critbit tree contains (data, size) */
bool critbit_contains(struct critbit_tree* cbt, const void* data, size_t size)
{
    if (!cbt || cbt->root == NULL) {
        return NULL;
    }
    struct critbit_leaf* leaf = critbit_walk(cbt->root, data, size);
    return (size == leaf->size) && memcmp(leaf->data, data, size) == 0;
}

static union critbit_node* new_leaf(struct critbit_tree* cbt, const void* data, size_t size)
{
    assert(cbt);
    union critbit_node* x = cbt_alloc(cbt, sizeof (x->leaf) + size);
    if (x == NULL) {
        return NULL;
    }
    memset(x, 0, sizeof (x->leaf) + size);
    memcpy(x->leaf.data, data, size);
    x->leaf.size = size;
    return x;
}

/* caller must remember to tag the pointer */
static union critbit_node* new_internal_node(struct critbit_tree* cbt, size_t crit_byte, uint8_t otherbits)
{
    union critbit_node* x = cbt_alloc(cbt, sizeof *x);
    if (x == NULL) {
        return NULL;
    }
    memset(x, 0, sizeof *x);
    x->node.crit_byte = crit_byte;
    x->node.otherbits = otherbits;
    return x;
}

static void delete_node(struct critbit_tree* cbt, union critbit_node* n)
{
    if (cbt->arena != NULL) {
        return;
    }
    free(n);
}

/* returns a bitmask that indicates the first bit (from LSB to MSB) that is not equal*/
static uint8_t mask_first_different_bit(uint8_t a, uint8_t b)
{
    assert(a != b);
    return (1 << __builtin_ctz(a ^ b));
}   

/* returns the index of the first byte in `a` and `b` that is not equal.
 * If a and b are equal, FAIL is returned. Otherwise OK. */
static int calculate_critbit(const uint8_t* a, size_t a_size, const uint8_t* b, size_t b_size, size_t* byte_index, uint8_t* bitmask)
{
    const size_t min = MIN(a_size, b_size);
    size_t i;
    for (i = 0; i < min; i++) {
        if (a[i] != b[i]) {
            break;
        }
    }
    *byte_index = i;
    *bitmask = ~mask_first_different_bit(a[i], b[i]);

    return OK;
}

int critbit_insert(struct critbit_tree* cbt, const void* data, size_t size)
{
    if (cbt->root == NULL) {
        return (cbt->root = new_leaf(cbt, data, size)) == NULL;
    }

    /* First walk existing tree to find critical byte and critical bit */
    const uint8_t* udata = data;
    const struct critbit_leaf* leaf = critbit_walk(cbt->root, data, size);
    /* find critical byte / bit */

    size_t new_crit_byte;
    uint8_t new_otherbits;
    if (calculate_critbit(leaf->data, leaf->size, data, size, &new_crit_byte, &new_otherbits)) {
        return FAIL;
    }
    const uint8_t ch = ((uint8_t*)leaf->data)[new_crit_byte];
    const size_t  new_direction = (1 + (new_otherbits | ch)) >> 8;

    /* Once again walk to find best leaf to split based on crit-byte and crit-bit
     *
     * existing leaf becomes one of
     *  
     *              node          |        node
     *            /      \        |       /     \
     *  existing_leaf  new_leaf   |  new_leaf  existing_leaf
     * */
    union critbit_node** wherep = &(cbt->root);
    while (1) {
        const union critbit_node* p = *wherep;
        if (is_leaf(p)) {
            break;
        }
        
        struct critbit_internal_node* node = &(untag_critbit_node_ptr(p)->node);
        if (node->crit_byte > new_crit_byte) {
            break;
        }
        if (node->crit_byte == new_crit_byte && node->otherbits > new_otherbits) {
            break;
        }
        uint8_t ch = 0;
        if (node->crit_byte < size) {
            ch = ((uint8_t*)data)[node->crit_byte];
        }
        
        const size_t direction = (1 + (node->otherbits | ch)) >> 8;
        wherep = &(node->child[direction]);
    }

    {
        union critbit_node* new_node = new_internal_node(cbt, new_crit_byte, new_otherbits);
        if (new_node == NULL) {
            return FAIL;
        }
        new_node->node.child[new_direction] = *wherep;
        union critbit_node* leaf = new_leaf(cbt, data, size);
        if (!leaf) {
            if (cbt->arena == NULL) {
                free(new_node);
            }
            return FAIL;
        }
        new_node->node.child[1-new_direction] = leaf;
        *wherep = tag_critbit_node_ptr(new_node, CRITBIT_NODE_BIT);
    }

    return OK;
}

#include <stdio.h>
int critbit_remove(struct critbit_tree* cbt, const void* data, size_t size)
{
    if (cbt->root == NULL) {
        return FAIL;
    }

    /* - Walk the tree for the best match */
    const uint8_t* u8data = data;
    
    union critbit_node* p = cbt->root;
    union critbit_node** wherep = &(cbt->root);
    union critbit_node** whereq = NULL;
    struct critbit_internal_node* q = NULL;
    size_t direction = 0;

    while (is_internal_node(p)) {
        whereq = wherep;
        q = &(untag_critbit_node_ptr(p)->node);
        uint8_t ch = 0;
        if (q->crit_byte < size) {
            ch = u8data[q->crit_byte];
        }
        direction = (1 + (q->otherbits | ch)) >> 8;
        wherep = &(q->child[direction]);
        p = *wherep;
    }

    /* - Orphan the node to be removed. Free'ing of p and q is done via cbt's
     *   arena */
    /* p is guaranteed to be a leaf at this point */
    struct critbit_leaf* pleaf = &(p->leaf);

    if (pleaf->size != size || memcmp(pleaf->data, data, size) != 0) {
        return FAIL;
    }

    /* there _is_ no grandparent node, i.e. tree only has one element. Make tree empty */
    if (!whereq) {
        cbt->root = NULL;
        return OK;
    }

    *whereq = q->child[1-direction];
    
    return OK;
}

static inline struct cb_iter_book* cb_iter_stack_push(struct critbit_iterator* iter, union critbit_node* n)
{
    iter->stack.items[iter->stack.len].node = n;
    iter->stack.items[iter->stack.len].visited[0] = false;
    iter->stack.items[iter->stack.len].visited[1] = false;
    return &iter->stack.items[iter->stack.len++];
}

static inline struct cb_iter_book* cb_iter_stack_pop(struct critbit_iterator* iter)
{
    return &iter->stack.items[--iter->stack.len];
}

static inline struct cb_iter_book* cb_iter_stack_top(struct critbit_iterator* iter)
{
    return &iter->stack.items[iter->stack.len];
}

static struct critbit_leaf* critbit_iterator_next(struct critbit_iterator* iter)
{
    struct cb_iter_book* t = cb_iter_stack_top(iter);
    if (is_leaf(t->node)) {
        t = cb_iter_stack_pop(iter);
        return &t->node->leaf;
    }
    if (is_internal_node(t->node)) {
        while (t->visited[0] && t->visited[1]) {
            t = cb_iter_stack_pop(iter);
        }
        while (is_internal_node(t->node)) {
            for (size_t direction = 0; direction < 2; direction++) {
                if (!t->visited[direction]) {
                    t->visited[direction] = true;
                    t = cb_iter_stack_push(iter, t->node->node.child[direction]);
                    break;
                }
            }
        }
        return &t->node->leaf;
    }
}

struct critbit_iterator critbit_allprefixed(struct critbit_tree* cbt, const void* data, size_t size)
{
    const uint8_t* u8data = data;

    if (!cbt || !(cbt->root)) {
        return (struct critbit_iterator){.root = NULL};
    }

    union critbit_node* p = cbt->root;
    union critbit_node* top = p;

    while (is_internal_node(p)) {
        struct critbit_internal_node* q = &(untag_critbit_node_ptr(p)->node);
        uint8_t ch = 0;
        if (q->crit_byte < size) {
            ch = u8data[q->crit_byte];
        }
        const size_t direction = (1 + (ch | q->otherbits)) >> 8;
        p = q->child[direction];
        if (q->crit_byte < size) top = p;
    }

    /* p can't be an internal node at this point */
    {
        const struct critbit_leaf* leaf = &(p->leaf);
        double p;

        if (memcmp(leaf->data, data, size) == 0) {
            return (struct critbit_iterator){.root = NULL};
        }

        return (struct critbit_iterator){.root = top};
    }
}
