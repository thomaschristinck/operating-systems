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
#define POD_NUMBERS 32
#define VALUE_SIZE 256  
#define POD_SIZE (VALUE_SIZE * POD_LENGTH)


//The store will have the following values: (1) A value array, holding a value at each slot in each pod. (2) A
//array, holding the key (unhashed) for each pod. (3) A write index for each pod number, specifying which slot
//in the pod we are writing to. (4) A read index for each pod, specifying which slot we are reading from. (5) An 
//array of semaphores for each pod

typedef struct
{
	char value[POD_NUMBERS][POD_LENGTH][VALUE_SIZE];
	char key[POD_NUMBERS][POD_LENGTH][VALUE_SIZE];
	int write_index[POD_NUMBERS];
	int stored_values[POD_NUMBERS];
	int read_index[POD_NUMBERS];
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
		memset(store, 0, sizeof(Store));
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

	//Check that there is room in the pod, otherwise use FIFO eviction
	if(store -> write_index[pod_index] == POD_LENGTH)
	{
		store -> write_index[pod_index] = 0;
		is_full = 1;
	}
	//Copy value and key to store. Set value slot to 0 before write to prevent over-writing
	memset(store -> value[pod_index][store -> write_index[pod_index]], 0, VALUE_SIZE);
	memcpy(store -> value[pod_index][store -> write_index[pod_index]], value, strlen(value));
	memcpy(store -> key[pod_index][store -> write_index[pod_index]], key, strlen(key));
	store -> write_index[pod_index]++;
	
	//Update stored values
	if(!is_full)
	{
		store -> stored_values[pod_index]++;
	}
	return 0;

}


char* kv_store_read(char* key)
{
	//Find pod associated with key
	int pod_index = hash(key);

	int slots_searched = 0;
	//store -> read_index[pod_index] = 0;

	//Find read index (key should match the key we are holding)
	while(strcmp(store -> key[pod_index][store -> read_index[pod_index]], key) != 0 && 
		slots_searched <= POD_LENGTH)
	{
		//If read index is at the last slot in the pod, reset to the beginning of the pod and continue searching
		if(store -> read_index[pod_index] == POD_LENGTH - 1)
		{
			store -> read_index[pod_index] = -1;	
		}
		store -> read_index[pod_index]++;
		slots_searched++;
	}
	//Check if there is no pod with requested key
	if(slots_searched > POD_LENGTH && 
		strcmp(store -> key[pod_index][store -> read_index[pod_index]], key) != 0)
	{
		printf("Could not find pod with requested key!\n");
		return NULL;
	}
	int readables = store -> stored_values[pod_index];
	//Now we have the read_index of the first value. 
	if(readables != 0)
	{
		size_t string_length = sizeof(char) * VALUE_SIZE;
		if(readables > (store -> read_index[pod_index]))
		{
			//Read a value in order (FIFO)
			
			
			//strcpy(return_value, store -> value[pod_index][(store -> read_index[pod_index])]);
			char* stored_value;
			stored_value = malloc(string_length);
			memcpy(stored_value, store -> value[pod_index][(store -> read_index[pod_index])], string_length);
		
			//Read a value in order (FIFO)
			store -> read_index[pod_index]++;
			return stored_value;
		}
		else
		{
			//Read first value
			store -> read_index[pod_index] = 0;
			char* stored_value;
			stored_value = malloc(string_length);
			memcpy(stored_value, store -> value[pod_index][(store -> read_index[pod_index])], string_length);
		
			//Increment index and resume reading in order (FIFO)
			store -> read_index[pod_index]++;
			
			return stored_value;
		}
	}
	else
	{
		printf("Nothing to read here!");
		return NULL;
	}
}

char** kv_store_read_all(char* key)
{
	//Find pod associated with key
	int pod_index = hash(key);

	int key_hits = 0;
	int index[POD_LENGTH];
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
		printf("Could not find anything with provided key!\n");
		return NULL;
	}
	//Now build char[] with the values
	char* long_string[key_hits];
	for(int i = 0; i < key_hits; i++)
	{
		long_string[i] = store -> value[pod_index][index[i]];
	}
	//ret = &memcpy(ret, )
	
	printf("Test %s \n", long_string[1]);
	/*char* tempc = "yes";
	char** tempcp = &tempc;*/
	size_t long_string_size = sizeof(char) * POD_LENGTH * VALUE_SIZE;
	char** ret = malloc(long_string_size);
	memmove(&ret, long_string, long_string_size);
	return ret;
}

int kv_delete_db()
{
	//Unmap the shared memory
	 munmap(store, sizeof(Store));
	//Unlink shared database
	// shm_unlink(store -> name);
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