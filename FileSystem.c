/***************************************************************
* Class: CSC-415-03 Spring 2020
* Group Name: Zeta 3
* Name: Dale Armstrong
* StudentID: 920649883
*
* Project: Assignment 3 - File System
* @file: FileSystem.c
*
* Description: This file contains all the implementations of the
*	functions prototypes found in FileSystem.h. It also contains
*	all the additional helper functions needed for the driver
*	functions.
****************************************************************/

#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "FileSystem.h"

void freeGlobals();
uint64_t findNextPrime(uint64_t minBlockSize);
uint64_t hashInode(char* name, uint64_t parentInode);
uint64_t findFreeInode(char* name, uint64_t parentInode);
int64_t writeFile(uint8_t* source, Inode_p inode, uint64_t startPos, uint64_t length);
uint64_t readFile(uint8_t** destination, Inode_p inode, uint64_t startPos, uint64_t length);
int readInode(uint64_t inodeID, Inode_p* inodeBuffer);
int writeInode(Inode_p inode);
void deleteFile(Inode_p inode);
int addToDirectory(Inode_p dirInode, FCB_p newFile);
int removeFromDirectory(Inode_p dirInode, uint64_t fileInodeID);
uint64_t deallocateBlocks(Inode_p inode, uint64_t size);
int64_t allocateBlocks(Inode_p inode, uint64_t size);
uint64_t findFreeBlocks(uint64_t numberBlocksRequired, uint64_t** blockLocations);
void fillArray(uint64_t* blockLocations, uint64_t startValue, uint64_t length);
int bitUsed(uint64_t bit);
void setBitOn(uint64_t bitToSet);
void setBitOff(uint64_t bitToSet);
void readBitVector();
int writeBitVector();
int writeSuperBlock();
int saveMemory();
void wipePartition();
uint32_t parsePath(char* path, char** args);
int getFullPath(uint64_t parentID, uint64_t currentID);
int getLastInode(char* path, uint64_t* lastInode, bool saveLastArg, char* lastArg);
uint64_t createFile(char* path, uint8_t type, uint64_t size, bool haveDirInode, uint64_t dirInodeID);
int inodeContainsFile(Inode_p inode, const char* fileName, uint64_t* foundInodeID);
void initWorkingDirectory();
int fileOpen(char* file);
int inodeOpen(Inode_p inode);
int fileClose(int fd);
int64_t fileWrite(int fd, uint8_t* buffer, uint64_t length);
int64_t fileRead(int fd, uint8_t* buffer, uint64_t length);
int fileSeek(int fd, int64_t offset, uint8_t method);
void calculateDirSize(Inode_p inode, uint64_t* totalDirSize, uint64_t* totalDirReserved);
int64_t copyFile(Inode_p srcInode, Inode_p destDirInode, char* fileName);
int64_t copyDirectory(Inode_p srcDirInode, Inode_p destDirInode, char* fileName);

SuperBlock_p sb = NULL;
uint8_t* bitVector = NULL;
WorkingDirectory_p wd = NULL;
FileDescriptor_p fdTable = NULL;

/** flushes the input buffer */
static void flushInput() {
    char c;
    while ((c = getchar()) != '\n' && c != EOF)
        ; //discard characters
}

/**
 * Frees any globals set. This is used when formatting.
 */
void freeGlobals() {
	if (sb != NULL)
		free(sb);
	if (bitVector != NULL)
		free(bitVector);
	if (wd != NULL)
		free(wd);
	if (fdTable != NULL)
		free(fdTable);
}

/**
 * Finds the next closest prime number to the given minimum
 * block size. This allows for the hash function to be more
 * efficient.
 * This function uses the fact that all primes are either +-1
 * from a multiple of 6 to increase efficiency.
 * @param minBlockSize The minimum number of inodes
 * @returns the next closest Prime number to the given minBlockSize
 */
uint64_t findNextPrime(uint64_t minBlockSize) {
	bool prime = true;
	bool incrementByTwo = false;

	if (minBlockSize <= 3)
		return minBlockSize;

	//Start the value at +-1 from a multiple of 6
	uint64_t start = minBlockSize % 6;
	if (start == 0)
		minBlockSize++;
	else if (start > 1 && start < 5) {
		minBlockSize += (5 - start);
		incrementByTwo = true;
	}

	//Loop till a prime number is found
	while (true) {
		if (!prime) {
			if (incrementByTwo) {
				minBlockSize += 2;
				incrementByTwo = false;
			} else {
				minBlockSize += 4;
				incrementByTwo = true;
			}
			prime = true;
		}

		if (minBlockSize % 2 == 0) {
			prime = false;
			continue;
		} else if (minBlockSize % 3 == 0) {
			prime = false;
			continue;
		}

		uint64_t i = 5;
		while (i + 2 < minBlockSize) {
			if (minBlockSize % i == 0) {
				prime = false;
				break;
			} else if (minBlockSize % (i + 2) == 0) {
				prime = false;
				break;
			}
			i += 6;
		}
		if (prime)
			break;
	}
	return minBlockSize;
}

/**
 * Hashes the name of the file/folder along with the parent
 * inode and should return an unused inode
 * @param name the file name
 * @param parentInode the parent inode id
 * @returns the hash
 */
uint64_t hashInode(char* name, uint64_t parentInode) {
	uint64_t hash = 7243;

	while (*name) {
		hash += (hash << 5) + *name + parentInode;
		name++;
	}
	return hash % sb->numInodes;
}

/**
 * Finds and returns a free inode number in the inode table.
 * @param name the name of the file
 * @param the parent inode ID
 * @returns a free inode
 * @returns 0 if unsuccessful
 */
uint64_t findFreeInode(char* name, uint64_t parentInode) {
	if (sb->usedInodes >= sb->numInodes)
		return 0;

	//hash the name and parent inode to find the starting point
	uint64_t numberSearched = 0;
	uint64_t inodeID = hashInode(name, parentInode);
	uint64_t currentID = inodeID;

	//Check from the starting point and loop around until a free inode is found
	uint8_t* buffer = malloc(partInfop->blocksize * 2);
	uint64_t byteLocation = inodeID * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;
	LBAread(&buffer[partInfop->blocksize], 1, blockLocation);
	blockLocation++;
	while (numberSearched < sb->numInodes - 1) {
		memcpy(buffer, &buffer[partInfop->blocksize], partInfop->blocksize);
		if (blockLocation < sb->bitVectorStart) {
			LBAread(&buffer[partInfop->blocksize], 1, blockLocation);
		}

		while (offset < partInfop->blocksize * 2 - sizeof(Inode)) {
			if (buffer[offset] == UNUSED_FLAG) {
				free(buffer);
				return currentID;
			} else {
				numberSearched++;
				currentID++;
				if (currentID >= sb->numInodes) {
					currentID = 1;
					offset = sizeof(Inode);
					break;
				} else {
					offset += sizeof(Inode);
				}
			}
		}
		if (currentID == 1) {
			blockLocation = sb->inodeStart;
		} else {
			blockLocation++;
			offset -= partInfop->blocksize;
		}
	}
	free(buffer);
	return 0;
}

/**
 * Writes the buffer to the file data from starting position for length.
 * Will automatically allocate more blocks if writing beyond the reserved block size.
 * @param source the source buffer to write from
 * @param inode the inode of the file to write to
 * @param startPos the starting byte to write to
 * @param length the number of bytes to write
 * @returns number of bytes written
 * @returns -1 if requested writes are too large
 */
int64_t writeFile(uint8_t* source, Inode_p inode, uint64_t startPos, uint64_t length) {
	if (inode == NULL || source == NULL)
		return 0;

	uint64_t maxSize = startPos + length;
	if (maxSize > sb->maxFileSize)
		return -1;

	if (allocateBlocks(inode, maxSize) == -1) {
		return -1;
	}

	if (maxSize > inode->size)
		inode->size = maxSize;

	//Calculate the starting and ending blocks to write to
	uint64_t startingBlock = startPos / partInfop->blocksize;
	uint64_t endingBlock = (maxSize + partInfop->blocksize - 1) / partInfop->blocksize;
	uint8_t* blockBuffer = calloc(1, partInfop->blocksize);
	uint64_t* indirectBlockBuffer = calloc(NUM_INDIRECT, partInfop->blocksize);
	uint64_t blockToWrite;
	uint32_t j = 0;
	uint64_t srcPos = 0;

	//Loop through all blocks and write to the correct block
	for (uint64_t i = startingBlock; i < endingBlock; i++) {
		//If the block falls within the direct blocks
		if (i < NUM_DIRECT) {
			blockToWrite = sb->rootDataPointer + inode->directData[i];
		//If the block is within the first indirect block
		} else if (i < NUM_DIRECT + sb->pointersPerIndirect) {
			if (i == NUM_DIRECT || i == startingBlock)
				LBAread(indirectBlockBuffer, 1, inode->indirectData[0] + sb->rootDataPointer);

			blockToWrite = indirectBlockBuffer[i-NUM_DIRECT] + sb->rootDataPointer;
		//Block is in the second indirect block. Calculate which indirect block from the inode indirect block.
		} else {
			if (i == NUM_DIRECT + sb->pointersPerIndirect || i == startingBlock) {
				LBAread(&indirectBlockBuffer[sb->pointersPerIndirect], 1, inode->indirectData[1] + sb->rootDataPointer);
				if (i == startingBlock)
					j = (i - NUM_DIRECT - sb->pointersPerIndirect) / sb->pointersPerIndirect;
				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[sb->pointersPerIndirect + j] + sb->rootDataPointer);
			}

			if ((i - NUM_DIRECT - sb->pointersPerIndirect) % sb->pointersPerIndirect == 0) {
				j = (i - NUM_DIRECT - sb->pointersPerIndirect) / sb->pointersPerIndirect;
				if (j >= sb->pointersPerIndirect) {
					return 0;
				}

				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[sb->pointersPerIndirect + j] + sb->rootDataPointer);
			}
			blockToWrite = indirectBlockBuffer[((i - NUM_DIRECT) % sb->pointersPerIndirect)] + sb->rootDataPointer;
		}

		//If the bytes to write fit in one block
		if (startingBlock == (endingBlock - 1)) {
			LBAread(blockBuffer, 1, blockToWrite);
			memcpy(&blockBuffer[startPos % partInfop->blocksize], source, length);
			LBAwrite(blockBuffer, 1, blockToWrite);
		//If writing the last block
		} else if (i == endingBlock - 1) {
			LBAread(blockBuffer, 1, blockToWrite);
			uint64_t startPart = (length + startPos) % partInfop->blocksize;
			memcpy(blockBuffer, &source[srcPos], startPart);
			LBAwrite(blockBuffer, 1, blockToWrite);
			srcPos += startPart;
		//If writing the starting block
		} else if (i == startingBlock) {
			LBAread(blockBuffer, 1, blockToWrite);
			uint64_t endPart = partInfop->blocksize - (startPos % partInfop->blocksize);
			memcpy(&blockBuffer[startPos % partInfop->blocksize], source, endPart);
			LBAwrite(blockBuffer, 1, blockToWrite);
			srcPos += endPart;
		//If writing a full block
		} else {
			LBAwrite(&source[srcPos], 1, blockToWrite);
			srcPos += partInfop->blocksize;
		}
	}
	free(blockBuffer);
	free(indirectBlockBuffer);
	return length;
}

