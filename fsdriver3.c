/***************************************************************
* Class: CSC-415-03 Spring 2020
* Group Name: Zeta 3
* Name: Dale Armstrong
* StudentID: 920649883
*
* Project: Assignment 3 - File System
* @file: fsdriver3.c
*
* Description: This file is a shell/driver for the file system. The
*	file system implements multiple commands to allow for the storage,
*	manipulation, and reading of files and directories within the
*	filesystem. This project is used to simulate and learn about
*	how file systems actually work, so that we have a better understanding
*	of the underlying layers within the operating system.
*
*   This file system implements these interactive commands:
*	format - formats the volume
*	ls - list directory
*	cd - change directory
*	pwd - print current working directory path
*	cp - copy file
*	mv - move file (also used to rename)
*	rm - remove file
*	mkdir - make directory
*   mkfile - make an empty file of a given size in bytes
*	rmdir - remove directory
*	help - info on all commands
*	lsfs - list filesystem info (mainly used for debugging)
*	cpin - copy from linux filesystem to this filesystem
*	cpout - copy from this filesystem to linux filesystem
*	reserve - resizes the reserved blocks to hold the number of bytes requested.
*	resize - resizes the file.
*	exit - exit the driver/shell
****************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FileSystem.h"

#define MAX_INPUT_BUFFER 512
#define MAX_ARG MAX_INPUT_BUFFER/2+1
#define MIN_BLOCKS 20

uint32_t parseArgs(char*, char**);
void processArgs(int, char**);
void run_help(int, char**);
void run_mkdir(int, char**);
void run_mkfile(int, char**);
void run_ls(int, char**);
void run_lsfs(int, char**);
void run_format(int, char**);
void run_rmdir(int, char**);
void run_reserve(int, char**);
void run_resize(int, char**);
void run_cd(int, char**);
void run_pwd(int, char**);
void run_cp(int, char**);
void run_mv(int, char**);
void run_rm(int, char**);
void run_cpin(int, char**);
void run_cpout(int, char**);
void flushInput();

int main(int argc, char **argv) {
    char userInput[MAX_INPUT_BUFFER];
    char* args[MAX_ARG];
    uint32_t numArgs;
    char* filename;
    uint64_t volumeSize = 0;
    uint64_t blockSize = 0;

    if (argc < 4) {
		if (access("testfile", F_OK) == -1) {
			printf("Missing arguments: Filename, Volume Size, Block Size\n");
			printf("Usage: ./myfs <filename> <volumesize> <blocksize>\n");
			exit(EXIT_FAILURE);
		} else {
			filename = "testfile";
		}
    } else if (argc > 4) {
		printf("Too many arguments\n");
		exit(EXIT_FAILURE);
    } else {
		filename = argv[1];
		volumeSize = atoll(argv[2]);
		blockSize = atoll(argv[3]);
		//Minimum volume size
		if (volumeSize < blockSize * MIN_BLOCKS) {
			volumeSize = blockSize * MIN_BLOCKS;
		}
    }

	int retVal = startPartitionSystem(filename, &volumeSize, &blockSize);
	if (retVal != 0) {
		printf("Error: opening partition %s\n", filename);
		exit(EXIT_FAILURE);
	}

	printf("Opened %s, Volume Size: %llu;  BlockSize: %llu; Return %d\n", filename, (ull_t)volumeSize, (ull_t)blockSize, retVal);

	/* Check if partition is already formatted, if not then format */
	if (!check_fs()) {
		char answer;
		printf("Partition is not formatted.\n");
		printf("You must format the partition to continue.\n");
		printf("Format now? (y/n): ");
		scanf(" %c", &answer);
		flushInput();
		if (answer == 'y' || answer == 'Y') {
			fs_format();
		} else {
			printf("Canceling format.\n");
			printf("Exiting...\n");
			closePartitionSystem();
			exit(EXIT_SUCCESS);
		}
	}

    while (1) {
		if (wd->pathLength > 30)
			printf("...%s>", &wd->wdPath[wd->pathLength-30]);
		else
			printf("%s>", wd->wdPath);

        /* retrieve input */
        if (fgets(userInput, MAX_INPUT_BUFFER, stdin) == NULL)
            /* check for EOF */
            if (feof(stdin))
                exit(EXIT_SUCCESS);

        /* test for empty input */
        if (strlen(userInput) == 1) {
            printf("Error: No command entered.\n");
        } else if (strcmp(userInput, "exit\n") == 0) {
        	/* test for exit command */
        	closePartitionSystem();
            exit(EXIT_SUCCESS);
        } else {
            /* check and flush extra data in input stream */
            if (strchr(userInput, '\n') == NULL)
                flushInput();

            numArgs = parseArgs(userInput, args);
            processArgs(numArgs, args);
        }
    }
    return 0;
}

