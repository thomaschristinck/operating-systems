#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>

#include "a2_lib.h"

#define POD_LENGTH 256
#define POD_NUMBERS 256
#define VALUE_SIZE 256  
#define KEY_SIZE 32 
#define POD_SIZE (VALUE_SIZE * POD_LENGTH)
//Author: Thomas Christinck
//March 12, 2017

//Note: please read accompanying for_marker.txt

//The store will have the following values: (1) A value array, holding a value at each slot in each pod. (2) A
//array, holding the key (unhashed) for each pod. (3) A write index for each pod number, specifying which slot
//in the pod we are writing to. (4) A read index for each pod, specifying which slot we are reading from and which 
//key we are indexing. (5) An is_index array to determine whether an index for a key is already taken. (6) A key_value
//array to hold key_values and relate them to their indices. (7) An array of semaphores for each pod.

typedef struct
{
	char value[POD_NUMBERS][POD_LENGTH][VALUE_SIZE];
	char key[POD_NUMBERS][POD_LENGTH][KEY_SIZE];
	int write_index[POD_NUMBERS];
	int stored_values[POD_NUMBERS];
	int read_index[POD_NUMBERS][POD_LENGTH];
	int is_index[POD_NUMBERS][POD_LENGTH];
	char key_value[POD_NUMBERS][POD_LENGTH][KEY_SIZE];
	sem_t semaphore[POD_NUMBERS];
} Store;

Store* store;


