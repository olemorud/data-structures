#pragma once

#include <stdint.h>
#include <stddef.h>

#include "arena.h"

struct critbit_tree {
    union critbit_node* root;
    struct arena*       arena;
};

struct critbit_iterator {
    union critbit_node* root;
    struct {
        size_t cap;
        size_t len;
        struct cb_iter_book {
            union critbit_node* node;
            bool                visited[2];
        } items[];
    } stack;
};

bool critbit_contains(struct critbit_tree* cb, const void* data, size_t size);

int critbit_insert(struct critbit_tree* cbt, const void* data, size_t size);

int critbit_remove(struct critbit_tree* cbt, const void* data, size_t size);

void print_node_data(int (*printf_function)(const char*, ...), union critbit_node* node, int depth, int indent);
