
//#define XCAT(a, b) a##b
//#define CAT(a, b) XCAT(a,b)
//
//#ifdef BTREE_VAL
//	#ifndef BTREE_PREFIX
//		#error "BTREE_VAL defined but not BTREE_PREFIX"
//	#endif
//    #define T BTREE_VAL
//    #define BTREE_T       CAT(Hashmap_,T)
//    #define BTREE_NODE_T CAT(hashmap_entry_,T)
//#else
//    #define T void*
//    #define BTREE_T      BTree
//	#define BTREE_PREFIX btree
//    #define BTREE_NODE_T btree_node
//#endif

#include <stddef.h>

#include "config.h"

#define STR(x) #x

#define BTREE_ORDER (CACHE_LINE_SIZE/(sizeof(void*) + sizeof(int)))

typedef struct BTree_node {
	int keys[BTREE_ORDER-1];
	
	// Whether a child is a leaf or internal node is determined by the height
	// of the tree, since all leaf nodes are on the same level.
	union {
		struct BTree_node* node;
		void* leaf;
	} children[BTREE_ORDER];
} BTree_node;

typedef struct BTree {
	size_t height;
	struct BTree_node root;
} BTree;


