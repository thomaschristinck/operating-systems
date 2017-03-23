#ifndef A3_LIB_H
#define A3_LIB_H

//Creates the file system
void mkssfs(int fresh);

//Opens the given file
int ssfs_fopen(char* name);

//Close the given file
int ssfs_fclose(int fileID);

//Seek (read) to the location starting at beginning
int ssfs_frseek(int fileID, int loc);

//Seek (write) to the location starting at beginning
int ssfs_fwseek(int fileID, int loc);

//Write buf characters to disk
int ssfs_fwrite(int fileID, char* buf, int length);

//Read characters from disk into buf
int ssfs_fread(int fileID, char* buf, int length);

//Removes a character from the filesystem
int ssfs_remove(char* name);

//Created a shadow of the file system 
int ssfs_commit();

//Restore the file system to a previous shadow
int ssfs_restore(int cnum);

#endif
//A3_LIB_H