int kv_store_create(char* name)
{
	//Establish connection between file descriptor and shared memory object
	int new_mem = 1;
	int fd = shm_open(name, O_CREAT|O_RDWR|O_EXCL, S_IRWXU);
	//Check if shared memory already exists
	if (fd == -1)
	{
		fd = shm_open(name, O_CREAT|O_RDWR, S_IRWXU);
		new_mem = 0;
	}
	else
	{
		//Size the store to fit pods
		ftruncate(fd, sizeof(Store));
	}
	
	if(fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}
	
	//Map file to memory and save address of mapped space
	store = (Store*) mmap(NULL, sizeof(Store), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	
	//Close the file descriptor associated with the shared memory
	if(close(fd) == -1)
	{
		printf("Error closing fd associated with shared memory object: %s", strerror(errno));
		return -1;
	}

	if (new_mem)
	{
		//Set store to 0 and initialize semaphores
		memset(store, 0, sizeof(Store));
		for(int i = 0; i < POD_NUMBERS; i++)
		{
			sem_init(&(store -> semaphore[i]), 1, 1);
		}
	}
	
	return 0;
}

int kv_store_write(char* key, char* value)
{
	if (strlen(value) > VALUE_SIZE)
	{
		printf("String value to be stored is too long! Max is %d bytes!", VALUE_SIZE);
		return -1;
	}
	else if(value == NULL)
	{
		printf("Must enter a non-null value!");
		return -1;
	}
	
	//Hash the key to determine pod to store data (replace with hash function later)
	int pod_index = hash(key);
	int is_full = 0;

	//**********Critical Section ***************
	sem_wait(&(store -> semaphore[pod_index]));

	//Check that there is room in the pod, otherwise use FIFO eviction
	if(store -> write_index[pod_index] == POD_LENGTH)
	{
		store -> write_index[pod_index] = 0;
		is_full = 1;
	}
	//Copy value and key to store. Set value slot to 0 before write to prevent over-writing
	memset(store -> value[pod_index][store -> write_index[pod_index]], 0, VALUE_SIZE);
	memset(store -> key[pod_index][store -> write_index[pod_index]], 0, VALUE_SIZE);
	memcpy(store -> value[pod_index][store -> write_index[pod_index]], value, strlen(value));
	memcpy(store -> key[pod_index][store -> write_index[pod_index]], key, strlen(key));
	
	store -> write_index[pod_index]++;

	//Update stored values
	if(!is_full)
	{
		store -> stored_values[pod_index]++;
	}

	sem_post(&(store -> semaphore[pod_index]));
	// ******************************************

	return 0;

}


char* kv_store_read(char* key)
{
	//Find pod associated with key
	int pod_index = hash(key);

	//**********Critical Section ***************
	sem_wait(&(store -> semaphore[pod_index]));

	//Ensure that there is at least one value to read
	int readables = store -> stored_values[pod_index];
	if(readables == 0)
	{
		sem_post(&(store -> semaphore[pod_index]));
		// ******************************************
		printf("Nothing to read here...\n");
		return NULL;
	}
	//First, search through key_values stored to see if the key has already been given a unique index 
	int index = 0;
	while ((strcmp(store -> key_value[pod_index][index], key) != 0) && index < POD_LENGTH)
	{
		index++;
	}
	if(index == POD_LENGTH)
	{
		//Key has not been rehashed to a unique read index
		int hashed_key = rehash(key);
		memcpy(store -> key_value[pod_index][hashed_key], key, strlen(key));
		store -> read_index[pod_index][hashed_key] = 0;
		store -> is_index[pod_index][hashed_key] = 1;
		index = hashed_key;
	}
	else
	{
		//Key has been rehashed and index = index
	}

	//Search through all slots in pod for a key that matches what we're holding 
	int slots_searched = 0;
	while(strcmp(store -> key[pod_index][store -> read_index[pod_index][index]], key) != 0 && 
		slots_searched <= readables)
	{
		//Ensure that we are reading valid keys
		//If read index is at the last written slot in the pod, reset to the beginning of the pod and continue searching
		if (store -> read_index[pod_index][index] >= (readables - 1))
		{
			store -> read_index[pod_index][index] = -1;
		}
		//printf("Actual key %s \n", store -> key[pod_index][store -> read_index[pod_index][index]]);
		store -> read_index[pod_index][index]++;
		slots_searched++;
	}
	//Check if there is no pod with requested key
	if(slots_searched > readables && 
		strcmp(store -> key[pod_index][store -> read_index[pod_index][index]], key) != 0)
	{
		sem_post(&(store -> semaphore[pod_index]));
		// ******************************************
		printf("Could not find pod with requested key!\n");
		
		return NULL;
	}

	//Now we have the read_index of the first value. 
	size_t string_length = sizeof(char) * VALUE_SIZE;
	//Read a value in order (FIFO)
	char* stored_value;
	stored_value = malloc(string_length);
	memcpy(stored_value, store -> value[pod_index][(store -> read_index[pod_index][index])], string_length);

	//Increment read index
	store -> read_index[pod_index][index]++;
	
	sem_post(&(store -> semaphore[pod_index]));
	// ******************************************

	return stored_value;
}
	

char** kv_store_read_all(char* key)
{
	//Find pod associated with key
	int pod_index = hash(key);
	int key_hits = 0;
	int index[POD_LENGTH];

	//**********Critical Section ***************
	sem_wait(&(store -> semaphore[pod_index]));

	for(int i = 0; i < POD_LENGTH; i++)
	{
		if(strcmp(store -> key[pod_index][i], key) == 0)
		{
			index[key_hits] = i;
			key_hits++;
		}
	}
	//Check if there is no pod with requested key
	if(key_hits == 0)
	{
		sem_post(&(store -> semaphore[pod_index]));
		// ******************************************
		printf("Could not find anything with provided key!\n");
		return NULL;
	}

	//Now build char[] with the values
	char** long_string;
	long_string = malloc(sizeof(char*) * (key_hits + 1) + 1);
	if(long_string == NULL)
	{
		sem_post(&(store -> semaphore[pod_index]));
		// ******************************************
		printf("Error when malloc'ing for readall!\n");
		return NULL;
	}
	//Allocate space for each string
	for(int i = 0; i < key_hits; i++)
	{
		long_string[i] = malloc(sizeof(char) * VALUE_SIZE);
		memset(long_string[i], 0, sizeof(char) * VALUE_SIZE);
		memcpy(long_string[i], store -> value[pod_index][index[i]], (sizeof(char) * VALUE_SIZE));
	}
	long_string[key_hits] = NULL;

	sem_post(&(store -> semaphore[pod_index]));
	// ******************************************

	return long_string;

}

int kv_delete_db()
{
	//Destroy the semaphores
	for(int i = 0; i < POD_NUMBERS; i++)
	{
		sem_destroy(&(store -> semaphore[i]));
	}

	//Unmap the shared memory
	 munmap(store, sizeof(Store));
}

//Hashing function provided on course page on myCourses
int hash (char* key)
{
	int hashAddress = 5381;
	for(int count = 0; key[count] != '\0'; count ++)
	{
		hashAddress = ((hashAddress << 5) + hashAddress) + key[count];
	}
	return hashAddress % POD_NUMBERS < 0 ? -hashAddress % POD_NUMBERS : hashAddress % POD_NUMBERS;
}

//Rehashing function to provide a unique index for each key in a pod
int rehash(char* key)
{
	int searched = 0;
	int pod_index = hash(key);
	int index = 0;
	while(searched <= POD_NUMBERS &&
		store -> is_index[pod_index][index] != 0)
	{
		index = (index + 1) %  POD_LENGTH;
	}
	return index;
}


