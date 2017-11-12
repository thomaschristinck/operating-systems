#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sfs_api.h"
#include "disk_emu.h"
//ECSE 427 Assignment 3 ---- Simple Shadow File System (No Commit/Restore Implementation)
//@author thomaschristinck

#define BLOCK_SIZE 1024			//Size (in bytes) of a block
#define BLOCK_NUMBERS 1024		//Number of total blocks in the file system
#define NODE_NUMBERS 224 		//Number of I-Nodes in the file system
#define NODE_SIZE 64			//Size of an I-Node 
#define TABLE_SIZE 64			//Size of slots iin file table
#define DIRECTORY_SIZE 64		//Limited by 1024 / (pointer size + filename size)
#define FBM_BLOCK (BLOCK_NUMBERS - 2)							//Block number of FBM block
#define WM_BLOCK (BLOCK_NUMBERS - 1)							//Block number of WM block
#define DIRECT_POINTERS ((NODE_SIZE/4) - 2)						//Number of direct pointers in an i-node - 14
#define INDIRECT_POINTERS (BLOCK_SIZE / sizeof(int))			//Number of pointers within an indirect pointer in an inode
#define NODE_BLOCKS (DIRECT_POINTERS)							//Number of blocks devoted to node storage - 14
#define NODES_PER_BLOCK (BLOCK_SIZE / NODE_SIZE)				//Number of nodes in a block - 8
#define DIRECT_FILE_SIZE (BLOCK_SIZE * (DIRECT_POINTERS))	//Size of a file pointed to by alll direct pointers and no indirect

typedef struct inode
{
	int size;
	int direct[DIRECT_POINTERS];
	int indirect;
} inode_t;

typedef struct superblock
{
	unsigned char magic[4];
	int block_size; 
	inode_t root;
	int lastshadow;
} superblock_t;

typedef struct block{
	unsigned char bytes[BLOCK_SIZE];
} block_t;

typedef struct root{
	char filename[DIRECTORY_SIZE][10];
	int inode_number[DIRECTORY_SIZE];
	int size;
} root_t;

typedef struct fd_table{
	int inode_number[TABLE_SIZE];
	int read_pointer[TABLE_SIZE];
	int write_pointer[TABLE_SIZE];
} fd_table_t;

root_t root_dir;
fd_table_t fd_table;
block_t bit_map;
block_t write_mask;

