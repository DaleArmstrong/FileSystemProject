## FileSystemProject

A Simple File System project to learn about how a filesystem works and the various trade-offs of different implementations.

***************************************************************************  

### Driver Instructions

To compile, use the included makefile, by having make installed and typing make. It will create the myfs executable file. I have included a 9MB 512 block size test file volume named “testfile” that is already partitioned and formatted.

* To run the program utilizing the testfile just type “./myfs”
* To create a new file type ./myfs \<filename\> \<volumesize\> \<blocksize\>
	* Once created, the program will present the option to format the volume.
	* The file system will automatically calculate the required inodes, bit vector size, and data blocks. The minimum volume size is currently set to 20 blocks, which the file system will automatically set the volume size to if the requested number is below 20.

***************************************************************************  
### Driver Commands

* **help** [command]
	* help by itself will display all commands and what they do.
	* help \<command\> will display usage information about a particular command
* **format** - Formats the partition and installs the filesystem. Will delete any current filesystems that are installed.
* **ls** - Lists the files in the current directory and their accompanying information. If the file is a directory, the size and blocks reserved are a sum of the directory size and reserved plus the sum of all files and directories residing inside that directory.
* **cd** \<directoryname\> - Lists files in the current directory.
	* cd or cd / will go straight to root
	* cd .. will move up one directory
	* It is possible to traverse several directories in one command eg “cd ../home/etc/../etc”
* **pwd** - Prints the full working directory
* **cp** \<source\> \<destination\> - Copies the file from the source to the destination
* **mv** \<source\> \<destination\> - Moves the file from the source to the destination. You may also use this function to change a filename.
* **rm** \<filename\> - Deletes the file. This is only for file types, directories require rmdir.
* **rmdir** \<directoryname\> - Deletes the directory and all files and folders in the directory, freeing up used blocks.
* **mkdir** \<directoryname\> - Creates the given directory
* **mkfile** \<filename\> [size] - Creates an empty file of the given filename, with an optional reserve size in bytes.
* **lsfs** - Displays various information about the filesystem. Free blocks, used blocks, block size, volume size, which block each part of the filesystem starts at, the maximum file size this filesystem could potentially support at this block size, and the maximum block size that this filesystem could potentially support based on the number of indirect blocks and direct blocks.
* **resize** \<filename\> \<size\> - Resizes the file. If the number of bytes exceeds the reserved block size, then new empty blocks will be allocated to the file. If the size is decreased, the reserved blocks will not be reduced. This only works on files and not directories.
* **reserve** \<filename\> \<size\> - Resizes the reserved blocks. The minimum reserved blocks is either one block or the number of blocks required to hold the size of the file. Ie: if size is 0, then blocks reserved will be 1, if size is between 1-2 block sizes, reserved size will be 2.
* **cpin** \<source\> \<destination\> - Copies a file from the linux filesystem into this filesystem
* **cpout** \<source\> \<destination\> - Copies a file from this filesystem to the linux filesystem
* **exit** - exits the file system