/**
 * Reads the entire data of the file/directory from the filesystem and stores it into the destination.
 * If the pointer is null, then memory will be allocated to hold the file data.
 * @param destination the buffer that the file data will be stored in.
 * @param inodeID the inode to read the data from
 * @param startPos the starting byte offset to read from the file
 * @param length is the number of bytes to read. 0 to read the entire file
 * @returns the number of bytes read into destination if successful
 * @returns 0 if unsuccessful
 */
uint64_t readFile(uint8_t** destination, Inode_p inode, uint64_t startPos, uint64_t length) {
	if (inode == NULL || inode->used == UNUSED_FLAG) {
		return 0;
	}
	if (startPos >= inode->size)
		return 0;

	uint64_t bytesToRead;
	uint64_t startingBlock;
	uint64_t endingBlock;
	uint64_t blockToRead;
	uint8_t* blockBuffer = calloc(1, partInfop->blocksize);
	uint64_t* indirectBlockBuffer = calloc(NUM_INDIRECT, partInfop->blocksize);

	//Calculate the number of bytes to read
	if (length == 0 || length + startPos > inode->size)
		bytesToRead = inode->size - startPos;
	else
		bytesToRead = length;

	if (*destination == NULL)
		*destination = calloc(1, bytesToRead);

	//Calculate the starting and ending block
	startingBlock = startPos / partInfop->blocksize;
	endingBlock = (startPos + bytesToRead + partInfop->blocksize - 1) / partInfop->blocksize;
	uint32_t j = 0;
	uint64_t destPos = 0;

	//Loop through and read the blocks into the buffer
	for (uint64_t i = startingBlock; i < endingBlock; i++) {
		if (i < NUM_DIRECT) {
			blockToRead = sb->rootDataPointer + inode->directData[i];
		} else if (i < NUM_DIRECT + sb->pointersPerIndirect) {
			if (i == NUM_DIRECT || i == startingBlock)
				LBAread(indirectBlockBuffer, 1, inode->indirectData[0] + sb->rootDataPointer);

			blockToRead = indirectBlockBuffer[i-NUM_DIRECT] + sb->rootDataPointer;
		} else {
			if (i == NUM_DIRECT + sb->pointersPerIndirect || i == startingBlock) {
				LBAread(&indirectBlockBuffer[sb->pointersPerIndirect], 1, inode->indirectData[1] + sb->rootDataPointer);
				if (i == startingBlock)
					j = (i - NUM_DIRECT - sb->pointersPerIndirect) / sb->pointersPerIndirect;
				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[sb->pointersPerIndirect + j] + sb->rootDataPointer);
			}

			if ((i - NUM_DIRECT - sb->pointersPerIndirect) % sb->pointersPerIndirect == 0) {
				j = (i - NUM_DIRECT - sb->pointersPerIndirect) / sb->pointersPerIndirect;
				if (j >= sb->pointersPerIndirect) {
					printf("Error: This filesystem does not support this large of a file size\n");
					return 0;
				}

				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[sb->pointersPerIndirect + j] + sb->rootDataPointer);
			}
			blockToRead = indirectBlockBuffer[((i - NUM_DIRECT) % sb->pointersPerIndirect)] + sb->rootDataPointer;
		}
		//If the bytes to read fit within one block
		if (startingBlock == (endingBlock - 1)) {
			LBAread(blockBuffer, 1, blockToRead);
			memcpy(&((*destination)[destPos]), &blockBuffer[startPos % partInfop->blocksize], bytesToRead);
		//If reading the last block
		} else if (i == endingBlock - 1) {
			LBAread(blockBuffer, 1, blockToRead);
			uint64_t startPart = (bytesToRead + startPos) % partInfop->blocksize;
			memcpy(&((*destination)[destPos]), blockBuffer, startPart);
			destPos += startPart;
		//If reading the first block
		} else if (i == startingBlock) {
			LBAread(blockBuffer, 1, blockToRead);
			uint64_t endPart = partInfop->blocksize - (startPos % partInfop->blocksize);
			memcpy(&((*destination)[destPos]), &blockBuffer[startPos % partInfop->blocksize], endPart);
			destPos += endPart;
		//If reading a whole block
		} else {
			LBAread(&((*destination)[destPos]), 1, blockToRead);
			destPos += partInfop->blocksize;
		}
	}
	free(indirectBlockBuffer);
	free(blockBuffer);
	return bytesToRead;
}

/**
 * Given an inode number and an Inode_p pointer, readInode
 * will read from the request inode into either a buffer already
 * preallocated, or will allocate a buffer if the pointer is null.
 * @param inodeID the ID number of the inode to read
 * @param inodeBuffer the inode buffer to read the inode into
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int readInode(uint64_t inodeID, Inode_p* inodeBuffer) {
	if (inodeID >= sb->numInodes)
		return 0;

	if (*inodeBuffer == NULL)
		*inodeBuffer = calloc(1, sizeof(Inode));

	//Find the block location and offset of the requested inode
	uint8_t* buffer = calloc(2, partInfop->blocksize);
	uint64_t byteLocation = inodeID * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;
	if (offset > partInfop->blocksize - sizeof(Inode))
		LBAread(buffer, 2, blockLocation);
	else
		LBAread(buffer, 1, blockLocation);
	memcpy(*inodeBuffer, &buffer[offset], sizeof(Inode));
	free(buffer);
	return 1;
}

/**
 * Given an Inode_p inode, writeInode will write out the contents
 * of the inode to the disk.
 * @param inode the inode to write back to disk
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int writeInode(Inode_p inode) {
	if (inode->inode >= sb->numInodes)
		return 0;

	//Find the block location and offset of the inode on disk to write to
	uint8_t* buffer = calloc(2, partInfop->blocksize);
	uint64_t byteLocation = inode->inode * sizeof(Inode) + partInfop->blocksize * sb->inodeStart;
	uint64_t blockLocation = byteLocation / partInfop->blocksize;
	uint64_t offset = byteLocation % partInfop->blocksize;

	//Check if inode spans two blocks
	if (offset > partInfop->blocksize - sizeof(Inode)) {
		LBAread(buffer, 2, blockLocation);
		memcpy(&buffer[offset], inode, sizeof(Inode));
		LBAwrite(buffer, 2, blockLocation);
	} else {
		LBAread(buffer, 1, blockLocation);
		memcpy(&buffer[offset], inode, sizeof(Inode));
		LBAwrite(buffer, 1, blockLocation);
	}
	free(buffer);
	return 1;
}

/**
 * Deletes the file. If the file is a directory, it will
 * recursively go through and delete all files and directories.
 * @param inode the inode to delete
 */
void deleteFile(Inode_p inode) {
	FCB_p directoryData = NULL;
	Inode_p newInode = NULL;

	//If a directory, recursively go through and delete the files
	if (inode->type == DIRECTORY_TYPE) {
		readFile((uint8_t**)(&directoryData), inode, 0, 0);
		uint32_t numberOfEntries = inode->size / sizeof(FCB);
		for (uint32_t i = 0; i < numberOfEntries; i++) {
			readInode(directoryData[i].inodeID, &newInode);
			deleteFile(newInode);
			free(newInode);
			newInode = NULL;
		}
		free(directoryData);
	}

	inode->size = 0;

	deallocateBlocks(inode, 0);
	inode->used = UNUSED_FLAG;
	sb->usedInodes--;
	writeInode(inode);
	saveMemory();
}