void mkssfs(int fresh)
{
	char* filename = "thomasdisk";

	if(fresh)
	{
		//Create a new file system
		init_fresh_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);

		//Set up super node fields. Super node is block 0 and holds the following:
		superblock_t super;
		strcpy(super.magic, "ABCD");
		super.block_size = BLOCK_SIZE;

		//Populate j-node. After this population process, the j-node should point to blocks 1 through 
		//DIRECT_POINTERS++. Next, fill these blocks with inodes
		super.root.size = 0;
		int block_index = 1;
		for(int i = 0; i < DIRECT_POINTERS; i++)
		{
			super.root.direct[i] = block_index;
			block_index++;
		}
		super.root.indirect = -1;
		super.lastshadow = 0;
	
		//Calloc empty block of size block size, memcpy the super block to that block
		void* superblock = calloc(1, BLOCK_SIZE);
		memcpy(superblock, &super, sizeof(superblock_t));

		//Write super block to the disk
		write_blocks(0, 1, superblock);
		free(superblock);

		//Initialize free bit map (all 1's, all blocks free), then write to second last block (BLOCK_NUMBERS -3)
		//Note that after bit_map.bytes[BLOCK_NUMBERS] the free bit map is invalid
		block_t bit_map;
		for(int i = 0; i < BLOCK_SIZE; i++)
		{
			bit_map.bytes[i] = 0xFF;
		}
		for(int i = 0; i < (NODE_BLOCKS + 2); i++)
		{
			bit_map.bytes[i] = 0xFE;
		}
		write_blocks(FBM_BLOCK, 1, &bit_map);	

		//Initialize write mask. Initially all are writeable. Blocks that are not writeable include the inode 
		//blocks
		block_t write_mask;
		for(int i = 0; i < BLOCK_SIZE; i++)
		{
			write_mask.bytes[i] = 0xFF;
		}

		write_blocks(WM_BLOCK, 1, &write_mask);

		
		//Now each of the blocks pointed to by the j-node should be full of i-nodes.
		block_t inodes[NODE_BLOCKS];
		//Array of inodes for each block
		inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
		for(int block_index = 0; block_index < NODE_BLOCKS; block_index ++)
		{
			for(int node_index = 0; node_index < NODES_PER_BLOCK; node_index++)
			{
				for(int i = 0; i < DIRECT_POINTERS; i++)
				{
					inode_array[block_index][node_index].direct[i] = -1;
				}
				inode_array[block_index][node_index].size = -1;
				inode_array[block_index][node_index].indirect = -1;
			}
		}
		
		//Let the 15th block (1st data block be the root dir, and inode_array[0][0].direct[0] points to this) be the root dir
		memcpy(&inodes, &inode_array, NODE_BLOCKS * BLOCK_SIZE);
		
		//Then write all inode blocks to the disk
		write_blocks(1, NODE_BLOCKS, &inodes);

		char nullstring[10] = "";
		//Initialize empty root directory
		for(int i = 0; i < DIRECTORY_SIZE; i++)
		{
			strcpy(root_dir.filename[i], nullstring);
			root_dir.inode_number[i] = -1;
		}
		root_dir.size = -1;
		block_t root;
		memcpy(&root, &root_dir, sizeof(root_t));
		write_blocks(NODE_BLOCKS + 1, 1, &root_dir);
	}
	else
	{
		//Open an old file system from the disk
		init_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);
	}
	//Initialize an empty fd table
	for(int table_index = 0; table_index < TABLE_SIZE; table_index++)
	{
		fd_table.read_pointer[table_index] = -1;
		fd_table.write_pointer[table_index] = -1;
		fd_table.inode_number[table_index] = -1;
	}
}