/* Separate arguments into an array */
uint32_t parseArgs(char* input, char** args) {
    uint32_t i = 0;
    char* token = strtok(input, " -\t\n");
    while (token != NULL) {
        args[i] = token;
        token = strtok(NULL, " -\t\n");
        i++;
    }
    args[i] = NULL;
    return i;
}

/* run command with given arguments */
void processArgs(int numArgs, char** args) {
	if (strcmp(args[0], "format") == 0) {
		run_format(numArgs, args);
	} else if (strcmp(args[0], "help") == 0) {
		run_help(numArgs, args);
	} else if (strcmp(args[0], "lsfs") == 0) {
		run_lsfs(numArgs, args);
	} else if (strcmp(args[0], "ls") == 0) {
		run_ls(numArgs, args);
	} else if (strcmp(args[0], "cd") == 0) {
		run_cd(numArgs, args);
	} else if (strcmp(args[0], "pwd") == 0) {
		run_pwd(numArgs, args);
	} else if (strcmp(args[0], "mkdir") == 0) {
		run_mkdir(numArgs, args);
	} else if (strcmp(args[0], "mkfile") == 0) {
		run_mkfile(numArgs, args);
	} else if (strcmp(args[0], "rmdir") == 0) {
		run_rmdir(numArgs, args);
	} else if (strcmp(args[0], "reserve") == 0) {
		run_reserve(numArgs, args);
	} else if (strcmp(args[0], "resize") == 0) {
		run_resize(numArgs, args);
	} else if (strcmp(args[0], "cp") == 0) {
		run_cp(numArgs, args);
	} else if (strcmp(args[0], "mv") == 0) {
		run_mv(numArgs, args);
	} else if (strcmp(args[0], "rm") == 0) {
		run_rm(numArgs, args);
	} else if (strcmp(args[0], "cpin") == 0) {
		run_cpin(numArgs, args);
	} else if (strcmp(args[0], "cpout") == 0) {
		run_cpout(numArgs, args);
	} else {
		printf("%s: command not found\n", args[0]);
		printf("Type help for more info\n");
	}
}