/**
 * Adds the new FCB to the directory
 * @param dirInode the directory inode to add the file control block
 * @param newFile the file control block of the file to add
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int addToDirectory(Inode_p dirInode, FCB_p newFile) {
	FCB_p directoryData = NULL;
	dirInode->size += sizeof(FCB);
	if (allocateBlocks(dirInode, dirInode->size) == -1) {
		dirInode->size -= sizeof(FCB);
		printf("Can not add to directory, not enough free blocks\n");
		return 0;
	}

	if (!readFile((uint8_t**)(&directoryData), dirInode, 0, 0))
		return 0;

	uint32_t numberOfEntries = dirInode->size / sizeof(FCB);
	memcpy(&directoryData[numberOfEntries - 1], newFile, sizeof(FCB));
	writeFile((uint8_t*)directoryData, dirInode, 0, dirInode->size);
	dirInode->dateModified = time(NULL);
	writeInode(dirInode);
	free(directoryData);
	return 1;
}

/**
 * Removes the file from the directory. Does not delete/free allocated blocks.
 * Use deleteFile to delete and free the blocks. Moves the last FCB into the slot.
 * @param dirInode the directory to remove the file's control block from
 * @param fileInodeID the file's inode ID
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int removeFromDirectory(Inode_p dirInode, uint64_t fileInodeID) {
	FCB_p directoryData = NULL;

	if (!readFile((uint8_t**)(&directoryData), dirInode, 0, 0))
		return 0;

	//Loop through the directory to find the FCB to remove
	uint64_t numberOfFiles = dirInode->size / sizeof(FCB);
	for (uint64_t i = 0; i < numberOfFiles; i++) {
		if (directoryData[i].inodeID == fileInodeID) {
			if (i != numberOfFiles - 1) {
				memcpy(&directoryData[i], &directoryData[numberOfFiles - 1], sizeof(FCB));
			}
			memset(&directoryData[numberOfFiles - 1], 0, sizeof(FCB));
			dirInode->dateModified = time(NULL);
			dirInode->size -= sizeof(FCB);
			deallocateBlocks(dirInode, partInfop->blocksize);
			writeInode(dirInode);
			writeFile((uint8_t*)directoryData, dirInode, 0, dirInode->size);
			free(directoryData);
			return 1;
		}
	}
	free(directoryData);
	return 0;
}

/**
 * Attempts to free up the blocks down to the new given byte size.
 * If size is less than inode size then it will deallocate down to inode size.
 * @param inode the inode of the file to deallocate blocks from
 * @param size the size in bytes to try to reduce the reserved blocks down to
 * @returns number of blocks deallocated
 */
uint64_t deallocateBlocks(Inode_p inode, uint64_t size) {
	if (inode == NULL)
		return 0;
	if (size < inode->size)
		size = inode->size;

	//Calculate the ending block from the given size
	uint64_t endBlock = (size + partInfop->blocksize - 1) / partInfop->blocksize;
	if (endBlock >= inode->blocksReserved)
		return 0;

	uint64_t blocksToFree = inode->blocksReserved - endBlock;
	if (blocksToFree == 0)
		return 0;

	uint64_t* indirectBlockBuffer = calloc(NUM_INDIRECT, partInfop->blocksize);
	uint64_t indirectLocation;
	uint64_t blocksFreed = 0;

	//Loop through all the blocks to free. Find their location, set the bit free, and decrement blocks in inode
	for (uint64_t i = 0; i < blocksToFree; i++) {
		//If the block falls in the second indirect data block pointer
		if (inode->blocksReserved > NUM_DIRECT + sb->pointersPerIndirect) {
			indirectLocation = inode->blocksReserved - NUM_DIRECT - sb->pointersPerIndirect - 1;

			if (i == 0) {
				LBAread(&indirectBlockBuffer[sb->pointersPerIndirect], 1, inode->indirectData[1] + sb->rootDataPointer);
				LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[indirectLocation / sb->pointersPerIndirect + sb->pointersPerIndirect] + sb->rootDataPointer);
			}
			setBitOff(indirectBlockBuffer[indirectLocation % sb->pointersPerIndirect]);
			inode->blocksReserved--;
			sb->freeDataBlocks++;
			blocksFreed++;
			if (indirectLocation % sb->pointersPerIndirect == 0) {
				setBitOff(indirectBlockBuffer[sb->pointersPerIndirect + indirectLocation / sb->pointersPerIndirect]);
				inode->blocksIndirect--;
				sb->freeDataBlocks++;
				blocksFreed++;
				if (indirectLocation == 0) {
					setBitOff(inode->indirectData[1]);
					inode->blocksIndirect--;
					sb->freeDataBlocks++;
					blocksFreed++;
				} else {
					LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[(indirectLocation - 1) / sb->pointersPerIndirect + sb->pointersPerIndirect] + sb->rootDataPointer);
				}
			}
		//If the block is in the first indirect pointer
		} else if (inode->blocksReserved > NUM_DIRECT) {
			indirectLocation = inode->blocksReserved - NUM_DIRECT - 1;
			if (i == 0 || inode->blocksReserved == NUM_DIRECT + sb->pointersPerIndirect) {
				LBAread(indirectBlockBuffer, 1, inode->indirectData[0] + sb->rootDataPointer);
			}
			setBitOff(indirectBlockBuffer[indirectLocation % sb->pointersPerIndirect]);
			inode->blocksReserved--;
			sb->freeDataBlocks++;
			blocksFreed++;
			if (indirectLocation == 0) {
				setBitOff(inode->indirectData[0]);
				inode->blocksIndirect--;
				sb->freeDataBlocks++;
				blocksFreed++;
			}
		//If the block is in a direct pointer
		} else {
			setBitOff(inode->directData[inode->blocksReserved - 1]);
			inode->blocksReserved--;
			sb->freeDataBlocks++;
			blocksFreed++;
		}
	}
	saveMemory();
	free(indirectBlockBuffer);
	return blocksFreed;
}

/**
 * Given the total size of bytes required, will allocate additional blocks
 * up to the total needed for the total bytes and assign them to the inode.
 * @param inode the inode to allocate blocks to
 * @param size the size in bytes to calculate the number of blocks to allocate
 * @returns the number of new blocks assigned
 * @returns -1 if not enough free blocks
 */
int64_t allocateBlocks(Inode_p inode, uint64_t size) {
	if (inode == NULL)
		return 0;

	//Check if it larger than the max file size for this filesystem
	if (size > sb->maxFileSize) {
		printf("Requested filesize is larger than max file size\n");
		return -1;
	}

	//Calculate the number of blocks needed
	int64_t totalBlocksNeeded = (size + partInfop->blocksize - 1) / partInfop->blocksize;
	if (totalBlocksNeeded == 0)
		totalBlocksNeeded = 1;


	if (totalBlocksNeeded <= inode->blocksReserved)
		return 0;

	//Add the additional blocks needed for indirect pointers if needed
	if (totalBlocksNeeded > NUM_DIRECT + sb->pointersPerIndirect) {
		totalBlocksNeeded += 2 + ((totalBlocksNeeded - NUM_DIRECT - 1) / sb->pointersPerIndirect) - inode->blocksIndirect;
	} else if (totalBlocksNeeded > NUM_DIRECT) {
		totalBlocksNeeded += 1 - inode->blocksIndirect;
	}

	//Subtract the blocks already reserved
	totalBlocksNeeded -= inode->blocksReserved;

	//Check if there are enough free blocks to allocate
	if (totalBlocksNeeded > sb->freeDataBlocks) {
		printf("Not enough free blocks to allocate to file\n");
		return -1;
	}

	uint64_t* blockLocations = NULL;
	bool needToWrite = false;
	uint64_t indirectLocation;
	uint64_t lastBlockRead;
	uint64_t i = 0;

	//Check if there are enough free blocks
	if (findFreeBlocks(totalBlocksNeeded, &blockLocations) != totalBlocksNeeded) {
		printf("Free Blocks not equal to total blocks needed\n");
		if (blockLocations != NULL)
			free(blockLocations);
		return -1;
	}

	uint64_t* clearBlock = calloc(1, partInfop->blocksize);
	uint64_t* indirectBlockBuffer = calloc(NUM_INDIRECT, partInfop->blocksize);

	//Fill the direct data blocks
	while (inode->blocksReserved < NUM_DIRECT && i < totalBlocksNeeded) {
		inode->directData[inode->blocksReserved] = blockLocations[i];
		LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
		inode->blocksReserved++;
		i++;
	}
	//Fill the first indirect data block
	while (inode->blocksReserved < NUM_DIRECT + sb->pointersPerIndirect && i < totalBlocksNeeded) {
		if (inode->blocksIndirect < 1) {
			inode->indirectData[0] = blockLocations[i];
			LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
			inode->blocksIndirect++;
			i++;
		}
		if (i == 0 || inode->blocksReserved == NUM_DIRECT)
			LBAread(indirectBlockBuffer, 1, inode->indirectData[0] + sb->rootDataPointer);

		indirectBlockBuffer[inode->blocksReserved - NUM_DIRECT % sb->pointersPerIndirect] = blockLocations[i];
		LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
		inode->blocksReserved++;
		i++;
		needToWrite = true;
	}
	if (needToWrite) {
		LBAwrite(indirectBlockBuffer, 1, inode->indirectData[0] + sb->rootDataPointer);
		needToWrite = false;
	}
	//Calculate the correct indirect data block pointed to from the second indirect data block and fill
	while (i < totalBlocksNeeded) {
		if (inode->blocksIndirect < 2) {
			inode->indirectData[1] = blockLocations[i];
			LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
			inode->blocksIndirect++;
			i++;
		}

		if (i == 0 || inode->blocksReserved == NUM_DIRECT + sb->pointersPerIndirect) {
			LBAread(&indirectBlockBuffer[sb->pointersPerIndirect], 1, inode->indirectData[1] + sb->rootDataPointer);
		}

		indirectLocation = (inode->blocksReserved - NUM_DIRECT - sb->pointersPerIndirect);
		if (i == 0 || (indirectLocation % sb->pointersPerIndirect == 0)) {
			indirectLocation /= sb->pointersPerIndirect;

			if (inode->blocksIndirect < 3 + indirectLocation) {
				indirectBlockBuffer[sb->pointersPerIndirect + indirectLocation] = blockLocations[i];
				LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
				LBAwrite(&indirectBlockBuffer[sb->pointersPerIndirect], 1, inode->indirectData[1] + sb->rootDataPointer);
				inode->blocksIndirect++;
				i++;
			}
			indirectLocation += sb->pointersPerIndirect;
			if (needToWrite) {
				LBAwrite(indirectBlockBuffer, 1, indirectBlockBuffer[indirectLocation - 1] + sb->rootDataPointer);
				needToWrite = false;
			}
			LBAread(indirectBlockBuffer, 1, indirectBlockBuffer[indirectLocation] + sb->rootDataPointer);
			lastBlockRead = indirectLocation;
		}
		indirectBlockBuffer[(inode->blocksReserved - NUM_DIRECT - sb->pointersPerIndirect) % sb->pointersPerIndirect] = blockLocations[i];
		LBAwrite(clearBlock, 1, blockLocations[i] + sb->rootDataPointer);
		inode->blocksReserved++;
		i++;
		needToWrite = true;
	}
	if (needToWrite) {
		LBAwrite(indirectBlockBuffer, 1, indirectBlockBuffer[lastBlockRead] + sb->rootDataPointer);
	}

	writeInode(inode);
	free(blockLocations);
	free(clearBlock);
	free(indirectBlockBuffer);
	return totalBlocksNeeded;
}