//Opens the given file
int ssfs_fopen(char* name)
{
	//Read in inodes, free bit map, and write mask
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	read_blocks(FBM_BLOCK, 1, &bit_map);
	read_blocks(WM_BLOCK, 1, &write_mask);
	read_blocks(NODE_BLOCKS + 1, 1, &root_dir);

	int root_index = 0;
	//Look for file in the root directory to see if it exists already
	while(root_index < DIRECTORY_SIZE && strcmp(root_dir.filename[root_index], name) != 0)
	{
		root_index++;
	}
	//Check if we found file name. Fetch inode
	if(root_index != DIRECTORY_SIZE)
	{
		int inode = root_dir.inode_number[root_index];
		int file_size = inode_array[inode].size;

		//Set up fd table entry
		int table_index = 0; 
		while(table_index < TABLE_SIZE && fd_table.inode_number[table_index] != -1)
		{
			table_index++;
		}
		if(table_index == TABLE_SIZE)
		{
			printf("No more available slots in file descriptor table!\n");
			free(inode_array);
			return -1;
		}
		else
		{
			fd_table.write_pointer[table_index] = file_size;
			fd_table.read_pointer[table_index] = 0;
			fd_table.inode_number[table_index] = inode;
			free(inode_array);
			return table_index;
		}
	}
	else
	{
		//We need to open a new file in the file descriptor table. This includes adding to the root_dir and setting
		//aside an inode. First ensure there is room in the root directory
		if(root_dir.size == DIRECTORY_SIZE)
		{
			printf("No more available slots in directory!\n");
			free(inode_array);
			return -1;
		}
		root_index = 0;
		while(root_index < DIRECTORY_SIZE && root_dir.inode_number[root_index] > -1)
		{
			root_index ++;
		}
		if(root_index == DIRECTORY_SIZE)
		{
			printf("It seems there is no room left for files in the system!\n");
			free(inode_array);
			return -1;
		}
		else
		{
			//Find free block (through free bit map)
			int free_index = 0;
			while(free_index < BLOCK_NUMBERS && (bit_map.bytes[free_index] != 0xFF || write_mask.bytes[free_index] != 0xFF))
			{
				free_index ++;
			}
			if(free_index == BLOCK_NUMBERS)
			{
				printf("No free blocks available!\n");
				free(inode_array);
				return -1;
			}
			bit_map.bytes[free_index] = 0xFE;
			write_mask.bytes[free_index] = 0xFF;
			
			//Have root index, now find free inode number
			int index = 0;
			while(index < (NODES_PER_BLOCK * DIRECT_POINTERS) && inode_array[index].size != -1)
			{
				index++;
			}
			if(index == (NODES_PER_BLOCK * DIRECT_POINTERS))
			{
				printf("Error! No inodes available!\n");
				free(inode_array);
				return -1;
			}

			int inode = index;
		
			//Find free slot in fd table
			int table_index = 0; 
			while(table_index < TABLE_SIZE && fd_table.inode_number[table_index] != -1)
			{
				table_index++;
			}
			if(table_index == TABLE_SIZE)
			{
				printf("No more available slots in file descriptor table!\n");
				free(inode_array);
				return -1;
			}
			//Write free bit map and write mask to disk
			write_blocks(FBM_BLOCK, 1, &bit_map);
			write_blocks(WM_BLOCK, 1, &write_mask);

			//Now set up root directory and table
			root_dir.inode_number[root_index] = inode;
			root_dir.size++;
			strcpy(root_dir.filename[root_index], name);
			fd_table.read_pointer[table_index] = 0;
			fd_table.write_pointer[table_index] = 0;
			fd_table.inode_number[table_index] = inode;

			//Then write all inode blocks to the disk
			inode_array[inode].size++;
			inode_array[inode].direct[0] = free_index;
			block_t inodes[NODE_BLOCKS];
			memcpy(&inodes, inode_array, NODE_BLOCKS * BLOCK_SIZE);
			write_blocks(1, NODE_BLOCKS, &inodes);
			free(inode_array);

			//Write root_dir to the disk
			write_blocks(NODE_BLOCKS + 1, 1, &root_dir);
			return table_index;
		}

	}
}

//Close the given file
int ssfs_fclose(int fileID)
{
	//Check file exists
	if(fileID >= TABLE_SIZE || fileID < 0)
	{
		printf("Error! File descriptor %d is out of range!\n", fileID);
		return -1;
	}
	if(fd_table.inode_number[fileID] == -1)
	{
		printf("Error! No file with file descriptor %d!\n", fileID);
		return -1;
	}
	//Otherwise, keep file in the root directory but remove all references in the file descriptor table
	fd_table.read_pointer[fileID] = -1;
	fd_table.write_pointer[fileID] = -1;
	fd_table.inode_number[fileID] = -1;
	return 0;
}

//Seek (read) to the location starting at beginning
int ssfs_frseek(int fileID, int loc){
	//Check file with fileID exists
	if(fileID > TABLE_SIZE || fileID < 0)
	{
		printf("Error! File descriptor %d is not valid!\n", fileID);
		return -1;
	}
	if(fd_table.inode_number[fileID] == -1)
	{
		printf("Error! File is not open!\n");
		return -1;
	}
	//Load inode (necessary for size check)
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	inode_t inode = inode_array[fd_table.inode_number[fileID]];

	if(loc > inode.size || loc < 0)
	{
		printf("Error! Cannot read from position %d in file of size %d!\n", loc, inode.size);
		free(inode_array);
		return -1;
	}
	//Otherwise, adjust write pointer accordingly
	fd_table.read_pointer[fileID] = loc;
	free(inode_array);
	return 0;
}

