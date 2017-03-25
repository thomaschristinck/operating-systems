#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "a3_lib.h"
#include "disk_emu.h"
//ECSE 427 Assignment 3 ---- Simple Shadow File System
//@author thomaschristinck

#define BLOCK_SIZE 1024			//Size (in bytes) of a block
#define BLOCK_NUMBERS 1024		//Number of total blocks in the file system
#define NODE_NUMBERS 224 		//Number of I-Nodes in the file system
#define NODE_SIZE 64			//Size of an I-Node 
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
	char filename[10][DIRECTORY_SIZE];
	int fpointer[DIRECTORY_SIZE];
} root_t;

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
		int check = write_blocks(0, 1, superblock);
		printf("Written blocks: %d\n", check);
		free(superblock);

		//Initialize free bit map (all 1's, all blocks free), then write to second last block (BLOCK_NUMBERS -3)
		//Note that after bit_map.bytes[BLOCK_NUMBERS] the free bit map is invalid
		block_t bit_map;
		for(int i = 0; i < BLOCK_SIZE; i++)
		{
			bit_map.bytes[i] = 0xFF;
		}

		write_blocks(FBM_BLOCK, 1, &bit_map);	

		//Initialize write mask. Initially all are writeable. Blocks that are not writeable include the inode 
		//blocks
		block_t write_mask;
		for(int i = 0; i < BLOCK_SIZE; i++)
		{
			write_mask.bytes[i] = 0xFF;
		}

		write_blocks(FBM_BLOCK, 1, &bit_map);

		
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
					inode_array[block_index][node_index].size = -1;
					inode_array[block_index][node_index].direct[i] = 0;
				}
			}
		}
		
		//Let the 15th block (1st data block be the root dir, and inode_array[0][0].direct[0] points to this) be the root dir
		memcpy(&inodes, &inode_array, NODE_SIZE);
		//Then write all inode blocks to the disk
		int check2 = write_blocks(1, NODE_BLOCKS, &inodes);
		printf("Written inode blocks: %d\n", check2);
	}
	else
	{
		//Open an old file system from the disk
		init_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);
	}
}

//Opens the given file
int ssfs_fopen(char* name)
{
	//To write a file, copy root dir and copy the i-node pointing to it, then modify the i-node so it 
	//points to the new directory. Make a copy of the new root and move the old root to shadow roots 

	//read_blocks(int start_address, int nblocks, void *buffer);

	//NOTE: root directory is a data block pointed to by the first direct pointer of i-node 1 in block 1
}

//Close the given file
int ssfs_fclose(int fileID)
{

}

//Seek (read) to the location starting at beginning
int ssfs_frseek(int fileID, int loc){

}

//Seek (write) to the location starting at beginning
int ssfs_fwseek(int fileID, int loc){

}

//Write buf characters to disk
int ssfs_fwrite(int fileID, char* buf, int length){

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