/**
 * Looks for the given number of freeblocks and returns an array
 * with the blocks that are free.
 * @param numberBlocksRequired the number of blocks requested
 * @param blockLocations NULL buffer that will store the block locations
 * @returns number of blocks found
 * @returns 0 if unsuccessful
 */
uint64_t findFreeBlocks(uint64_t numberBlocksRequired, uint64_t** blockLocations) {
	if (numberBlocksRequired > sb->freeDataBlocks) {
		printf("Error: Number of blocks required exceeds number of free blocks\n");
		return 0;
	}

	if (*blockLocations != NULL)
		free(*blockLocations);

	*blockLocations = calloc(numberBlocksRequired, sizeof(uint64_t));

	uint64_t numberBlocksFound = 0;
	uint64_t largestHolePos = 0;
	uint64_t largestHoleLength = 0;
	uint64_t currentHolePos = 0;
	uint64_t currentHoleLength = 0;
	uint64_t currentBit = 0;
	//Iterate through bitvector to find the first set of requested contiguous blocks
	//If there isn't a large enough set of contiguous blocks, use the largest sets
	//until all blocks have been allocated.
	while (numberBlocksFound < numberBlocksRequired) {
		for (uint64_t i = 0; i < sb->bytesUsedByBitVector; i++) {
			//Check if entire byte is used
			if (bitVector[i] == USED_FLAG) {
				currentBit += 8;
				if (currentHoleLength > largestHoleLength) {
					largestHolePos = currentHolePos;
					largestHoleLength = currentHoleLength;
				}
				currentHolePos = 0;
				currentHoleLength = 0;
			//Check if entire byte is unused
			} else if (bitVector[i] == UNUSED_FLAG) {
				if (currentHolePos == 0) {
					currentHolePos = currentBit;
				}
				if (numberBlocksRequired - numberBlocksFound - currentHoleLength > 8) {
					currentHoleLength += 8;
					currentBit += 8;
				} else {
					currentHoleLength += numberBlocksRequired - numberBlocksFound - currentHoleLength;
					fillArray(&((*blockLocations)[numberBlocksFound]), currentHolePos, currentHoleLength);
					for (uint64_t j = numberBlocksFound; j < numberBlocksRequired; j++)
						setBitOn((*blockLocations)[j]);
					sb->freeDataBlocks -= currentHoleLength;
					return numberBlocksFound + currentHoleLength;
				}
			//Partial byte is used, iterate through byte
			} else {
				for (; currentBit < (i+1) * 8 && currentBit < sb->totalDataBlocks; currentBit++) {
					if (!bitUsed(currentBit)) {
						if (currentHolePos == 0) {
							currentHolePos = currentBit;
						}
						currentHoleLength++;
						if (currentHoleLength == numberBlocksRequired - numberBlocksFound) {
							fillArray(&((*blockLocations)[numberBlocksFound]), currentHolePos, currentHoleLength);
							for (uint64_t j = numberBlocksFound; j < numberBlocksRequired; j++)
								setBitOn((*blockLocations)[j]);
							sb->freeDataBlocks -= currentHoleLength;
							return numberBlocksFound + currentHoleLength;
						}
					} else {
						if (currentHoleLength > largestHoleLength) {
							largestHolePos = currentHolePos;
							largestHoleLength = currentHoleLength;
						}
						currentHolePos = 0;
						currentHoleLength = 0;
					}
				}
			}
		}
		if (currentHoleLength > largestHoleLength) {
			largestHolePos = currentHolePos;
			largestHoleLength = currentHoleLength;
		}
		currentHolePos = 0;
		currentHoleLength = 0;
		//fill the array with the largest hole and iterate through bitvector again
		if (largestHoleLength != 0) {
			fillArray(&((*blockLocations)[numberBlocksFound]), largestHolePos, largestHoleLength);
			for (uint64_t j = numberBlocksFound; j < numberBlocksFound + largestHoleLength; j++)
				setBitOn((*blockLocations)[j]);
			numberBlocksFound += largestHoleLength;
			sb->freeDataBlocks -= largestHoleLength;
		} else {
			printf("Error: Problem with finding free blocks\n");
			return 0;
		}
		largestHolePos = 0;
		largestHoleLength = 0;
		currentBit = 0;
	}
	return 0;
}

/**
 * Used to fill the array with values start to finish. Helper function
 * for finding free blocks.
 * @param blockLocations the array to fill
 * @param startValue the starting block location to put into the array
 * @param length the number of block locations to put in the array
 */
void fillArray(uint64_t* blockLocations, uint64_t startValue, uint64_t length) {
	for (uint64_t i = 0; i < length; i++) {
		blockLocations[i] = startValue + i;
	}
}

/**
 * Returns whether the bit is set or not
 * @param bit the bit to check
 * @returns 1 if set
 * @returns 0 if not set
 */
int bitUsed(uint64_t bit) {
	return bitVector[bit / 8] & (1 << (bit % 8));
}

/**
 * Sets the bit to one/used
 * @param bitToSet the bit to set
 */
void setBitOn(uint64_t bitToSet) {
	bitVector[bitToSet / 8] |= (1 << (bitToSet % 8));
}

/**
 * Sets the bit to zero/free
 * @param bitToSet the bit to set
 */
void setBitOff(uint64_t bitToSet) {
	bitVector[bitToSet / 8] &= ~(1 << (bitToSet % 8));
}

/**
 * Allocates memory for bit vector and reads the bit vector from
 * the drive.
 */
void readBitVector() {
	bitVector = calloc(sb->blocksUsedByBitVector, partInfop->blocksize);
	LBAread(bitVector, sb->blocksUsedByBitVector, sb->bitVectorStart);
}

/**
 * Writes the current bitVector to drive
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int writeBitVector() {
	if (LBAwrite(bitVector, sb->blocksUsedByBitVector, sb->bitVectorStart) == 0) {
		printf("Could not write bit vector to drive\n");
		return 0;
	}
	return 1;
}

/**
 * Writes the current superblock to drive
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int writeSuperBlock() {
	if (LBAwrite(sb, 1, 0) == 0) {
		printf("Could not write super block to drive\n");
		return 0;
	}
	return 1;
}

/**
 * Writes bitVector and SuperBlock to drive
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int saveMemory() {
	if (!writeSuperBlock() || !writeBitVector()) {
		printf("Could not save memory\n");
		return 0;
	}
	return 1;
}

/**
 * Sets the entire partition to 0
 */
void wipePartition() {
	printf("Please wait, wiping partition....");
	fflush(stdout);
	uint8_t* buffer = calloc(1, partInfop->blocksize);
	for (uint64_t i = 0; i < partInfop->numberOfBlocks; i++)
		LBAwrite(buffer, 1, i);
	free(buffer);
	printf("Done!\n");
}

/**
 * Parse path string into separate arguments
 * @param path the path to parse
 * @param args the buffer to store the pointers for the arguments
 * @returns the number of arguments
 */
uint32_t parsePath(char* path, char** args) {
	uint32_t i = 0;
    char* token = strtok(path, "/");
    while (token != NULL) {
        args[i] = token;
        token = strtok(NULL, "/");
        i++;
    }
    args[i] = NULL;
    return i;
}

/**
 * Get working directory full path through recursion
 * @param parentID the parent inode ID of the current directory
 * @param currentID the inode ID of the current directory
 * @returns the length of the pathname
 * @returns 0 if failed
 */
int getFullPath(uint64_t parentID, uint64_t currentID) {
	int retval;
	//Base case
	if (currentID == 0) {
		strcpy(wd->wdPath, "/");
		return 1;
	}

	//Recursively call getFullPath
	Inode_p inode = NULL;
	readInode(parentID, &inode);
	retval = getFullPath(inode->parentInodeID, parentID);
	if (retval == 0) {
		return 0;
	}

	//Find name of file
	FCB_p directoryData = NULL;
	int fcbLocation = -1;
	readFile((uint8_t**)(&directoryData), inode, 0, 0);
	uint32_t numberOfEntries = inode->size / sizeof(FCB);
	for (uint32_t i = 0; i < numberOfEntries; i++) {
		if (currentID == directoryData[i].inodeID) {
			fcbLocation = i;
			break;
		}
	}
	if (fcbLocation == -1) {
		return 0;
	}

	//Check if the path name is too long
	int length = strlen(directoryData[fcbLocation].name);
	if (length + retval > MAX_PATH_NAME) {
		free(inode);
		free(directoryData);
		printf("Error path is too long\n");
		return retval;
	}

	strcat(wd->wdPath, directoryData[fcbLocation].name);
	if (directoryData[fcbLocation].inodeID != wd->inodeID)
		strcat(wd->wdPath, "/");
	free(inode);
	free(directoryData);
	return length + retval;
}