//Seek (write) to the location starting at beginning
int ssfs_fwseek(int fileID, int loc){
	//Check file with fileID exists
	if(fileID > TABLE_SIZE || fileID < 0)
	{
		printf("Error! File descriptor %d is not valid!\n", fileID);
		return -1;
	}
	if(fd_table.inode_number[fileID] == -1)
	{
		printf("Error! File is not open!\n");
		return -1;
	}
	//Load inode
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	inode_t inode = inode_array[fd_table.inode_number[fileID]];

	if(loc > inode.size || loc < 0)
	{
		printf("Error! Cannot write from position %d in file of size %d!\n", loc, inode.size);
		free(inode_array);
		return -1;
	}
	//Otherwise, adjust write pointer accordingly
	fd_table.write_pointer[fileID] = loc;
	free(inode_array);
	return 0;
}

//Write buf characters to disk
int ssfs_fwrite(int fileID, char* buf, int length){
	//Check file with fileID exists
	if(fileID > TABLE_SIZE || fileID < 0)
	{
		printf("Error! File descriptor %d is not valid!\n", fileID);
		return -1;
	}
	if(fd_table.inode_number[fileID] == -1)
	{
		printf("Error! File is not open!\n");
		return -1;
	}
	if(length < 0)
	{
		printf("Error! Write length %d is not valid!\n", length);
		return -1;
	}

	//Find inode
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	inode_t* inode = (inode_t*)malloc(sizeof(inode_t));
	memcpy(inode, &inode_array[fd_table.inode_number[fileID]], sizeof(inode_t));

	//Get write pointer. We need to figure out how many more blocks need to be set aside for the write (if any)
	int write_ptr = fd_table.write_pointer[fileID];
	int total_length = write_ptr + length;
	
	//Check if more blocks need to be allocated
	int total_needed = (total_length % BLOCK_SIZE == 0) ? total_length / BLOCK_SIZE : total_length / BLOCK_SIZE + 1;
	int total_allocated = (inode -> size % BLOCK_SIZE == 0 && inode -> size != 0) ? inode -> size / BLOCK_SIZE : inode -> size / BLOCK_SIZE + 1;
	int blocks_needed = total_needed - total_allocated;

	if(total_needed > (BLOCK_SIZE / sizeof(int) + DIRECT_POINTERS))
	{
		printf("Error! File is requesting %d blocks (this is more than the max file size)!\n", total_needed);
		free(inode_array);
		free(inode);
		return -1;	
	}

	int blocks_given = 0;
	if(blocks_needed > 0)
	{
		int free_indexes[blocks_needed];
		//Find free blocks
		read_blocks(FBM_BLOCK, 1, &bit_map);
		read_blocks(WM_BLOCK, 1, &write_mask);
		int i = 0;
		while(i < BLOCK_NUMBERS && blocks_given <= blocks_needed)
		{
			if(bit_map.bytes[i] == 0xFF && write_mask.bytes[i] == 0xFF)
			{
				free_indexes[blocks_given] = i;
				bit_map.bytes[i] = 0xFE;
				blocks_given++;
			}
			i++;
		}
		if(blocks_given < blocks_needed || i == BLOCK_NUMBERS)
		{
			printf("Error! Could not write because we coudn't find enough free blocks!\n");
			free(inode_array);
			free(inode);
			return -1;
		}

		//Allocate blocks needed
		blocks_given = 0;
		int pointer_index = 0;
		while(blocks_given < blocks_needed && pointer_index < DIRECT_POINTERS)
		{
			if(inode -> direct[pointer_index] == -1)
			{
				inode -> direct[pointer_index] = free_indexes[blocks_given];
				blocks_given++;
			}
			pointer_index++;
		} 
		
		if(pointer_index == DIRECT_POINTERS && blocks_given < blocks_needed)
		{
			//Find which slot in the indirect block we'll be starting from. If the file hasn't previously used the indirect, we start at 
			//position 1. Otherwise, we start at the next spot to increase the file size.
			int indirect_start = (inode -> size <= DIRECT_FILE_SIZE) ? 0 : (inode -> size / BLOCK_SIZE) - DIRECT_POINTERS;
			
			if(indirect_start >= BLOCK_SIZE / sizeof(int))
			{
				printf("Error! Could not write because we have no more room in the indirect!\n");
				free(inode_array);
				free(inode);
				return -1;
			}
			//Find a free block for the indirect
			int indirect_pointers[INDIRECT_POINTERS];
			if(inode -> indirect == -1)
			{
				int block_index = 0;
				while(block_index < BLOCK_NUMBERS && 
					(bit_map.bytes[block_index] != 0xFF || write_mask.bytes[block_index] != 0xFF))
				{
					block_index++;
				}
				if(block_index == BLOCK_NUMBERS)
				{
					printf("Error! Could not write because there are no more free blocks!\n");
					free(inode_array);
					free(inode);
					return -1;
				}
				for(int i = 0; i < INDIRECT_POINTERS; i++)
				{
					memset(&indirect_pointers[i], 0, sizeof(int));
				}
				inode -> indirect = block_index;

				bit_map.bytes[block_index] = 0xFE;
			}
			else
			{
				//There must already be a block for the indirect pointer
				read_blocks(inode -> indirect, 1, &indirect_pointers);
			}
			//We have an indirect. Now allocate the blocks for the indirect
			for(int i = (blocks_given == 0) ? 0 : (blocks_needed - blocks_given); i <= blocks_needed && indirect_start < INDIRECT_POINTERS; indirect_start++)
			{
				if(indirect_pointers[indirect_start] == 0)
				{
					memcpy(&indirect_pointers[indirect_start], &free_indexes[i], sizeof(int));
					i++;
				}
			}
			if(indirect_start == INDIRECT_POINTERS)
			{
				printf("Error! Ran out of indirect pointers!\n");
				free(inode_array);
				free(inode);
				return -1;
			}
			write_blocks(inode -> indirect, 1, &indirect_pointers);
			//Now we have an indirect with the appropriate block pointers. 
		}
		//Update free bit map
		write_blocks(FBM_BLOCK, 1, &bit_map);
	}
	//Now start at write pointer and WRITE. Establish an offset
	int offset;
	if(write_ptr % BLOCK_SIZE + length < BLOCK_SIZE)
	{
		offset = length;
	}
	else
	{
		offset = BLOCK_SIZE - (write_ptr % BLOCK_SIZE);
	}

	int bytes_written = 0;
	//The maximum writeable "middle" blocks we could possible have (Note this is at least zero)
	int writeable_blocks = length / BLOCK_SIZE;

	//Start writing the first block
	int start_block = (write_ptr / BLOCK_SIZE);
	if(start_block < DIRECT_POINTERS)
	{
		char* block_buff = (char*)malloc(BLOCK_SIZE);
		read_blocks(inode -> direct[start_block], 1, block_buff);
		memcpy(block_buff + (write_ptr % BLOCK_SIZE), buf, offset);
		write_blocks(inode -> direct[start_block], 1, block_buff);
		free(block_buff);
		bytes_written += offset;
	}

	//Now if there are more bytes addressable by direct pointers to write, we do so directly
	int blocks_written = 0;
	if(writeable_blocks > 0 && bytes_written < length)
	{
		for(start_block++; start_block < DIRECT_POINTERS && blocks_written < writeable_blocks && (bytes_written + BLOCK_SIZE <= length); bytes_written += BLOCK_SIZE)
		{
			write_blocks(inode -> direct[start_block], 1, buf + bytes_written);
			start_block++;
			blocks_written++;
		}
	}

	//Check if there is a remaining (final) block to be written addressable by direct pointers
	if(bytes_written < length && start_block < DIRECT_POINTERS)
	{
		int length_remaining;
		if(writeable_blocks == 0 && (start_block + 1) < DIRECT_POINTERS)
		{
			if((start_block + 1) < DIRECT_POINTERS)
			{
				start_block++;
				length_remaining = (length + write_ptr > DIRECT_FILE_SIZE) ? (DIRECT_FILE_SIZE - bytes_written) : (length - bytes_written);
			}
			else
			{
				length_remaining = 0;
			}
		}
		else
		{
			length_remaining = (length + write_ptr > DIRECT_FILE_SIZE) ? (DIRECT_FILE_SIZE - bytes_written) : (length - bytes_written);
		}
		char* block_buf = malloc(BLOCK_SIZE);
		read_blocks(inode -> direct[start_block], 1, block_buf);
		memcpy(block_buf, buf + bytes_written, length_remaining);
		write_blocks(inode -> direct[start_block], 1, block_buf);
		free(block_buf);
		bytes_written += length_remaining;
	}

	//Finally, if the indirect pointer must be used, start here
	if(bytes_written < length)
	{	
		//Determine section of inode pointer to start at
		int inode_start = (write_ptr > DIRECT_FILE_SIZE) ? (write_ptr / BLOCK_SIZE - DIRECT_POINTERS) : 0;
	
		//Read in indirect blocks that will be written to
		int indirect_pointers[INDIRECT_POINTERS];
		read_blocks(inode -> indirect, 1, &indirect_pointers);
		
		//Write first indirect block
		char* read_buf = malloc(BLOCK_SIZE);
		read_blocks(indirect_pointers[inode_start], 1, read_buf);
		if(bytes_written != 0)
		{
			//Non zero offset if bytes have already been written
			offset = (((bytes_written + write_ptr) % BLOCK_SIZE) + length > BLOCK_SIZE) ? BLOCK_SIZE - ((bytes_written + write_ptr) % BLOCK_SIZE) : length;
			if(length - bytes_written < offset)
			{
				offset = length - bytes_written;
			}
		}
		memcpy(read_buf + ((write_ptr + bytes_written) % BLOCK_SIZE), buf + bytes_written, offset);
		write_blocks(indirect_pointers[inode_start], 1, read_buf);
		bytes_written += offset;
		inode_start++;
		free(read_buf);

		//Now if there are more bytes addressable by direct pointers to write, we do so directly
		int blocks_written = 0;
		if(writeable_blocks > 0 && bytes_written < length && inode_start < INDIRECT_POINTERS)
		{
			for(inode_start+=0; inode_start < INDIRECT_POINTERS && blocks_written < writeable_blocks && (bytes_written + BLOCK_SIZE <= length); bytes_written += BLOCK_SIZE)
			{
				write_blocks(indirect_pointers[inode_start], 1, buf + bytes_written);
				inode_start++;
				blocks_written++;
			}
		}
		
		//Check if there is a remaining (final) block to be written addressable by indirect pointers
		if(bytes_written < length && inode_start < INDIRECT_POINTERS)
		{
			int length_remaining = length - bytes_written;
			char* block_buf = malloc(BLOCK_SIZE);
			read_blocks(indirect_pointers[inode_start], 1, block_buf);
			memcpy(block_buf, buf + bytes_written, length_remaining);
			write_blocks(indirect_pointers[inode_start], 1, block_buf);
			free(block_buf);
			bytes_written += length_remaining;
		}

	}

	//Update size of file
	int size_increase;
	if(write_ptr + length <= inode -> size)
	{
		size_increase = 0;
	}
	else
	{
		size_increase = (inode -> size == write_ptr) ? bytes_written : (write_ptr + bytes_written - inode -> size);
	}
	inode -> size += size_increase;
	fd_table.write_pointer[fileID] += bytes_written;

	//Update inodes
	block_t* inodes = (block_t*)malloc(NODE_BLOCKS * BLOCK_SIZE);
	inode_array[fd_table.inode_number[fileID]] = *inode;
	memcpy(inodes, inode_array, NODE_BLOCKS * BLOCK_SIZE);
	write_blocks(1, NODE_BLOCKS, inodes);
	free(inode_array);
	free(inodes);
	free(inode);
	
	return bytes_written;	
}

