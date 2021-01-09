/***************************************************************
* Class: CSC-415-03 Spring 2020
* Group Name: Zeta 3
* Name: Dale Armstrong
* StudentID: 920649883
*
* Project: Assignment 3 - File System
* @file: FileSystem.h
*
* Description: This header file contains the defined macros,
*	file system structures, and prototypes for functions
*	used by a driver/shell for this file system.
****************************************************************/

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "fsLow.h"

#define SUPER_SIGNATURE 0x44616c6541726d73
#define SUPER_SIGNATURE2 0x736d7241656c6144
#define NUM_DIRECT 10				//Number of direct pointers to data blocks per file
#define NUM_INDIRECT 2				//Number of indirect pointer to data blocks per file
#define BLOCKS_PER_INODE 2			//Used to determine number of inodes

#define DIRECTORY_TYPE 1			//Used to signify directories
#define FILE_TYPE 2					//Used to signify files
#define USED_FLAG 0xFF
#define UNUSED_FLAG 0

#define MAX_DIRECTORIES 256			//Max directory depth
#define MAX_PATH_NAME 4096			//Max size of the path name
#define MAX_NAME_SIZE 128			//Max size of a file name
#define MAX_OPEN_FILES 256			//Max size of file descriptor table

#define FS_SEEK_SET 1				//Start of file
#define FS_SEEK_END 2				//End of file
#define FS_SEEK_CUR 3				//Current position

/* Volume Control Block */
typedef struct SuperBlock {
    uint64_t superSignature;		//Signature for file system
	uint64_t inodeStart;			//Pointer to inode starting block
    uint64_t numInodes;				//Total number of inodes
    uint64_t blocksUsedByInodes;	//Number of blocks reserved by inodes
    uint64_t usedInodes;			//Number of used inodes
    uint64_t bitVectorStart;		//Pointer to free blocks bitmap
    uint64_t bytesUsedByBitVector;  //Number of bytes used by bitVector
    uint64_t blocksUsedByBitVector; //Number of blocks reserved by bitVector
    uint64_t freeDataBlocks;		//The number of free blocks not in use
    uint64_t totalDataBlocks;		//Total Number of Data Blocks and Number of bits in BitVector
    uint64_t pointersPerIndirect;   //Max pointers per indirect block
    uint64_t maxFileSize;			//Max file size in bytes
    uint64_t maxBlocksPerFile;		//Max blocks per file
    uint64_t rootDataPointer;		//Pointer to root data block, also start of data blocks
    uint64_t superSignature2;		//Second signature for file system
} SuperBlock, *SuperBlock_p;

/* Inodes to point to data */
typedef struct Inode {
	uint8_t used;						//Whether this Inode is in use
	uint8_t type;						//File or Directory
	uint64_t inode;						//Inode number
	uint64_t parentInodeID;				//Pointer to parent inode
	uint64_t size;						//Size of file in bytes
	uint32_t blocksReserved;			//Number of blocks reserved for data
	uint8_t blocksIndirect;				//Number of blocks reserved for indirect blocks
	time_t dateModified;				//Date when the file/directory was last modified
	uint64_t directData[NUM_DIRECT]; 	//Pointers directly to data blocks
	uint64_t indirectData[NUM_INDIRECT];//Pointers to data block that points to other data blocks
} Inode, *Inode_p;

/* File Control Block */
typedef struct FCB {
	uint64_t inodeID;					//Number of inode
	char name[MAX_NAME_SIZE];			//Name of file
} FCB, *FCB_p;

/* File Descriptor */
typedef struct FileDescriptor {
	uint8_t used;						//If file descriptor is in use
	Inode_p inode;
	uint64_t byteOffset;				//Pointer where to start read/write
} FileDescriptor, *FileDescriptor_p;

typedef struct WorkingDirectory {
	uint64_t inodeID;					//Inode ID of the current directory
	char wdPath[MAX_PATH_NAME];			//Full path name to current directory
	uint32_t pathLength;				//Stored path length
} WorkingDirectory, *WorkingDirectory_p;

extern SuperBlock_p sb;					//Global super block
extern uint8_t* bitVector;				//Global bit vector
extern WorkingDirectory_p wd;			//Global working directory
extern FileDescriptor_p fdTable;		//Global file table

