#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "a3_lib.h"
#include "disk_emu.h"
//ECSE 427 Assignment 3 ---- Simple Shadow File System
//@author thomaschristinck

#define BLOCK_SIZE 1024			//Size (in bytes) of a block
#define BLOCK_NUMBERS 10000		//Number of total blocks in the file system
#define NODE_NUMBERS 224 		//Number of I-Nodes in the file system
#define NODE_SIZE 64			//Size of an I-Node 
#define SHADOW_NUMBERS 50		//Number of shadow roots
#define FBM_BLOCK (BLOCK_NUMBERS - 3)							//Block number of FBM block
#define DIRECT_POINTERS ((NODE_SIZE/4) - 2)						//Number of direct pointers in an i-node
#define NODE_BLOCKS (BLOCK_SIZE / NODE_SIZE * DIRECT_POINTERS)	//Number of blocks devoted to node storage
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


void mkssfs(int fresh)
{
	char* filename = "thomasdisk";

	/* FILE SYSTEM

	/--Super Block--/-------Data Blocks------/--Free Bit Map---/---Write Mask---/
	
	//block 0 is the super block and holds the following:

	/-Magic-/-Block Size-/-# of blocks-/-# of i-nodes-/---Root (j-node)---/------Shadow Roots------/

	//Could memcpy super block (a struct) to block[0], then i-node blocks (also structs) could be
	//copied as well as the FBD and WM

	//To write a file, copy root dir and copy the i-node pointing to it, then modify the i-node so it 
	//points to the new directory. Make a copy of the new root and move the old root to shadow roots 
	*/ 
	if(fresh)
	{
		//Create a new file system
		init_fresh_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);

		//Set up super node fields
		superblock_t super;
		strcpy(super.magic, "0xABCD0000");
		super.block_size = BLOCK_SIZE;
		super.lastshadow = 0;
		
		//Initialize free bit map (all 1's, all blocks free), then write to second last block (BLOCK_NUMBERS -3)
		//Note that after bit_map.bytes[BLOCK_NUMBERS] the free bit map is invalid
		block_t bit_map;
		for(int i = 0; i < BLOCK_SIZE; i++)
		{
			bit_map.bytes[i] = 0xFF;
		}

		write_blocks(FBM_BLOCK, 1, &bit_map);	

		//Initialize write mask. Blocks that are not writeable include the inode blocks
		block_t write_mask;

		//Populate j-node. After this population process, the j-node should point to blocks 1 through 
		//DIRECT_POINTERS++. Next, fill these blocks with inodes
		super.root.size = 0;
		int block_index = 1;
		for(int i = 0; i < DIRECT_POINTERS; i++)
		{
			super.root.direct[i] = block_index;
			block_index++;
		}
		//Now each of these blocks should be full of inodes.
		block_t inodes[NODE_BLOCKS];
		inode_t inode;
		inode.size = 1;
		for(int i = 0; i < DIRECT_POINTERS; i++)
		{
			inode.direct[i] = 0;
		}
		//Copy null empty inodes into NODE_BLOCKS blocks
		for(int j = 0; j < NODE_BLOCKS; j++)
		{
			for(int k = 0; k < BLOCK_SIZE; k += NODE_SIZE)
			{
				memcpy(&inodes[j].bytes[k], &inode, NODE_SIZE);
			}
		}
		//Then write all inode blocks to the disk
		write_blocks(1, NODE_BLOCKS, &inodes);
		//Write super block to the disk
		//write_blocks(0, 1, &super);
	}
	else
	{
		//Open an old file system from the disk
		init_disk(filename, BLOCK_SIZE, BLOCK_NUMBERS);
	}
/*
	//This should all go to fresh (the below code in function)

	//Inode sample to memcpy into each slot in inodes array
	inode_t inode;
	inode.size = 1;
	for(int i = 0; i < DIRECT_POINTERS; i++)
	{
		inode.direct[i] = 0;
	}
	//Array should hold all I-Nodes
	inode_t inodes[NODE_BLOCKS][NODES_PER_BLOCK];
	for(int i = 0; i < NODE_BLOCKS; i++)
	{
		for(int j = 0; j < NODES_PER_BLOCK; j++)
		{
			memcpy(&inodes[i][j], &inode, sizeof(inode));
		}
	}
	//Write all i-nodes to the disk
	for(int i = 0; i < NODE_BLOCKS; i++)
	{
		for(int j = 0; j < NODES_PER_BLOCK; j++)
		{
			//Nodes_per_block * NODE_SIZE should FILL the block
			write_blocks(i, 1, &inodes[i][j]);
			//printf("i: %d j: %d\n", i, j);
		}
	}
	*/
}

//Opens the given file
int ssfs_fopen(char* name)
{

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