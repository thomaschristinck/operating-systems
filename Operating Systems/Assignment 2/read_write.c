#include <stdlib.h>
#include <stdio.h>

int kv_store_create(char* name){
	return 1;
}

int kv_store_write(char* key, char* value){
	return 1;
}

char* kv_store_read(char* key){
	char tempc = 'f';
	char* tempp = &tempc;
	return tempp;
}

char** kv_store_read_all(char* key){
	return 1;
}