/**
 * Traverses the given path and sets the last inode to lastInode. If saveLastArg
 * is true, then lastArg will be set to the last arg and the last inode id will be the directory
 * containing the lastArg.
 * @param path the path to parse and traverse
 * @param lastInode a provided uint64_t to store the last inode ID
 * @param saveLastArg whether to save the last argument in lastArg
 * @param lastArg if saveLastArg is set, will save the last argument in this buffer
 * @returns 1 if found
 * @returns 0 if not found
 */
int getLastInode(char* path, uint64_t* lastInode, bool saveLastArg, char* lastArg) {
	char* args[MAX_DIRECTORIES] = {NULL};
	Inode_p currentInode = NULL;
	uint32_t numArgs = 0;
	uint64_t strlength;
	char* pathCopy;

	//Check if from the current directory or from root
	if (path == NULL || path[0] == '/') {
		*lastInode = 0;
	} else {
		*lastInode = wd->inodeID;
	}

	//Make a copy of the path string
	if (path != NULL) {
		strlength = strlen(path) + 1;
		pathCopy = malloc(strlength);
		memcpy(pathCopy, path, strlength);
		numArgs = parsePath(pathCopy, args);
		if (saveLastArg)
			numArgs--;
	}

	//Iterate through all arguments to find next inode
	for (uint32_t i = 0; i < numArgs; i++) {
		readInode(*lastInode, &currentInode);
		//If we're saving the last argument, then all arguments must be directories
		//If we're not saving, then all arguments except the last must be directories
		if (currentInode->type != DIRECTORY_TYPE) {
			if (saveLastArg || i != numArgs - 1) {
				free(currentInode);
				free(pathCopy);
				return 0;
			}
		}
		//Check if moving up a directory
		if (strcmp(args[i], ".") == 0) {
			continue;
		} else if (strcmp(args[i], "..") == 0) {
			*lastInode = currentInode->parentInodeID;
		} else {
			//Check if the folder contains the file or directory
			if (inodeContainsFile(currentInode, args[i], lastInode) == 0) {
				free(currentInode);
				free(pathCopy);
				return 0;
			}
		}
	}
	if (currentInode != NULL)
		free(currentInode);

	//Save last argument if requested
	if (path != NULL && saveLastArg && lastArg != NULL) {
		if (strlen(args[numArgs]) + 1 > MAX_NAME_SIZE) {
			printf("Name is too long, max size is:%d\n", MAX_NAME_SIZE);
			free(pathCopy);
			return 0;
		}
		strcpy(lastArg, args[numArgs]);
	}
	free(pathCopy);
	return 1;
}

/**
 * Creates the file type at the given path and reserves enough blocks for the size.
 * If file already exists or there are not sufficient free blocks, will return.
 * @param path the path to parse, with the name of the file or directory to place the file in
 * @param type the type of file (file or directory)
 * @param size the size in bytes to calculate the number of blocks to reserve
 * @param haveDirInode if set, will use dirInodeID as the directory to place the file and path as the filename
 * @param dirInodeID if haveDirInode is set, will contain the directory inode id to place the file
 * @returns the inodeID of the new file
 * @returns 0 if unsuccessful
 */
uint64_t createFile(char* path, uint8_t type, uint64_t size, bool haveDirInode, uint64_t dirInodeID) {
	uint64_t lastInode;
	uint64_t newInodeID;
	char fileName[MAX_NAME_SIZE];
	Inode_p newInode = NULL;
	Inode_p previousInode = NULL;
	FCB_p directoryData = NULL;

	//If directory is already given
	if (haveDirInode) {
		lastInode = dirInodeID;
		strcpy(fileName, path);
	} else if (!getLastInode(path, &lastInode, true, fileName)) {
		printf("Could not find %s\n", path);
		return 0;
	}

	//Check if filename is already in the directory
	readInode(lastInode, &previousInode);
	if (inodeContainsFile(previousInode, fileName, &newInodeID)) {
		free(previousInode);
		return 0;
	}

	//Get a new inode ID for new file
	newInodeID = findFreeInode(fileName, lastInode);
	if (newInodeID == 0)
		return 0;

	//Create new file, allocate blocks, and put the file in the directory
	FCB_p newFCB = calloc(1, sizeof(FCB));
	newFCB->inodeID = newInodeID;
	strcpy(newFCB->name, fileName);
	previousInode->size += sizeof(FCB);
	if (allocateBlocks(previousInode, previousInode->size) == -1) {
		free(previousInode);
		free(newFCB);
		return 0;
	}
	previousInode->dateModified = time(NULL);
	readFile((uint8_t**)(&directoryData), previousInode, 0, 0);
	uint32_t numberOfEntries = previousInode->size / sizeof(FCB);
	memcpy(&directoryData[numberOfEntries - 1], newFCB, sizeof(FCB));

	writeFile((uint8_t*)directoryData, previousInode, 0, previousInode->size);
	writeInode(previousInode);

	//Create and set the new inode of the file
	newInode = calloc(1, sizeof(Inode));
	newInode->used = USED_FLAG;
	newInode->type = type;
	newInode->dateModified = time(NULL);
	newInode->inode = newInodeID;
	newInode->parentInodeID = lastInode;
	newInode->size = 0;
	newInode->blocksReserved = 0;
	newInode->blocksIndirect = 0;
	allocateBlocks(newInode, size);
	writeInode(newInode);
	sb->usedInodes++;
	saveMemory();

	free(newFCB);
	free(directoryData);
	free(newInode);
	free(previousInode);
	return newInodeID;
}

/**
 * Checks if the inode directory contains the filename, returns if the inode is found.
 * If the inode is found, it will update the foundInodeID with the ID number.
 * @param inode the directory inode to search
 * @param fileName the name of the file to search for
 * @param foundInodeID if the name is found, will store the inode id of the file here
 * @returns 1 if found
 * @returns 0 if not found or error
 */
int inodeContainsFile(Inode_p inode, const char* fileName, uint64_t* foundInodeID) {
	if (inode == NULL) {
		printf("Inode is null\n");
		return 0;
	}
	if (inode->used != USED_FLAG) {
		printf("Inode not in use\n");
		return 0;
	}
	if (inode->type != DIRECTORY_TYPE) {
		printf("Inode is not a directory\n");
		return 0;
	}

	//Check directory for fileName
	FCB_p currentDirectoryData = NULL;
	readFile((uint8_t**)(&currentDirectoryData), inode, 0, 0);
	uint64_t numberOfFiles = inode->size / sizeof(FCB);
	for (uint64_t i = 0; i < numberOfFiles; i++) {
		if (strcmp(currentDirectoryData[i].name, fileName) == 0) {
			*foundInodeID = currentDirectoryData[i].inodeID;
			free(currentDirectoryData);
			return 1;
		}
	}
	free(currentDirectoryData);
	return 0;
}

/**
 * Initializes the working directory to root
 */
void initWorkingDirectory() {
	wd = calloc(1, sizeof(WorkingDirectory));
	wd->inodeID = 0;
	strcpy(wd->wdPath, "/");
	wd->pathLength = strlen(wd->wdPath);
}

/**
 * Finds the file at the given path and copies the inode
 * to the file descriptor table.
 * @param file the path or filename of the file to open
 * @returns the file descriptor index in the file descriptor table
 * @returns -1 if unsuccessful
 */
int fileOpen(char* file) {
	int32_t i = 0;
	uint64_t inodeID;
	Inode_p inode = NULL;

	//Get the inode ID of the file
	if (!getLastInode(file, &inodeID, false, NULL))
		return -1;

	//Get the inode of the file
	if (!readInode(inodeID, &inode)) {
		free(inode);
		return -1;
	}

	while (i < MAX_OPEN_FILES && fdTable[i].used == USED_FLAG)
		i++;

	if (i >= MAX_OPEN_FILES) {
		free(inode);
		return -1;
	}

	//Set and return the file descriptor in the table
	fdTable[i].used = USED_FLAG;
	fdTable[i].byteOffset = 0;
	fdTable[i].inode = inode;
	return i;
}

/**
 * Copies the inode to the file descriptor table.
 * @param inode the inode to open
 * @returns the file descriptor index in the file descriptor table
 * @returns -1 if unsuccessful
 */
int inodeOpen(Inode_p inode) {
	uint64_t i = 0;
	Inode_p thisInode = NULL;
	readInode(inode->inode, &thisInode);

	while (i < MAX_OPEN_FILES && fdTable[i].used == USED_FLAG)
		i++;

	if (i >= MAX_OPEN_FILES) {
		free(thisInode);
		return -1;
	}

	//Set and return the file descriptor in the table
	fdTable[i].used = USED_FLAG;
	fdTable[i].byteOffset = 0;
	fdTable[i].inode = thisInode;
	return i;
}

/**
 * Closes the file and removes it from the file descriptor table
 * @param fd the file descriptor to close
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int fileClose(int fd) {
	if (fdTable[fd].used == UNUSED_FLAG)
		return 0;

	if (fdTable[fd].inode != NULL) {
		free(fdTable[fd].inode);
		fdTable[fd].inode = NULL;
	}

	fdTable[fd].used = UNUSED_FLAG;
	return 1;
}

/**
 * Writes to the file in the file descriptor, length in bytes
 * from the given buffer.
 * @param fd the file descriptor to write to
 * @param buffer the source data to write from
 * @param length the number of bytes to write
 * @returns the number of bytes written
 * @returns -1 if error
 */
int64_t fileWrite(int fd, uint8_t* buffer, uint64_t length) {
	if (fdTable[fd].used == UNUSED_FLAG || length == 0)
		return 0;

	uint64_t bytesWritten = writeFile(buffer, fdTable[fd].inode, fdTable[fd].byteOffset, length);
	if (bytesWritten == -1)
		return -1;
	fdTable[fd].byteOffset += bytesWritten;
	return bytesWritten;
}

