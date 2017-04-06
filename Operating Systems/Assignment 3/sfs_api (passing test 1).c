#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sfs_api.h"
#include "disk_emu.h"
//ECSE 427 Assignment 3 ---- Simple Shadow File System
//@author thomaschristinck

#define BLOCK_SIZE 1024			//Size (in bytes) of a block
#define BLOCK_NUMBERS 1024		//Number of total blocks in the file system
#define NODE_NUMBERS 224 		//Number of I-Nodes in the file system
#define NODE_SIZE 64			//Size of an I-Node 
#define TABLE_SIZE 64			//Size of slots iin file table
#define SHADOW_NUMBERS 50		//Number of shadow roots
#define DIRECTORY_SIZE 64		//Limited by 1024 / (pointer size + filename size)
#define FBM_BLOCK (BLOCK_NUMBERS - 2)							//Block number of FBM block
#define WM_BLOCK (BLOCK_NUMBERS - 1)							//Block number of WM block
#define DIRECT_POINTERS ((NODE_SIZE/4) - 2)						//Number of direct pointers in an i-node - 14
#define NODE_BLOCKS (DIRECT_POINTERS)							//Number of blocks devoted to node storage - 14
#define NODES_PER_BLOCK (BLOCK_SIZE / NODE_SIZE)				//Number of nodes in a block - 8

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
	int system_size;
	int node_numbers; 
	inode_t root;
	inode_t shadow[5];
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

	/* FILE SYSTEM
	/--Super Block (0)--/-------Data Blocks------/--Free Bit Map (BLOCK_NUMBERS - 2)---/---Write Mask (BLOCK_NUMBERS - 1)---/
	Data Block (zoomed in)
	/---I-Node Blocks (1-14)----/-----Data Blocks----/
	*/
	
	if(fresh)
	{
		//Create a new file system
		init_fresh_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);

		//Set up super node fields. Super node is block 0 and holds the following:
		//-Magic-/-Block Size-/-# of blocks-/-# of i-nodes-/---Root (j-node)---/------Shadow Roots------/
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
		//Read in root dir, and file 
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
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
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
		int node_index = inode % DIRECT_POINTERS;
		int block_index = (inode - node_index) / DIRECT_POINTERS;
		int file_size = inode_array[block_index][node_index].size;

		//Set up fd table entry
		int table_index = 0; 
		while(table_index < TABLE_SIZE && fd_table.inode_number[table_index] != -1)
		{
			table_index++;
		}
		if(table_index == TABLE_SIZE)
		{
			printf("No more available slots in file descriptor table!\n");
			return -1;
		}
		else
		{
			fd_table.write_pointer[table_index] = file_size;
			fd_table.read_pointer[table_index] = 0;
			fd_table.inode_number[table_index] = inode;
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
			return -1;
		}
		root_index = 0;
		while(root_index < DIRECTORY_SIZE && root_dir.inode_number[root_index] > -1)
		{
			root_index ++;
		}
		if(root_index == DIRECTORY_SIZE)
		{
			printf("Big error! I must have made a mistake around here\n");
			return -1;
		}
		else
		{
			//Find free block (through free bit map)
			int free_index = 0;
			while(free_index < BLOCK_NUMBERS && bit_map.bytes[free_index] != 0xFF && write_mask.bytes[free_index] != 0xFF)
			{
				free_index ++;
			}
			if(free_index == BLOCK_NUMBERS)
			{
				printf("No free blocks available!\n");
				return -1;
			}
			bit_map.bytes[free_index] = 0xFE;
			write_mask.bytes[free_index] = 0xFF;
			
			//Have root index, now find free inode number
			int found = 0;
			int block_index;
			int node_index;
			for(block_index = 0; block_index < BLOCK_NUMBERS && found == 0; block_index ++)
			{
				for(node_index = 0; node_index < NODE_NUMBERS && found == 0; node_index ++)
				{
					if(inode_array[block_index][node_index].size == -1 )
					{
						found = 1;
					}
				}
			}
			block_index--;
			node_index--;
			int inode = (block_index * DIRECT_POINTERS) + node_index;
		
			//Find free slot in fd table
			int table_index = 0; 
			while(table_index < TABLE_SIZE && fd_table.inode_number[table_index] != -1)
			{
				table_index++;
			}
			if(table_index == TABLE_SIZE)
			{
				printf("No more available slots in file descriptor table!\n");
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
			inode_array[block_index][node_index].size++;
			inode_array[block_index][node_index].direct[0] = free_index;
			block_t inodes[NODE_BLOCKS];
			memcpy(&inodes, &inode_array, NODE_BLOCKS * BLOCK_SIZE);
			write_blocks(1, NODE_BLOCKS, &inodes);

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
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	int node_index = fd_table.inode_number[fileID] % DIRECT_POINTERS;
	int block_index = (fd_table.inode_number[fileID] - node_index) / DIRECT_POINTERS;
	inode_t inode = inode_array[block_index][node_index];

	if(loc > inode.size || loc < 0)
	{
		printf("Error! Cannot write from position %d in file of size %d!\n", loc, inode.size);
		return -1;
	}
	//Otherwise, adjust write pointer accordingly
	fd_table.read_pointer[fileID] = loc;
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
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	int node_index = fd_table.inode_number[fileID] % DIRECT_POINTERS;
	int block_index = (fd_table.inode_number[fileID] - node_index) / DIRECT_POINTERS;
	inode_t inode = inode_array[block_index][node_index];

	if(loc > inode.size || loc < 0)
	{
		printf("Error! Cannot write from position %d in file of size %d!\n", loc, inode.size);
		return -1;
	}
	//Otherwise, adjust write pointer accordingly
	fd_table.write_pointer[fileID] = loc;
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
	//Find inode
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	int node_index = fd_table.inode_number[fileID] % DIRECT_POINTERS;
	int block_index = (fd_table.inode_number[fileID] - node_index) / DIRECT_POINTERS;
	inode_t inode = inode_array[block_index][node_index];

	//Get write pointer. We need to figure out how many more blocks need to be set aside for the write (if any)
	int write_ptr = fd_table.write_pointer[fileID];
	int total_length = write_ptr + length;
	//Check if more blocks need to be allocated
	int total_needed = (total_length % BLOCK_SIZE == 0) ? total_length / BLOCK_SIZE : total_length / BLOCK_SIZE + 1;
	int total_allocated = (inode.size % BLOCK_SIZE == 0 && inode.size != 0) ? inode.size / BLOCK_SIZE : inode.size / BLOCK_SIZE + 1;
	int blocks_needed = total_needed - total_allocated;

	if(blocks_needed > 0)
	{
		//Find free blocks
		read_blocks(FBM_BLOCK, 1, &bit_map);
		read_blocks(WM_BLOCK, 1, &write_mask);
		int free_indexes[blocks_needed];
		int i = 0;
		int blocks_given = 0;
		while(i < BLOCK_NUMBERS && blocks_given < blocks_needed)
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
			printf("Error! Could not write because we coudn't find enough blocks!\n");
			return -1;
		}

		//Allocate blocks needed
		blocks_given = 0;
		int pointer_index = 0;
		while(blocks_given < blocks_needed && pointer_index < DIRECT_POINTERS)
		{
			if(inode.direct[pointer_index] == -1)
			{
				inode.direct[pointer_index] = free_indexes[blocks_given];
				blocks_given++;
			}
			pointer_index++;
		} 
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
	int start_block = write_ptr / BLOCK_SIZE;
	char* block_buf = malloc(BLOCK_SIZE);
	read_blocks(inode.direct[start_block], 1, block_buf);
	memcpy(block_buf + (write_ptr % BLOCK_SIZE), buf, offset);
	write_blocks(inode.direct[start_block], 1, block_buf);
	free(block_buf);
	bytes_written += offset;

	//Now if there are more bytes to write, we do so directly
	int blocks_written = 0;
	if(writeable_blocks > 0 && bytes_written < length)
	{
		for(start_block++; start_block < DIRECT_POINTERS 
			&& blocks_written < writeable_blocks 
			&& (bytes_written + BLOCK_SIZE < length); bytes_written += BLOCK_SIZE)
		{
			write_blocks(inode.direct[start_block],1, buf += bytes_written);
			start_block++;
		}
	}
	//Check if there is a remaining (final) block to be written
	if(bytes_written < length && start_block != DIRECT_POINTERS)
	{
		if(start_block == write_ptr / BLOCK_SIZE)
		{
			start_block++;
		}
		char* block_buf = malloc(BLOCK_SIZE);
		read_blocks(inode.direct[start_block], 1, block_buf);
		memcpy(block_buf, buf + bytes_written, length - bytes_written);
		write_blocks(inode.direct[start_block], 1, block_buf);
		free(block_buf);
		bytes_written += (length - bytes_written);
	}
	//Update size of file
	inode.size += (inode.size == write_ptr) ? bytes_written : (write_ptr + bytes_written - inode.size);
	//fd_table.write_pointer[fileID] += bytes_written;

	//Update inodes
	block_t inodes[NODE_BLOCKS];
	inode_array[block_index][node_index] = inode;
	memcpy(&inodes, &inode_array, NODE_BLOCKS * BLOCK_SIZE);
	write_blocks(1, NODE_BLOCKS, &inodes);
	
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
	//Find inode
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	int node_index = fd_table.inode_number[fileID] % DIRECT_POINTERS;
	int block_index = (fd_table.inode_number[fileID] - node_index) / DIRECT_POINTERS;
	inode_t inode = inode_array[block_index][node_index];	

	int read_ptr = fd_table.read_pointer[fileID];
	if(read_ptr + length > inode.size + 1)
	{
		printf("Error! Cannot read %d bytes when file size is %d bytes and read pointer is %d!\n", length, inode.size, read_ptr);
		return -1;
	}
	//Otherwise, find offset and find how many "center" blocks we read.
	int offset;
	if(read_ptr % BLOCK_SIZE + length < BLOCK_SIZE)
	{
		offset = length;
	}
	else
	{
		offset = BLOCK_SIZE - (read_ptr % BLOCK_SIZE);
	}
	int readable_blocks = length / BLOCK_SIZE;

	//Read first block
	int bytes_read = 0;
	int start_index = read_ptr / BLOCK_SIZE;
	char* read_buf = malloc(BLOCK_SIZE);
	read_blocks(inode.direct[start_index], 1, read_buf);
	memcpy(buf, read_buf + (read_ptr % BLOCK_SIZE), offset);
	free(read_buf);
	bytes_read += offset;

	//Read middle blocks
	if(bytes_read < length && readable_blocks > 0)
	{
		int blocks_read = 0;
		for(start_index++; blocks_read < readable_blocks 
			&& start_index < DIRECT_POINTERS
			&& (bytes_read + BLOCK_SIZE < length); bytes_read += BLOCK_SIZE)
		{
			read_blocks(inode.direct[start_index], 1, buf += bytes_read);
			start_index++;
		}

	}

	//Read last block
	if(bytes_read < length && start_index != DIRECT_POINTERS)
	{
		if(start_index == read_ptr / BLOCK_SIZE)
		{
			start_index++;
		}
		char* read_buf = malloc(BLOCK_SIZE);
		read_blocks(inode.direct[start_index], 1, read_buf);
		memcpy(buf + bytes_read, read_buf, length - bytes_read);
		free(read_buf);
		bytes_read += length - bytes_read;
	}
	
	return bytes_read;


}

//Removes a file from the filesystem
int ssfs_remove(char* name){
	//Check if file is in the root directory
	if(root_dir.size == 0)
	{
		printf("Cannot remove file, no files exist!\n");
		return -1;
	}
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
	//Find inode
	int inode_number = root_dir.inode_number[root_index];
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	int node_index = inode_number % DIRECT_POINTERS;
	int block_index = (inode_number - node_index) / DIRECT_POINTERS;
	inode_t inode = inode_array[block_index][node_index];
	//Remove root
	strcpy(root_dir.filename[root_index], "");
	root_dir.inode_number[root_index] = -1;
	root_dir.size--;
	//Treat inode
	inode.size = -1;
	for(int i = 0; i < DIRECT_POINTERS; i++)
	{
		inode.direct[i] = -1;
	}
	inode.indirect = -1;
	inode_array[block_index][node_index] = inode;
	block_t inodes[NODE_BLOCKS];
	inode_array[block_index][node_index] = inode;
	memcpy(&inodes, &inode_array, NODE_BLOCKS * BLOCK_SIZE);
	write_blocks(1, NODE_BLOCKS, &inodes);

	//Also write root directory to disk
	write_blocks(NODE_BLOCKS + 1, 1, &root_dir);

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