/* Display help information for each function in filesystem */
void run_help(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Too many arguments\n");
		printf("Type help or help <function> for more information\n");
		return;
	} else if (numArgs == 1) {
		printf("Type help <function> to get more information about a function\n");
		printf("Commands:\n");
		printf("format - formats the partition\n");
		printf("lsfs   - lists the information of the current filesystem\n");
		printf("ls     - lists files in the directory\n");
		printf("cd     - changes current directory\n");
		printf("pwd    - prints the full working directory\n");
		printf("mkdir  - creates a directory\n");
		printf("mkfile - creates a file with size\n");
		printf("rmdir  - removes a directory\n");
		printf("resize - changes the size of the file\n");
		printf("reserve- increases or reduces the reserved blocks\n");
		printf("cp     - copies a file from source to destination\n");
		printf("mv     - moves a file from source to destination\n");
		printf("rm     - deletes a file\n");
		printf("cpin   - copy a file in from another filesystem\n");
		printf("cpout  - copies a file to another filesystem\n");
		printf("exit   - exit shell\n");
	} else {
		if (strcmp(args[1], "format") == 0) {
			printf("Usage: format\n");
			printf("Formats the partition and installs the filesystem.\n");
			printf("Will delete any current filesystems that are installed\n");
		} else if (strcmp(args[1], "lsfs") == 0) {
			printf("Usage: lsfs\n");
			printf("Lists the information about the current filesystem.\n");
			printf("This includes: Volume name, volume ID, block size, number of blocks,\n");
			printf("	free blocks, and space used.\n");
		} else if (strcmp(args[1], "ls") == 0) {
			printf("Usage: ls\n");
			printf("lists files in the current directory.\n");
		} else if (strcmp(args[1], "cd") == 0) {
			printf("Usage: cd <dirname>\n");
			printf("Changes current directory to the given directory\n");
		} else if (strcmp(args[1], "pwd") == 0) {
			printf("Usage: pwd\n");
			printf("Prints the full working directory\n");
		} else if (strcmp(args[1], "mkdir") == 0) {
			printf("Usage: mkdir <dirname>\n");
			printf("Creates a directory with the given name\n");
		} else if (strcmp(args[1], "mkfile") == 0) {
			printf("Usage: mkfile <filename> [size]\n");
			printf("Creates a file with the given name and optional size\n");
		} else if (strcmp(args[1], "rmdir") == 0) {
			printf("Usage: rmdir <dirname>\n");
			printf("Deletes the directory with the given name\n");
		} else if (strcmp(args[1], "resize") == 0) {
			printf("Usage: resize <filename> <size>\n");
			printf("Resizes the file. Maximum is capped by the number of available blocks to hold the\n");
			printf("requested size. When increasing the size, if the size is greater than the currently\n");
			printf("allocated blocks, new blocks will be wiped and allocated to the file.\n");
			printf("Resize does not deallocate blocks, reserve command must be used.\n");
		} else if (strcmp(args[1], "reserve") == 0) {
			printf("Usage: reserve <filename> <size>\n");
			printf("Resizes the reserved blocks to hold the number of bytes requested.\n");
			printf("Minimum reserved is one block or the minium blocks needed to hold the current size of the file.\n");
			printf("Maximum blocks are limited by the number of available free blocks to be allocated.\n");
		} else if (strcmp(args[1], "cp") == 0) {
			printf("Usage: cp <source> <destination>\n");
			printf("Copies the file from the source to the destination\n");
		} else if (strcmp(args[1], "mv") == 0) {
			printf("Usage: mv <source> <destination>\n");
			printf("Moves the file from the source to the destination\n");
		} else if (strcmp(args[1], "rm") == 0) {
			printf("Usage: rm <filename>\n");
			printf("Deletes the given filename\n");
		} else if (strcmp(args[1], "cpin") == 0) {
			printf("Usage: cpin <source> <desintation>\n");
			printf("Copies a file from another filesystem into the destination in this filesystem\n");
		} else if (strcmp(args[1], "cpout") == 0) {
			printf("Usage: cpout <source> <destination>\n");
			printf("Copies a file from the current filesystem to another filesystem\n");
		} else {
			printf("Unknown command.\n");
			printf("Type help or help <function> for more information\n");
		}
	}
}

//Format the volume
void run_format(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: format\n");
		return;
	}

	char answer;
	int retvalue;
	printf("Warning: This will delete the current filesystem!\n");
	printf("Do you want to continue? (y/n): ");
	scanf(" %c", &answer);
	flushInput();
	if (answer == 'y' || answer == 'Y') {
		retvalue = fs_format();
	} else {
		printf("Canceled format\n");
		return;
	}

	if (retvalue == -1) {
		printf("Format failed!\n");
	} else if (retvalue == 0) {
		printf("Format success!\n");
	}
}

//Print information about the filesystem
void run_lsfs(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: lsfs\n");
		return;
	}

	fs_lsfs();
}

//List the current directory
void run_ls(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: ls\n");
		return;
	}

	fs_ls();
}

//Print full working directory path name
void run_pwd(int numArgs, char** args) {
	if (numArgs > 1) {
		printf("Unknown arguments\n");
		printf("Usage: pwd\n");
		return;
	}

	fs_pwd();
}

//Used to change directory
void run_cd(int numArgs, char** args) {
	int retvalue;
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: mkdir <dirname>\n");
		return;
	}

	if (numArgs == 2) {
		retvalue = fs_cd(args[1]);
		if (retvalue == -1)
			printf("Could not change to directory: %s\n", args[1]);
		if (retvalue == -2)
			printf("Not a directory: %s\n", args[1]);
	} else {
		retvalue = fs_cd(NULL);
		if (retvalue == -1)
			printf("Error changing direct to root\n");
	}
}