/**
 * Reads from the file in the file descriptor, length in bytes
 * from the given buffer.
 * @param fd the file descriptor to read from
 * @param buffer the buffer to store the read bytes
 * @param length the number of bytes to read
 * @returns the number of bytes written
 * @returns -1 if error
 */
int64_t fileRead(int fd, uint8_t* buffer, uint64_t length) {
	if (fdTable[fd].used == UNUSED_FLAG || length == 0)
		return 0;

	uint64_t bytesRead = readFile(&buffer, fdTable[fd].inode, fdTable[fd].byteOffset, length);
	if (bytesRead == -1)
		return -1;
	fdTable[fd].byteOffset += bytesRead;
	return bytesRead;
}

/**
 * Moves the file pointer to the given offset from the position
 * @param fd the file descriptor to modify
 * @param offset the number of bytes to offset from the method
 * @param method the method (start, end, cur) to initially set the pointer
 * @returns 1 if successful
 * @returns 0 if unsuccessful
 */
int fileSeek(int fd, int64_t offset, uint8_t method) {
	if (fdTable[fd].used == UNUSED_FLAG)
		return 0;

	int64_t newPosition;
	if (method == FS_SEEK_SET) {
		newPosition = offset;
	} else if (method == FS_SEEK_END) {
		newPosition = fdTable[fd].inode->size + offset;
	} else if (method == FS_SEEK_CUR) {
		newPosition = fdTable[fd].byteOffset + offset;
	} else {
		printf("fileSeek method is not valid\n");
		return 0;
	}

	//Check if the position is negative
	if (newPosition < 0)
		fdTable[fd].byteOffset = 0;
	else
		fdTable[fd].byteOffset = newPosition;

	return 1;
}

/**
 * Checks for the filesystem on this partition, by checking the signatures of the first block for a match.
 * @returns 1 if filesystem exists
 * @returns 0 if filesystem does not exist.
 */
int check_fs() {
	SuperBlock_p buffer = malloc(partInfop->blocksize);
	LBAread(buffer, 1, 0);
	if (buffer->superSignature != SUPER_SIGNATURE || buffer->superSignature2 != SUPER_SIGNATURE2) {
		free(buffer);
		return 0;
	}
	sb = malloc(sizeof(SuperBlock));
	memcpy(sb, buffer, sizeof(SuperBlock));
	readBitVector();
	initWorkingDirectory();
	fdTable = calloc(MAX_OPEN_FILES, sizeof(FileDescriptor));
	free(buffer);
	return 1;
}

/**
 * Formats the current partition and installs a new filesystem.
 * @returns 0 if format was successful
 * @returns -1 if format was unsuccessful
 */
int fs_format() {
	//Write 0s to entire partition and free all globals
	wipePartition();
	freeGlobals();

	SuperBlock_p buffer = calloc(1, partInfop->blocksize);
	buffer->superSignature = SUPER_SIGNATURE;
	//For every BLOCKS_PER_INODE there is one Inode. Set to a prime number to make the hash more efficient
	buffer->numInodes = findNextPrime(partInfop->numberOfBlocks / BLOCKS_PER_INODE);
	buffer->inodeStart = 1;	//Inode blocks start right after superblock

	//Free Blocks starts at one block after blocks used by inodes and block used by superblock
	buffer->bitVectorStart = ((buffer->numInodes * sizeof(Inode) + partInfop->blocksize - 1) / partInfop->blocksize) + buffer->inodeStart;
	//Total Free Blocks = Total number of blocks - total blocks used by inodes - block used by superblock - blocks used by freeblocks
	uint64_t unusedDataBlocks = partInfop->numberOfBlocks - (buffer->bitVectorStart - 1);
	buffer->bytesUsedByBitVector = (unusedDataBlocks + 8 - 1) / 8;
	buffer->blocksUsedByBitVector = (buffer->bytesUsedByBitVector + partInfop->blocksize - 1) / partInfop->blocksize;
	buffer->freeDataBlocks = unusedDataBlocks - buffer->blocksUsedByBitVector;
	buffer->totalDataBlocks = buffer->freeDataBlocks;
	buffer->blocksUsedByInodes = buffer->bitVectorStart - buffer->inodeStart;
	buffer->pointersPerIndirect = partInfop->blocksize / sizeof(uint64_t);
	buffer->maxBlocksPerFile = NUM_DIRECT;
	for (uint32_t i = 0; i < NUM_INDIRECT; i++) {
		uint64_t pointers = buffer->pointersPerIndirect;
		for (uint32_t j = 0; j < i; j++)
			pointers *= buffer->pointersPerIndirect;
		buffer->maxBlocksPerFile += pointers;
	}

	buffer->maxFileSize = buffer->maxBlocksPerFile * partInfop->blocksize;
	buffer->rootDataPointer = buffer->bitVectorStart + buffer->blocksUsedByBitVector;
	buffer->superSignature2 = SUPER_SIGNATURE2;

	if (LBAwrite(buffer, 1, 0) == 0) {
		free(buffer);
		return -1;
	}
	sb = malloc(sizeof(SuperBlock));
	memcpy(sb, buffer, sizeof(SuperBlock));
	free(buffer);

	//Initialize root and working directory
	Inode_p root = calloc(1, sizeof(Inode));
	root->used = USED_FLAG;
	root->type = DIRECTORY_TYPE;
	root->inode = 0;
	root->dateModified = time(NULL);
	root->parentInodeID = 0;
	root->directData[0] = 0;
	root->size = 0;
	root->blocksReserved = 1;
	root->blocksIndirect = 0;
	readBitVector();
	setBitOn(0);
	writeInode(root);
	initWorkingDirectory();
	fdTable = calloc(MAX_OPEN_FILES, sizeof(FileDescriptor));
	sb->usedInodes++;
	sb->freeDataBlocks--;
	saveMemory();
	free(root);
	return 0;
}

/** Outputs data about the current filesystem */
void fs_lsfs() {
	printf("Volume name:        %s\n", partInfop->volumeName);
	printf("Volume size:        %lu\n", partInfop->volumesize);
	printf("Block size:         %lu\n", partInfop->blocksize);
	printf("Blocks:             %lu\n", partInfop->numberOfBlocks);
	printf("Inodes:             %lu\n", sb->numInodes);
	printf("Free Inodes:        %lu\n", (sb->numInodes - sb->usedInodes));
	printf("Used Inodes:        %lu\n", sb->usedInodes);
	printf("Inode index:        %lu\n", sb->inodeStart);
	printf("Inode Blocks:       %lu\n", sb->blocksUsedByInodes);
	printf("BitVector index:    %lu\n", sb->bitVectorStart);
	printf("BitVector Blocks:   %lu\n", sb->blocksUsedByBitVector);
	printf("Total Data Blocks:  %lu\n", sb->totalDataBlocks);
	printf("Free Data Blocks:   %lu\n", sb->freeDataBlocks);
	printf("Used Data Blocks:   %lu\n", (sb->totalDataBlocks - sb->freeDataBlocks));
	printf("Root Data index:    %lu\n", sb->rootDataPointer);
	printf("FS Max File Size:   %lu\n", sb->maxFileSize);
	printf("FS Max Blocks/File: %lu\n", sb->maxBlocksPerFile);
}

/**
 * Adds the entire size and reserved size of all files and folders
 * within a folder.
 * @param totalDirSize where to store the total size
 * @param totalDirReserved where to store the total reserved
 */
void calculateDirSize(Inode_p inode, uint64_t* totalDirSize, uint64_t* totalDirReserved) {
	Inode_p currentInode = NULL;
	FCB_p currentDirectoryData = NULL;
	readFile((uint8_t**)(&currentDirectoryData), inode, 0, 0);
	uint64_t numberOfFiles = inode->size / sizeof(FCB);
	for (uint64_t i = 0; i < numberOfFiles; i++) {
		readInode(currentDirectoryData[i].inodeID, &currentInode);
		*totalDirSize += currentInode->size;
		*totalDirReserved += currentInode->blocksReserved;
		if (currentInode->type == DIRECTORY_TYPE) {
			calculateDirSize(currentInode, totalDirSize, totalDirReserved);
		}
	}
	if (currentInode != NULL)
		free(currentInode);
	free(currentDirectoryData);
}

/** Lists the files in the current directory */
void fs_ls() {
	uint64_t totalDirSize;
	uint64_t totalDirReserved;
	FCB_p currentDirectory = NULL;
	Inode_p wdInode = NULL;
	readInode(wd->inodeID, &wdInode);
	readFile((uint8_t**)(&currentDirectory), wdInode, 0, 0);
	Inode_p currentInode = calloc(1, sizeof(Inode));
	uint64_t numberOfFiles = wdInode->size / sizeof(FCB);
	struct tm* timeInfo;
	char date[20];
		printf("| Type | File Size | Reserved | Last Modified | File Name\n");
	for (uint64_t i = 0; i < numberOfFiles; i++) {
		readInode(currentDirectory[i].inodeID, &currentInode);
		timeInfo = localtime(&(currentInode->dateModified));
		strftime(date, 13, "%b%e %R", timeInfo);
		if (currentInode->type == DIRECTORY_TYPE) {
			totalDirSize = currentInode->size;
			totalDirReserved = currentInode->blocksReserved;
			calculateDirSize(currentInode, &totalDirSize, &totalDirReserved);
			printf("   %d     %10lu %10lu   %s    %s", currentInode->type, totalDirSize, (totalDirReserved * partInfop->blocksize), date, currentDirectory[i].name);
		}
		else
			printf("   %d     %10lu %10lu   %s    %s", currentInode->type, currentInode->size, (currentInode->blocksReserved * partInfop->blocksize), date, currentDirectory[i].name);

		if (currentInode->type == DIRECTORY_TYPE)
			printf("/\n");
		else
			printf("\n");
	}

	free(wdInode);
	free(currentInode);
	free(currentDirectory);
}

