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
	//To write a file, copy root dir and copy the i-node pointing to it, then modify the i-node so it 
	//points to the new directory. Make a copy of the new root and move the old root to shadow roots 

	//read_blocks(int start_address, int nblocks, void *buffer);

	//NOTE: root directory is a data block pointed to by the first direct pointer of i-node 1 in block 1
	//Check if root dir has entries
	inode_t inode_array[NODE_BLOCKS][NODES_PER_BLOCK];
	read_blocks(1, NODE_BLOCKS, &inode_array);
	printf("1st Inode Size: %d Second Inode size: %d\n", inode_array[0][0].size, inode_array[0][1].size);

	block_t bit_map;
	read_blocks(FBM_BLOCK, 1, &bit_map);
	block_t write_mask;
	read_blocks(WM_BLOCK, 1, &write_mask);
	
	int root_index = 0;
	while(root_index < DIRECTORY_SIZE && strcmp(root_dir.filename[root_index], name) != 0)
	{
		root_index++;
	}
	//Check if we found file name. Fetch inode
	if(root_index != DIRECTORY_SIZE)
	{
		int inode = root_dir.inode_number[root_index];
		int block_index = inode / BLOCK_SIZE;
		int node_index = inode % BLOCK_SIZE;
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
		while(root_index < DIRECTORY_SIZE && root_dir.inode_number > -1)
		{
			root_index ++;
		}
		if(root_index == DIRECTORY_SIZE)
		{
			printf("FATAL ERROR! I must have made a mistake around here\n");
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
			printf("Block index: %d Node Index: %d\n", block_index, node_index);
			int inode = block_index * BLOCK_SIZE + (node_index * BLOCK_SIZE);
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

}

//Seek (write) to the location starting at beginning
int ssfs_fwseek(int fileID, int loc){

}

//Write buf characters to disk
int ssfs_fwrite(int fileID, char* buf, int length){
	//To write, read first and last block into a buffer (memcpy). Overwrite by offset.
	//Write into blocks directly. 
	
}

//Read characters from disk into buf
int ssfs_fread(int fileID, char* buf, int length){

}

//Removes a character from the filesystem
int ssfs_remove(char* name){

}





//Created a shadow of the file system 
int ssfs_commit(){

}

//Restore the file system to a previous shadow
int ssfs_restore(int cnum){

}