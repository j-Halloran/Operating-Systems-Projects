/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

//size of a disk block
#define	BLOCK_SIZE 512

//size of disk
#define DISK_SIZE 5242880

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//declaration for concat
char* concat(const char *, const char *);

//Struct to hold which blocks are used
struct cs1550_in_use {
  char in_use[DISK_SIZE/BLOCK_SIZE]; //stores on disk the allocated blocks;
  //gotta add padding if block is different size (right now it works out perfectly)
};
typedef struct cs1550_in_use cs1550_in_use;

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct cs1550_disk_block
{
	//The next disk block, if needed. This is the next pointer in the linked
	//allocation list
	long nNextBlock;

	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//Finds what level a file is (root/subdirectory/lower)
static int level_find(const char *path){
  int level = 0;
  int len = strlen(path);
  int i = 0;
  for(i=0;i<len;i++){
    if(path[i]=='/'){
      level++;
    }
  }
  return level;
}

//blatantly stolen from http://www.psacake.com/web/func/rmchr_function.htm
static void rmchr(char *str, char target)
{
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++)
    {
        *dst = *src;
        if (*dst != target) dst++;
    }
    *dst = '\0';
}

//checks if a directory is valid
static int is_directory(const char *path){
  int i =0;
  int found = 0;

  FILE *disk_image = fopen(".disk","r+");
  char buffer[sizeof(cs1550_root_directory)];
  memset(buffer, 0,sizeof(cs1550_root_directory));

  fseek(disk_image,(DISK_SIZE/BLOCK_SIZE)*BLOCK_SIZE,SEEK_CUR); //seek the file to the start of the root node
  fread(buffer,sizeof(cs1550_root_directory),1,disk_image);
  struct cs1550_root_directory *root = (cs1550_root_directory*)buffer;
  for(i =0; i<MAX_DIRS_IN_ROOT;i++){
    if(!strcmp(root->directories[i].dname,path)){
      found = 1;
      break;
    }
  }

  fclose(disk_image);
  return found;
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

//checks if a file is valid
static int is_file(const char * path, size_t * filesize){
  int found = 0;
  int i,j;
  if(level_find(path)<2){
    return found; //exit if not within a subdirectory
  }
  char * directory = malloc(MAX_FILENAME+2); //name plus / plus null terminator
  char * file = malloc(MAX_FILENAME+MAX_EXTENSION+2);
  directory = strtok(path,"/");
  file = strtok(NULL,"/");

  //get the directory if possiblec
  FILE *disk_image = fopen(".disk","r+");
  char buffer[sizeof(cs1550_root_directory)];
  memset(buffer, 0,sizeof(cs1550_root_directory));
  fseek(disk_image,(DISK_SIZE/BLOCK_SIZE)*BLOCK_SIZE,SEEK_CUR); //seek the file to the start of the root node
  fread(buffer,sizeof(cs1550_root_directory),1,disk_image);
  struct cs1550_root_directory *root = (cs1550_root_directory*)buffer;
  for(i =0; i<root->nDirectories;i++){
    if(!strcmp(root->directories[i].dname,directory)){

      //if we found a matching directory look for the file
      fseek(disk_image,BLOCK_SIZE*root->directories[i].nStartBlock,SEEK_SET);
      fread(disk_image,sizeof(cs1550_directory_entry),1,disk_image);
      struct cs1550_directory_entry *sub = (cs1550_directory_entry*)buffer;
      for(j=0;sub->nFiles;j++){
        if(!strcmp(sub->files[j].fname,strtok(file,"."))&&!strcmp(sub->files[j].fext,strtok(NULL,"."))){
          //if we found the file mark that and get its size
          found = 1;
          *filesize = sub->files[j].fsize;
          break;
        }
      }
      break; //break if matching directory
    }
  }

  free(directory);
  free(file);
  fclose(disk_image);
  return found;
}
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
    //Check if name is subdirectory
    if(is_directory(path)){
      printf("%s is a valid directory/n",path);
      //Might want to return a structure with these fields
  		stbuf->st_mode = S_IFDIR | 0755;
  		stbuf->st_nlink = 2;
  		res = 0; //no error
    }
    //Check if name is a regular file
    size_t filesize = 0;
    if(is_file(path,&filesize)){
      printf("%s is a valid file/n",path);
      //regular file, probably want to be read and write
      stbuf->st_mode = S_IFREG | 0666;
      stbuf->st_nlink = 1;
      stbuf->st_size = filesize;
      res = 0;
    }


	//Check if name is a regular file
	/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
	*/

		//Else return that path doesn't exist
		res = -ENOENT;
	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//check if directory
	if(!is_directory(path)&&path!=NULL){
    printf("Error %s not found\n");
    return -ENOENT;
  }

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
  FILE *disk_image = fopen(".disk","r+");
  char buffer[sizeof(cs1550_root_directory)];
  memset(buffer, 0,sizeof(cs1550_root_directory));

  if(!strcmp(path,"/")){
    fseek(disk_image,(DISK_SIZE/BLOCK_SIZE)*BLOCK_SIZE,SEEK_CUR);
    fread(buffer,sizeof(cs1550_root_directory),1,disk_image);
  }

  fclose(disk_image);
	return 0;
}