//Read characters from disk into buf
int ssfs_fread(int fileID, char* buf, int length){
	//Check file with fileID exists
	if(fileID > TABLE_SIZE || fileID < 0)
	{
		printf("Error! File descriptor %d is not valid!\n", fileID);
		return -1;
	}
	if(fd_table.inode_number[fileID] == -1)
	{
		printf("Error! File is not open!\n");
		return -1;
	}
	if(length < 0)
	{
		printf("Error! Read length %d is not valid!\n", length);
		return -1;
	}

	//Find inode
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	inode_t* inode = (inode_t*)malloc(sizeof(inode_t));
	memcpy(inode, &inode_array[fd_table.inode_number[fileID]], sizeof(inode_t));
	free(inode_array);

	int read_ptr = fd_table.read_pointer[fileID];
	if(read_ptr + length > inode -> size )
	{
		length = inode -> size - read_ptr;
	}

	//Otherwise, find offset and find how many "center" blocks we read.
	int offset = ((read_ptr % BLOCK_SIZE) + length < BLOCK_SIZE) ? length : BLOCK_SIZE - (read_ptr % BLOCK_SIZE);
	int readable_blocks = length / BLOCK_SIZE;
	int start_index = read_ptr / BLOCK_SIZE;

	//Read first block
	int bytes_read = 0;
	if(start_index < DIRECT_POINTERS)
	{
		char* read_buf = malloc(BLOCK_SIZE);
		read_blocks(inode -> direct[start_index], 1, read_buf);
		memcpy(buf, read_buf + (read_ptr % BLOCK_SIZE), offset);
		free(read_buf);
		bytes_read += offset;
	}
	
	//Read middle blocks
	if(bytes_read < length && readable_blocks > 0 && start_index < DIRECT_POINTERS)
	{
		int blocks_read = 0;
		for(start_index++; blocks_read < readable_blocks && start_index < DIRECT_POINTERS && (bytes_read + BLOCK_SIZE < length); bytes_read += BLOCK_SIZE)
		{
			read_blocks(inode -> direct[start_index], 1, buf + bytes_read);
			start_index++;
		}

	}

	//Read last block
	if(bytes_read < length && start_index < DIRECT_POINTERS)
	{
		int length_remaining;
		if(readable_blocks == 0 && (start_index + 1) < DIRECT_POINTERS)
		{
			if((start_index + 1) < DIRECT_POINTERS)
			{
				start_index++;
				length_remaining = (length + read_ptr > DIRECT_FILE_SIZE) ? (DIRECT_FILE_SIZE - bytes_read) : (length - bytes_read);
			}
			else
			{
				length_remaining = 0;
			}
		}
		else
		{
			length_remaining = (length + read_ptr > DIRECT_FILE_SIZE) ? (DIRECT_FILE_SIZE - bytes_read) : (length - bytes_read);
		}
		//Find length remaining to read in direct pointer section of file
		char* read_buf = malloc(BLOCK_SIZE);
		read_blocks(inode -> direct[start_index], 1, read_buf);
		memcpy(buf + bytes_read, read_buf, length_remaining);
		free(read_buf);
		bytes_read += length_remaining;
	}

	//Determine if there is more to read (i.e. do we have to use indirects)
	if(bytes_read < length)
	{
		//Read in indirect blocks 
		int indirect_pointers[INDIRECT_POINTERS];
		read_blocks(inode -> indirect, 1, &indirect_pointers);

		//Determine starting point for read
		int inode_start = (read_ptr <= DIRECT_FILE_SIZE) ? 0 : (read_ptr / BLOCK_SIZE) - DIRECT_POINTERS;

		//Write first indirect block
		char* read_buf = malloc(BLOCK_SIZE);
		read_blocks(indirect_pointers[inode_start], 1, read_buf);
		if(bytes_read != 0)
		{
			//Non zero offset
			offset = (((bytes_read + read_ptr) % BLOCK_SIZE) + length > BLOCK_SIZE) ? BLOCK_SIZE - ((bytes_read + read_ptr) % BLOCK_SIZE) : length;
			if(length - bytes_read < offset)
			{
				offset = length - bytes_read;
			}
		}
		memcpy(buf + bytes_read, read_buf + ((read_ptr + bytes_read) % BLOCK_SIZE), offset);
		bytes_read += offset;
		inode_start++;
		free(read_buf);

		//Now if there are more bytes addressable by direct pointers to write, we do so directly
		int blocks_read = 0;
		if(readable_blocks > 0 && bytes_read < length && inode_start < INDIRECT_POINTERS)
		{
			for(inode_start+=0; inode_start < INDIRECT_POINTERS && blocks_read < readable_blocks && (bytes_read + BLOCK_SIZE <= length); bytes_read += BLOCK_SIZE)
			{
				char* block_buf = (char*) malloc(BLOCK_SIZE);
				read_blocks(indirect_pointers[inode_start], 1, block_buf);
				memcpy(buf + bytes_read, block_buf, BLOCK_SIZE);
				free(block_buf);
				inode_start++;
				blocks_read++;
			}
		}
		
		//Check if there is a remaining (final) block to be written addressable by direct pointers
		if(bytes_read < length && inode_start < INDIRECT_POINTERS)
		{
			int length_remaining = length - bytes_read;
			char* block_buf = malloc(BLOCK_SIZE);
			read_blocks(indirect_pointers[inode_start], 1, block_buf);
			memcpy(buf + bytes_read, block_buf, length_remaining);
			write_blocks(indirect_pointers[inode_start], 1, block_buf);
			free(block_buf);
			bytes_read += length_remaining;
		}
	}
	free(inode);
	fd_table.read_pointer[fileID] += bytes_read;
	return bytes_read;
}