/**
 * Changes directory to the given path.
 * "cd /" or "cd" to go to root
 * @param path the path to parse and change directory to
 * @returns 0 if successful
 * @returns -1 if unsuccessful
 */
int fs_cd(char* path) {
	uint64_t pathInode;
	Inode_p inode = NULL;
	if (getLastInode(path, &pathInode, false, NULL) == 0)
		return -2;

	readInode(pathInode, &inode);
	if (inode->type != DIRECTORY_TYPE) {
		free(inode);
		return -2;
	}

	wd->inodeID = pathInode;
	getFullPath(inode->parentInodeID, pathInode);
	wd->pathLength = strlen(wd->wdPath);
	if (inode != NULL)
		free(inode);
	return 0;
}

/**
 * Creates a directory of the given name.
 * @param directoryName the name of the directory to create
 * @returns 0 if sucessful
 * @returns -1 if it could not create directory
 */
int fs_mkdir(char* directoryName) {
	if (createFile(directoryName, DIRECTORY_TYPE, 0, false, 0)) {
		return 0;
	}
	return -1;
}

/**
 * Creates a file of the given name and size
 * @param fileName the name of the file to create
 * @param size the size in bytes to determine the number of blocks to reserve
 * @returns 0 if sucessful
 * @returns -1 if it could not create file
 */
int fs_mkfile(char* fileName, uint64_t size) {
	if (createFile(fileName, FILE_TYPE, size, false, 0)) {
		return 0;
	}
	return -1;
}

/**
 * Deletes the directory with the given name.
 * @param directoryName the name of the directory to delete
 * @returns 1 if successful
 * @returns 0 if it could not delete directory
 * @returns -1 if not a directory
 */
int fs_rmdir(char* directoryName) {
	uint64_t directoryID;
	Inode_p dirInode = NULL;
	char answer;

	//Get the last inode id of the directory
	if (!getLastInode(directoryName, &directoryID, false, NULL)) {
		printf("Could not find directory name\n");
		return 0;
	}

	//Make sure the inode is a directory type
	readInode(directoryID, &dirInode);
	if (dirInode->type != DIRECTORY_TYPE) {
		printf("File is not a directory\n");
		free(dirInode);
		return -1;
	}

	//Double check that the user wants to delete all files and folders in the directory
	printf("This will delete all files located within the directory.\n");
	printf("Do you want to continue? (y/n): ");
	scanf(" %c", &answer);
	flushInput();
	if (answer == 'y' || answer == 'Y') {
		deleteFile(dirInode);
		readInode(dirInode->parentInodeID, &dirInode);

		if (!removeFromDirectory(dirInode, directoryID)) {
			printf("Error: Didn't remove from directory!\n");
			free(dirInode);
			return 0;
		}
	}

	free(dirInode);
	return 1;
}

/**
 * Copies the directory and all files within that directory to the new location
 * @param srcDirInode the directory inode to copy
 * @param destDirInode the directory inode to copy to
 * @param fileName the name of the new directory
 * @returns the directory's new inode id
 * @returns -1 if could not copy directory
 */
int64_t copyDirectory(Inode_p srcDirInode, Inode_p destDirInode, char* fileName) {
	int64_t newInodeID;
	Inode_p currentInode = NULL;
	FCB_p currentDirectoryData = NULL;
	Inode_p currentDestDirInode = NULL;

	newInodeID = createFile(fileName, DIRECTORY_TYPE, srcDirInode->size, true, destDirInode->inode);
	if (newInodeID == -1)
		return -1;

	readInode(newInodeID, &currentDestDirInode);
	readFile((uint8_t**)(&currentDirectoryData), srcDirInode, 0, 0);
	uint64_t numberOfFiles = srcDirInode->size / sizeof(FCB);
	for (uint64_t i = 0; i < numberOfFiles; i++) {
		readInode(currentDirectoryData[i].inodeID, &currentInode);
		if (currentInode->type == DIRECTORY_TYPE) {
			if(copyDirectory(currentInode, currentDestDirInode, currentDirectoryData[i].name) == -1)
				return -1;
		} else {
			if (copyFile(currentInode, currentDestDirInode, currentDirectoryData[i].name) == -1)
				return -1;
		}
	}

	if (currentInode != NULL)
		free(currentInode);
	free(currentDirectoryData);
	return newInodeID;
}

/**
 * Copies the file from the source inode to the directory inode
 * @param srcInode the file inode to copy
 * @param destDirInode the directory inode to copy to
 * @param fileName the name of the new file
 * @returns the file's new inode ID
 * @returns -1 if could not copy file
 */
int64_t copyFile(Inode_p srcInode, Inode_p destDirInode, char* fileName) {
	Inode_p destInode = NULL;
	uint8_t* fileBuffer = NULL;
	int64_t newInodeID;

	//Create file in the directory
	newInodeID = createFile(fileName, srcInode->type, (srcInode->blocksReserved * partInfop->blocksize), true, destDirInode->inode);

	if (newInodeID == 0) {
		return -1;
	}

	//Copy the file contents over
	readInode(newInodeID, &destInode);
	fileBuffer = malloc(partInfop->blocksize);
	int srcFD = inodeOpen(srcInode);
	int destFD = inodeOpen(destInode);

	uint64_t bytesToTransfer;

	do {
		bytesToTransfer = fileRead(srcFD, fileBuffer, partInfop->blocksize);
		if (bytesToTransfer == -1)
			break;
		if (fileWrite(destFD, fileBuffer, bytesToTransfer) == -1)
			break;
	} while (bytesToTransfer == partInfop->blocksize);

	fdTable[destFD].inode->dateModified = time(NULL);
	writeInode(fdTable[destFD].inode);

	fileClose(srcFD);
	fileClose(destFD);
	saveMemory();
	free(destInode);
	free(fileBuffer);
	return newInodeID;
}

/**
 * Copies the file from source to destination
 * @param sourceFile the source file to copy from
 * @param destFile the destination file to copy to
 * @returns 0 if successful
 * @returns -1 if could not copy file
 * @returns -2 if source file does not exist
 */
int fs_cp(char* sourceFile, char* destFile) {
	int retval;
	uint64_t foundInodeID;
	uint64_t srcDirInodeID;
	uint64_t srcInodeID;
	char srcFileName[MAX_NAME_SIZE];
	uint64_t destDirInodeID;
	char destFileName[MAX_NAME_SIZE];
	Inode_p srcDirInode = NULL;
	Inode_p srcInode = NULL;
	Inode_p destDirInode = NULL;
	Inode_p destInode = NULL;
	bool useSrcFileName = false;

	if(strcmp(sourceFile, destFile) == 0)
		return -1;

	//Get the filename and source directory inode id
	if (!getLastInode(sourceFile, &srcDirInodeID, true, srcFileName))
		return -2;

	//read source directory inode
	readInode(srcDirInodeID, &srcDirInode);

	//Check if source directory contains the filename
	if (!inodeContainsFile(srcDirInode, srcFileName, &srcInodeID))
		return -2;

	//read the source inode
	readInode(srcInodeID, &srcInode);

	//Get the filename and destination directory inode id
	if (!getLastInode(destFile, &destDirInodeID, true, destFileName))
		return -1;

	//read destination directory inode
	readInode(destDirInodeID, &destDirInode);

	//Check if file exists. If it is a directory, add file to directory.
	//If it is a file, delete file, and copy inode to that file.
	if (inodeContainsFile(destDirInode, destFileName, &foundInodeID)) {
		readInode(foundInodeID, &destInode);

		if (destInode->type == DIRECTORY_TYPE) {
			useSrcFileName = true;
			//Set the destination directory to the directory
			readInode(foundInodeID, &destDirInode);
			//If a directory, check if filename exists inside directory
			if (inodeContainsFile(destDirInode, srcFileName, &foundInodeID)) {
				Inode_p fileToDelete = NULL;
				readInode(foundInodeID, &fileToDelete);
				deleteFile(fileToDelete);
				removeFromDirectory(destDirInode, fileToDelete->inode);
				free(fileToDelete);
			}
		//If only a file then delete
		} else {
			deleteFile(destInode);
			removeFromDirectory(destDirInode, destInode->inode);
		}
	}

	if (srcInode->type == FILE_TYPE) {
		//Create file in the directory
		if (useSrcFileName) {
			retval = copyFile(srcInode, destDirInode, srcFileName);
		} else {
			retval = copyFile(srcInode, destDirInode, destFileName);
		}
	} else {
		if (useSrcFileName) {
			retval = copyDirectory(srcInode, destDirInode, srcFileName);
		} else {
			retval = copyDirectory(srcInode, destDirInode, destFileName);
		}
	}

	free(srcDirInode);
	free(srcInode);
	free(destDirInode);
	if (destInode != NULL)
		free(destInode);

	return retval;
}

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
int fs_mv(char* sourceFile, char* destFile) {
	uint64_t foundInodeID;
	uint64_t srcDirInodeID;
	uint64_t srcInodeID;
	char srcFileName[MAX_NAME_SIZE];
	uint64_t destDirInodeID;
	char destFileName[MAX_NAME_SIZE];
	Inode_p srcDirInode = NULL;
	Inode_p srcInode = NULL;
	Inode_p destDirInode = NULL;
	Inode_p destInode = NULL;
	bool useSrcFileName = false;

	if(strcmp(sourceFile, destFile) == 0)
		return -1;

	//Get the filename and source directory inode id
	if (!getLastInode(sourceFile, &srcDirInodeID, true, srcFileName))
		return -2;

	//read source directory inode
	readInode(srcDirInodeID, &srcDirInode);

	//Check if source directory contains the filename
	if (!inodeContainsFile(srcDirInode, srcFileName, &srcInodeID))
		return -2;

	//read the source inode
	readInode(srcInodeID, &srcInode);

	//Get the filename and destination directory inode id
	if (!getLastInode(destFile, &destDirInodeID, true, destFileName))
		return -1;

	//read destination directory inode
	readInode(destDirInodeID, &destDirInode);

	//Check if file exists. If it is a directory, add file to directory.
	//If it is a file, delete file, and copy inode to that file.
	if (inodeContainsFile(destDirInode, destFileName, &foundInodeID)) {
		readInode(foundInodeID, &destInode);
		if (destInode->type == DIRECTORY_TYPE) {
			useSrcFileName = true;
			//Set the destination directory to the directory
			readInode(foundInodeID, &destDirInode);
			//If a directory, check if filename exists inside directory
			if (inodeContainsFile(destDirInode, srcFileName, &foundInodeID)) {
				Inode_p fileToDelete = NULL;
				readInode(foundInodeID, &fileToDelete);
				deleteFile(fileToDelete);
				removeFromDirectory(destDirInode, fileToDelete->inode);
				free(fileToDelete);
			}
		//If only a file then delete
		} else {
			deleteFile(destInode);
			removeFromDirectory(destDirInode, destInode->inode);
		}
	}

	//Create new file FCB
	FCB_p newFCB = calloc(1, sizeof(FCB));
	newFCB->inodeID = srcInode->inode;
	if (useSrcFileName) {
		strcpy(newFCB->name, srcFileName);
	}
	else {
		strcpy(newFCB->name, destFileName);
	}

	addToDirectory(destDirInode, newFCB);
	srcInode->parentInodeID = destDirInode->inode;
	srcInode->dateModified = time(NULL);

	//Remove from old directory
	readInode(srcDirInodeID, &srcDirInode);
	removeFromDirectory(srcDirInode, srcInode->inode);
	writeInode(srcInode);
	saveMemory();

	//Free memory
	free(newFCB);
	free(srcDirInode);
	free(srcInode);
	free(destDirInode);
	if (destInode != NULL)
		free(destInode);
	return 0;
}