/**
 * Checks for the filesystem on this partition, by checking the signatures of the first block for a match.
 * @returns 1 if filesystem exists
 * @returns 0 if filesystem does not exist.
 */
int check_fs();

/**
 * Formats the current partition and installs a new filesystem.
 * @returns 0 if format was successful
 * @returns -1 if format was unsuccessful
 */
int fs_format();

/** Outputs data about the current filesystem */
void fs_lsfs();

/** Lists the files in the current directory */
void fs_ls();

/**
 * Creates a directory of the given name.
 * @param directoryName the name of the directory to create
 * @returns 0 if sucessful
 * @returns -1 if it could not create directory
 */
int fs_mkdir(char* directoryName);

/**
 * Creates a file of the given name and size
 * @param fileName the name of the file to create
 * @param size the size in bytes to determine the number of blocks to reserve
 * @returns 0 if sucessful
 * @returns -1 if it could not create file
 */
int fs_mkfile(char* fileName, uint64_t size);

/**
 * Deletes the directory with the given name.
 * @param directoryName the name of the directory to delete
 * @returns 1 if successful
 * @returns 0 if it could not delete directory
 * @returns -1 if not a directory
 */
int fs_rmdir(char* directoryName);

/**
 * Copies the file from source to destination
 * @param sourceFile the source file to copy from
 * @param destFile the destination file to copy to
 * @returns 0 if successful
 * @returns -1 if could not copy file
 * @returns -2 if source file does not exist
 */
int fs_cp(char* sourceFile, char* destFile);

/**
 * Moves the file from source to destination. If the destination is a directory
 * file will be moved into the directory. If the destination is a file, the file will
 * be overwritten by source file.
 * @param sourceFile the source file to move
 * @param destFile the destination of the file
 * @returns 0 if successful
 * @returns -1 if could not move file
 * @returns -2 if source file does not exist
 */
int fs_mv(char* sourceFile, char* destFile);

/**
 * Deletes the file with the given name. Frees up any allocated blocks
 * and removes the file from the directory.
 * @param filename the name of the file to remove
 * @returns 0 if successful
 * @returns -1 if could not delete file
 * @returns -2 if file does not exist
 */
int fs_rm(char* filename);

/**
 * Copies a file from another filesystem to destination
 * in current filesystem.
 * @param sourceFile the source file from another file system to copy from
 * @param destFile the destination file in this file system to copy the file to
 * @returns 0 if successful
 * @returns -1 if unsuccessful
 * @returns -2 if could not open source file
 * @returns -3 if could not open destination file
 */
int fs_cpin(char* sourceFile, char* destFile);

/**
 * Copies a file from this filesystem to another filesystem
 * @param sourceFile the source file in this file system to copy from
 * @param destFile the destination file in another file system to copy the file to
 * @returns 0 if successful
 * @returns -1 if unsuccessful
 * @returns -2 if could not open source file
 * @returns -3 if could not open destination file
 */
int fs_cpout(char* sourceFile, char* destFile);

/**
 * Changes directory to the given path.
 * "cd /" or "cd" to go to root
 * @param path the path to parse and change directory to
 * @returns 0 if successful
 * @returns -1 if unsuccessful
 */
int fs_cd(char* path);

/**
 * Prints the full working directory
 */
void fs_pwd();

/**
 * Resizes the file. Maximum is capped by the number of available blocks to hold the requested size.
 * When increasing the size, if the size is greater than the currently allocated blocks,
 * new blocks will be wiped and allocated to the file.
 * Resize does not deallocate blocks, reserve command must be used.
 * @param file the file to resize
 * @param value the number of bytes to resize to
 * @returns 0 if successful
 * @returns -1 if could not resize file
 * @returns -2 if file could not be found
 */
int fs_resize(char* file, uint64_t size);

/**
 * Resizes the reserved blocks to hold the number of bytes requested. Minimum reserved is one block
 * or the minium blocks needed to hold the current size of the file. Maximum blocks are limited by
 * the number of available free blocks to be allocated.
 * @param file the file to change the reserved block size
 * @param value the size in bytes to calculate the number of blocks to reserve
 * @returns 0 if successful
 * @returns -1 if could not resize file
 * @returns -2 if file could not be found
 */
int fs_reserve(char* file, uint64_t size);

#endif
