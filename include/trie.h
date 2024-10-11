#include <stdlib.h>
#include <stdint.h>
#include "lib.h"
#include <arpa/inet.h>

//  Struct for a node
typedef struct node {
	struct route_table_entry *entry;

	struct node *left, *right;
} TNode, *TTrie;

//  Trie useful functions
TTrie create_node();
void trie_insert(TTrie trie, struct route_table_entry *entry);
struct route_table_entry *trie_search(TTrie trie, uint32_t ip);