//Removes a file from the filesystem
int ssfs_remove(char* name){
	//Check if file is in the root directory
	int root_index = 0;
	while(root_index < DIRECTORY_SIZE && strcmp(root_dir.filename[root_index], name) != 0)
	{
		root_index++;
	}
	if(root_index == DIRECTORY_SIZE)
	{
		printf("Cannot remove file, file with filename %s does not exist!\n", name);
		return -1;
	}

	//Otherwise, remove the file with root_index.
	//Find file's inode
	int inode_number = root_dir.inode_number[root_index];
	inode_t* inode_array = (inode_t*)malloc(sizeof(inode_t) * NODE_BLOCKS * NODES_PER_BLOCK);
	read_blocks(1, NODE_BLOCKS, inode_array);
	inode_t inode = inode_array[inode_number];	
	//Remove root
	strcpy(root_dir.filename[root_index], "");
	root_dir.inode_number[root_index] = -1;
	root_dir.size--;
	
	//Treat inode. This will require updating the free bit map
	read_blocks(FBM_BLOCK, 1, &bit_map);
	inode.size = -1;
	for(int i = 0; i < DIRECT_POINTERS; i++)
	{
		bit_map.bytes[inode.direct[i]] = 0xFF;
		inode.direct[i] = -1;
	}
	if(inode.indirect != -1)
	{
		int indirect_pointers[INDIRECT_POINTERS];
		read_blocks(inode.indirect, 1, &indirect_pointers);
		for(int i = 0; i < INDIRECT_POINTERS; i++)
		{
			if(indirect_pointers[i] >= 0)
			{
				bit_map.bytes[indirect_pointers[i]] = 0xFF;
			}
		}
	}
	inode.indirect = -1;
	block_t inodes[NODE_BLOCKS];
	inode_array[inode_number] = inode;
	memcpy(&inodes, inode_array, NODE_BLOCKS * BLOCK_SIZE);
	write_blocks(1, NODE_BLOCKS, &inodes);
	free(inode_array);

	//Also write root directory to disk
	write_blocks(NODE_BLOCKS + 1, 1, &root_dir);

	//Update free bit map
	write_blocks(FBM_BLOCK, 1, &bit_map);

	//Finally, check file descriptor table for the open file
	int table_index = 0; 
	while(table_index < TABLE_SIZE && fd_table.inode_number[table_index] != inode_number)
	{
		table_index++;
	}
	if(table_index == TABLE_SIZE)
	{
		//File is already closed
		return 0;
	}
	ssfs_fclose(table_index);
	return 0;
}

//Created a shadow of the file system 
int ssfs_commit(){
	return 0;
}

//Restore the file system to a previous shadow
int ssfs_restore(int cnum){
	return 0;
}
