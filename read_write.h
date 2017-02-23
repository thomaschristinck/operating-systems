#ifndef READ_WRITE_H
#define READ_WRITE_H

#include <stdbool.h>

#define MAX_PAIRS 80;

//Creates a store if not yet created, and opens the store if created. Returns -1 if failure occurs
int kv_store_create(char* name);

//Set max size of key and value. Evict the first entry if the store is full. The function should also 
//update an index maintained in the store
int kv_store_write(char* name, int key, char* value);

//Takes a key and searches for key-value pair. If found, returns a copy of the value (a pointer to the
//string is returned, NULL if no pair is found)
char* kv_store_read(char* name, int key);

//Function should take a key and return all values in the store
char** kv_store_read_all(char* name, int key);

#endif
//READ_WRITE_H