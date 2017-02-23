#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <errno.h>
#include <unistd.h>

#include "read_write.h"

size_t value_size = 150;
size_t pod_size = 4 * 150;

struct Section
{
	char* addr;
	bool isEmpty;
};
struct Section sections[16 * 4];

int kv_store_create(char* name)
{
	//Establish connection between file descriptor and shared memory object
	int fd = shm_open(name, O_CREAT|O_RDWR, S_IRWXU);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}

	//Loop through all sections and set to isEmpty = true
	for (int i = 0; i < 16 * 4 ; i++)
	{
		sections[i].isEmpty = true;
		sections[i].addr = '\0';
	}

	return 1;
}

int kv_store_write(char* name, int key, char* value)
{
	if (sizeof(value) > value_size)
	{
		printf("String value to be stored is too long! Max is %zu bytes!", value_size);
		return -1;
	}
	//Open the shared memory object
	int fd = shm_open(name, O_RDWR, S_IRWXU);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}

	//Map file to memory, save pointer to the start of the mapped data. Then set up all
	//pods of size pod_size 
	char* pod[16];
	pod[0] = mmap(NULL, value_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = 1; i < 16; i++)
	{
		pod[i] = (void*) ((char*)pod[i-1] + pod_size);
	}
	
	//Hash the key to determine pod to store data
	int pod_number = key % 16;
	//Idea - when key is known we know what pod, i.e pod[i], to write in. We can divide this pod into pod_size / value_size sections.
	//Maybe set up an array of pods, where each pod is a queue of values
	
	//Now look for the first non-empty slot. If all slots are empty, evict the FIRST (HOW TO DO??) value entered
	for (int i = pod_number * 4; i < pod_number * 4 + 4; i ++)
	{
		
	} 
	//Size the store to fit 16 pods
	ftruncate(fd, 16 * pod_size);
	close(fd);

	//Copy string to memory
	memcpy(pod[pod_number], value, strlen(value));
	return 1;
}

char* kv_store_read(char* name, int key)
{
	//Open the shared memory object
	int fd = shm_open(name, O_RDONLY, 0);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return NULL;
	}
	//Find pod associated with key
	int pod_number = key % 16;
	char* pod[16];
	pod[0] = mmap(NULL, value_size, PROT_READ, MAP_SHARED, fd, 0);
	for(int i = 1; i < 16; i++)
	{
		pod[i] = (void*) ((char*)pod[i-1] + pod_size);
	}
	//Return value stored in pod, indexed by the key
	return pod[pod_number];
	
}

char** kv_store_read_all(char* name, int key)
{
	char* tempc = "yes";
	char** tempcp = &tempc;
	return tempcp;
}