//Create a directory
void run_mkdir(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: mkdir <dirname>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing directory name\n");
		printf("Usage: mkdir <dirname>\n");
		return;
	}

	int retvalue = fs_mkdir(args[1]);

	if (retvalue == -1) {
		printf("Could not make directory: %s\n", args[1]);
	} else if (retvalue == -2) {
		printf("%s directory already exists\n", args[1]);
	}
}

//Create an empty file
void run_mkfile(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: mkfile <filename> [size]\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing filename\n");
		printf("Usage: mkfile <filename> [size]\n");
		return;
	}

	int retvalue;
	if (numArgs == 3)
		retvalue = fs_mkfile(args[1], atol(args[2]));
	else
		retvalue = fs_mkfile(args[1], 0);

	if (retvalue == -1) {
		printf("Could not make file: %s\n", args[1]);
	} else if (retvalue == -2) {
		printf("%s file already exists\n", args[1]);
	}
}

//Remove a directory
void run_rmdir(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: rmdir <dirname>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing directory name\n");
		printf("Usage: rmdir <dirname>\n");
		return;
	}

	int retvalue = fs_rmdir(args[1]);

	if (retvalue == 0) {
		printf("Could not remove directory: %s\n", args[1]);
	} else if (retvalue == -1) {
		printf("%s is not a directory\n", args[1]);
	}
}

//Resize a file, Can not resize a directory
void run_resize(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: resize <filename> <size>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: resize <filename> <size>\n");
		return;
	}

	uint64_t newSize = atol(args[2]);
	if (newSize < 0) {
		printf("Size must be positive\n");
		return;
	}

	int retvalue = fs_resize(args[1], newSize);

	if (retvalue == -1) {
		printf("Could not resize file %s to %ld\n", args[1], newSize);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

//Deallocate or allocate more blocks for a file
void run_reserve(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: reserve <filename> <size>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: reserve <filename> <size>\n");
		return;
	}

	int64_t newSize = atol(args[2]);
	if (newSize < 0) {
		printf("Size must be positive\n");
		return;
	}

	int retvalue = fs_reserve(args[1], newSize);

	if (retvalue == -1) {
		printf("Could not changed reserved in file %s to %ld\n", args[1], newSize);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

//Copy a file or directory
void run_cp(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cp <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cp <source> <destination>\n");
		return;
	}

	int retvalue = fs_cp(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

//Move a file or directory
void run_mv(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: mv <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: mv <source> <destination>\n");
		return;
	}

	int retvalue = fs_mv(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not move file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

//Remove a file
void run_rm(int numArgs, char** args) {
	if (numArgs > 2) {
		printf("Unknown arguments\n");
		printf("Usage: rm <filename>\n");
		return;
	} else if (numArgs < 2) {
		printf("Missing arguments\n");
		printf("Usage: rm <filename>\n");
		return;
	}

	int retvalue = fs_rm(args[1]);

	if (retvalue == -1) {
		printf("Could not delete file\n");
	} else if (retvalue == -2) {
		printf("%s does not exist\n", args[1]);
	}
}

//Copy a file from another filesystem to this filesystem
void run_cpin(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cpin <source> <desintation>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cpin <source> <desintation>\n");
		return;
	}

	int retvalue = fs_cpin(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("Could not open %s\n", args[1]);
	} else if (retvalue == -3) {
		printf("Could not open %s\n", args[2]);
	}
}

//Copy a file from this filesystem to another filesystem
void run_cpout(int numArgs, char** args) {
	if (numArgs > 3) {
		printf("Unknown arguments\n");
		printf("Usage: cpout <source> <destination>\n");
		return;
	} else if (numArgs < 3) {
		printf("Missing arguments\n");
		printf("Usage: cpout <source> <destination>\n");
		return;
	}

	int retvalue = fs_cpout(args[1], args[2]);

	if (retvalue == -1) {
		printf("Could not copy file %s to %s\n", args[1], args[2]);
	} else if (retvalue == -2) {
		printf("Could not open %s\n", args[1]);
	} else if (retvalue == -3) {
		printf("Could not open %s\n", args[2]);
	}
}

// flushes the input buffer
void flushInput() {
    char c;
    while ((c = getchar()) != '\n' && c != EOF)
        ; // discard characters
}
