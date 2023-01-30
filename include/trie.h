ANN void trie_ini(MemPool _mp);
ANN void trie_end(void);
ANN void trie_add(char *name, void *value);
ANN Vector trie_get(char *name);
ANN Vector trie_starts_with(char *partial);