//finds the first open block to assign to the new directory
static long getFreeBlock(){
  FILE *disk_image = fopen(".disk","r+");
  char buffer[sizeof(cs1550_in_use)];
  memset(buffer, 0,sizeof(cs1550_in_use));
  fread(buffer,sizeof(cs1550_in_use),1,disk_image);
  struct cs1550_in_use *used = (cs1550_in_use *)buffer;

}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

  if(strlen(path)-1 > MAX_FILENAME){ //check if too big giving 1 extra for the root /
    return -ENAMETOOLONG;
  }
  if(level_find(path)>1){
    printf("Error: Attempting to create directory outside of the root directory\n");
    return -EPERM;
  }
  if(is_directory(path)){
    printf("Error directory already exists\n");
    return -EEXIST;
  }

  FILE *disk_image = fopen(".disk","r+");
  if(disk_image==NULL){
    printf("Error file not opened");
  }
  int temp = fgetc(disk_image);

  //if temp == 0 then this is the first time anytinh has been added meaning we need to initialize tracking
  if(!temp){
    struct cs1550_in_use in_use;
    memset(&in_use,0,sizeof(cs1550_in_use));
    int i;

    //mark the first serveral blocks as the tracking and the one after taht as the root
    for(i=0;i<(DISK_SIZE/BLOCK_SIZE)+1;i++){
      in_use.in_use[i] = 1;
    }

    //create the root
    struct cs1550_root_directory root;
    memset(&root,0,sizeof(cs1550_root_directory));
    root.nDirectories = 0;
    fseek(disk_image,(DISK_SIZE/BLOCK_SIZE)*BLOCK_SIZE,SEEK_CUR);
    fwrite(&root,sizeof(cs1550_root_directory),1,disk_image);
  }

  //find the root
  char buffer[sizeof(cs1550_root_directory)];
  memset(buffer, 0,sizeof(cs1550_root_directory));

  fseek(disk_image,(DISK_SIZE/BLOCK_SIZE)*BLOCK_SIZE,SEEK_CUR); //seek the file to the start of the root node
  fread(buffer,sizeof(cs1550_root_directory),1,disk_image);
  struct cs1550_root_directory *root = (cs1550_root_directory*)buffer;

  int next_location = root->nDirectories;
  if(nDirectories==MAX_DIRS_IN_ROOT){
    printf("Error cannot create new directory, root full\n");
    return -EEXIST;
  }
  if(path[0]=="/"){
    rmchr(path,"/");
  }
  root->directories[next_location].dname = path;
  root->directories[next_location].nStartBlock = getFreeBlock();

  struct cs1550_directory_entry new_directory;
  memset(&new_directory,0,sizeof(cs1550_directory_entry));

  new_directory.nFiles = 0;
  fclose(disk_image);
	return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
