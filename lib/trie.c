#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "trie.h"

//  Function to allocate memory for a node
TTrie create_node() {
	TTrie node = malloc(sizeof(TNode));
	DIE(node == NULL, "memory");

	node->left = NULL;
	node->right = NULL;

	return node;
}

//  Function that reverses bits of number
uint32_t reverse_bits(uint32_t n)
{
    uint32_t rev = 0;
    while (n > 0) {
        rev <<= 1;
 
        if ((n & 1) == 1)
            rev ^= 1;
 
        n >>= 1;
    }
 
    return rev;
}

//  Helper function for tail recursion
void trie_insert_helper(TTrie trie, struct route_table_entry *entry, uint32_t prefix, uint32_t mask) {
	if (mask == 0) {
		trie->entry = entry;
		return;
	}

	TTrie next_node = NULL;
	int child = prefix & 1;

	//  Choose child
	if (child == 0) {
		next_node = trie->left;
		if (!next_node) {
			//  Add new prefix
			next_node = create_node();
			if ((prefix & 1) == 0)
				trie->left = next_node;
			else
				trie->right = next_node;
		}
	} else {
		next_node = trie->right;
		if (!next_node) {
			//  Add new prefix
			next_node = create_node();
			if ((prefix & 1) == 0)
				trie->left = next_node;
			else
				trie->right = next_node;
		}
	}

	trie_insert_helper(next_node, entry, prefix >> 1, mask >> 1);
}


//  Function that inserts an entry as a new node in trie
void trie_insert(TTrie trie, struct route_table_entry *entry) {
	trie_insert_helper(trie, entry, reverse_bits(ntohl(entry->prefix)), reverse_bits(ntohl(entry->mask)));
}

//  Helper function for tail recursion
void trie_search_helper(TTrie trie, uint32_t ip, struct route_table_entry **best_route) {
	if (trie->entry) {
		*best_route = trie->entry;
	}

	//  Choose child
	TTrie next_node = NULL;
	if ((ip & 1) == 0) {
		next_node = trie->left;
	} else {
		next_node = trie->right;
	}

	if (!next_node)
		return;

	return trie_search_helper(next_node, ip >> 1, best_route);
}

//  Function that searches the trie to find given ip
struct route_table_entry *trie_search(TTrie trie, uint32_t ip) {
	struct route_table_entry *best_route = NULL;
	trie_search_helper(trie, reverse_bits(ntohl(ip)), &best_route);
	return best_route;
}