/**
 * Deletes the file with the given name. Frees up any allocated blocks
 * and removes the file from the directory.
 * @param filename the name of the file to remove
 * @returns 0 if successful
 * @returns -1 if could not delete file
 * @returns -2 if file does not exist
 */
int fs_rm(char* filename) {
	uint64_t fileID;
	Inode_p fileInode = NULL;

	if (!getLastInode(filename, &fileID, false, NULL))
		return -2;

	readInode(fileID, &fileInode);
	if (fileInode->type != FILE_TYPE) {
		printf("Error: Not a file, use rmdir\n");
		free(fileInode);
		return -1;
	}

	deleteFile(fileInode);
	readInode(fileInode->parentInodeID, &fileInode);

	if (!removeFromDirectory(fileInode, fileID)) {
		free(fileInode);
		return -1;
	}

	free(fileInode);
	return 0;
}

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
int fs_cpin(char* sourceFile, char* destFile) {
	int srcFD;
	int destFD;
	struct stat statBuffer;
	size_t fileLength;
	uint64_t foundInodeID;
	uint64_t destDirInodeID;
	char destFileName[MAX_NAME_SIZE];
	Inode_p destDirInode = NULL;
	Inode_p destInode = NULL;
	bool useSrcFileName = false;
	uint64_t newInodeID = 0;

	//Open source file and get the length of file
	srcFD = open(sourceFile, O_RDONLY);
	if (srcFD == -1)
		return -2;
	fstat(srcFD, &statBuffer);
    fileLength = statBuffer.st_size;

	//Parse source string to get source filename
	uint32_t strlength = strlen(sourceFile) + 1;
	char* strCopy = malloc(strlength);
	memcpy(strCopy, sourceFile, strlength);
    char* token = strtok(strCopy, "/");
    char* srcFileName;
    while (token != NULL) {
        srcFileName = token;
        token = strtok(NULL, "/");
    }

	//Get the filename and destination directory inode id
	if (!getLastInode(destFile, &destDirInodeID, true, destFileName))
		return -1;

	//read destination directory inode
	readInode(destDirInodeID, &destDirInode);

	//Check if file exists. If it is a directory, add file to directory.
	//If it is a file, delete file, and copy inode to that file.
	if (inodeContainsFile(destDirInode, destFileName, &foundInodeID)) {
		readInode(foundInodeID, &destInode);

		if (destInode->type == DIRECTORY_TYPE) {
			useSrcFileName = true;
			//Set the destination directory to the directory
			readInode(foundInodeID, &destDirInode);
			//If a directory, check if filename exists inside directory
			if (inodeContainsFile(destDirInode, srcFileName, &foundInodeID)) {
				Inode_p fileToDelete = NULL;
				readInode(foundInodeID, &fileToDelete);
				deleteFile(fileToDelete);
				removeFromDirectory(destDirInode, fileToDelete->inode);
				free(fileToDelete);
			}
		//If only a file then delete
		} else {
			deleteFile(destInode);
			removeFromDirectory(destDirInode, destInode->inode);
		}
	}

	//Create file in the directory
	if (useSrcFileName) {
		newInodeID = createFile(srcFileName, FILE_TYPE, fileLength, true, destDirInode->inode);
	} else {
		newInodeID = createFile(destFileName, FILE_TYPE, fileLength, true, destDirInode->inode);
	}
	if (newInodeID == 0) {
		free(strCopy);
		free(destDirInode);
		free(destInode);
		return -1;
	}

	//Copy the file contents over
	readInode(newInodeID, &destInode);
	uint8_t* fileBuffer = malloc(partInfop->blocksize);
	destFD = inodeOpen(destInode);

	uint64_t bytesToTransfer;

	do {
		bytesToTransfer = read(srcFD, fileBuffer, partInfop->blocksize);
		if (fileWrite(destFD, fileBuffer, bytesToTransfer) == -1)
			break;
	} while (bytesToTransfer == partInfop->blocksize);

	fdTable[destFD].inode->dateModified = time(NULL);
	writeInode(fdTable[destFD].inode);

	close(srcFD);
	fileClose(destFD);
	saveMemory();

	free(strCopy);
	free(destDirInode);
	free(destInode);
	free(fileBuffer);
	return 0;
}

/**
 * Copies a file from this filesystem to another filesystem
 * @param sourceFile the source file in this file system to copy from
 * @param destFile the destination file in another file system to copy the file to
 * @returns 0 if successful
 * @returns -1 if unsuccessful
 * @returns -2 if could not open source file
 * @returns -3 if could not open destination file
 */
int fs_cpout(char* sourceFile, char* destFile) {
	int srcFD;
	int destFD;

	srcFD = fileOpen(sourceFile);
	if (srcFD == -1)
		return -2;

    destFD = open(destFile, O_WRONLY | O_CREAT);
    if (destFD == -1)
        return -3;

    uint8_t* fileBuffer = malloc(partInfop->blocksize);
	uint32_t bytesToTransfer;

	do {
		bytesToTransfer = fileRead(srcFD, fileBuffer, partInfop->blocksize);
		if (bytesToTransfer == -1) {
			printf("reading file error\n");
			break;
		}
		if (write(destFD, fileBuffer, bytesToTransfer) == -1) {
			printf("writing file error\n");
			break;
		}
	} while (bytesToTransfer == partInfop->blocksize);

    fileClose(srcFD);
    close(destFD);
    free(fileBuffer);
	return 0;
}

/**
 * Prints the full working directory
 */
void fs_pwd() {
	printf("%s\n", wd->wdPath);
}

/**
 * Resizes the file. Can not resize directories. Maximum is capped by the number of available
 * blocks to hold the requested size. When increasing the size, if the size is greater than
 * the currently allocated blocks, new blocks will be wiped and allocated to the file.
 * Resize does not deallocate blocks, reserve command must be used.
 * @param file the file to resize
 * @param size the size in bytes to resize to
 * @returns 0 if successful
 * @returns -1 if could not resize file
 * @returns -2 if file could not be found
 */
int fs_resize(char* file, uint64_t size) {
	uint64_t fileID;
	Inode_p fileInode = NULL;

	if (!getLastInode(file, &fileID, false, NULL))
		return -2;

	readInode(fileID, &fileInode);
	if (fileInode->type != FILE_TYPE) {
		free(fileInode);
		return -1;
	}

	//Check if the size is larger than the current file size. Allocate blocks if needed.
	if ((size > fileInode->size) && allocateBlocks(fileInode, size) == -1) {
		free(fileInode);
		return -1;
	}

	fileInode->size = size;
	fileInode->dateModified = time(NULL);
	writeInode(fileInode);
	saveMemory();
	free(fileInode);
	return 0;
}

/**
 * Resizes the reserved blocks to hold the number of bytes requested. Minimum reserved is one block
 * or the minium blocks needed to hold the current size of the file. Maximum blocks are limited by
 * the number of available free blocks to be allocated.
 * @param file the file to change the reserved block size
 * @param size the size in bytes to calculate the number of blocks to reserve
 * @returns 0 if successful
 * @returns -1 if could not resize file
 * @returns -2 if file could not be found
 */
int fs_reserve(char* file, uint64_t size) {
	uint64_t fileID;
	Inode_p fileInode = NULL;

	if (!getLastInode(file, &fileID, false, NULL))
		return -2;

	readInode(fileID, &fileInode);
	uint64_t neededBlocks = (size + partInfop->blocksize - 1) / partInfop->blocksize;
	if (size < 1)
		size = 1;


	if (neededBlocks > fileInode->blocksReserved) {
		if (allocateBlocks(fileInode, size) == -1) {
			free(fileInode);
			return -1;
		}
	} else if (neededBlocks < fileInode->blocksReserved) {
		deallocateBlocks(fileInode, size);
	}

	fileInode->dateModified = time(NULL);
	writeInode(fileInode);
	saveMemory();
	free(fileInode);
	return 0;
}
