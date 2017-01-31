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
  long next_free_node;
  char padding[BLOCK_SIZE-sizeof(long)]; //padding for the block
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
/*static void rmchr(char *str, char target)
{
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++)
    {
        *dst = *src;
        if (*dst != target) dst++;
    }
    *dst = '\0';
}*/

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

//checks if a directory is valid
static int is_directory(const char *path){
  int i =0;
  int found = 0;

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  FILE *disk_image = fopen(".disk","r+");
  struct cs1550_root_directory *root = (cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));

  rewind(disk_image); //seek the file to the start of the root node
  fread(root,sizeof(cs1550_root_directory),1,disk_image);
  for(i =0; i<root->nDirectories;i++){
    if(!strcmp(root->directories[i].dname,directory)){
      found = 1;
      break;
    }
  }

  free(root);
  free(directory);
  free(filename);
  free(extension);
  fclose(disk_image);
  return found;
}


//checks if a file is valid
static int is_file(const char * path, size_t * filesize){
  int found = 0;
  int i,j;
  if(level_find(path)<2){
    return found; //exit if not within a subdirectory
  }

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  FILE *disk_image = fopen(".disk","r+");
  struct cs1550_root_directory *root = (cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));

  rewind(disk_image); //seek the file to the start of the root node
  fread(root,sizeof(cs1550_root_directory),1,disk_image);
  for(i =0; i<root->nDirectories;i++){
    if(!strcmp(root->directories[i].dname,directory)){

      //now that we found a matching directory, loop over its files
      struct cs1550_directory_entry *sub = (cs1550_directory_entry *)calloc(1,sizeof(cs1550_directory_entry));
      fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
      fread(sub,sizeof(cs1550_directory_entry),1,disk_image);
      for(j=0;j<sub->nFiles;j++){
        if(!strcmp(sub->files[j].fname,filename)&&!strcmp(sub->files[j].fext,extension)){
          found = 1;
          *filesize = sub->files[i].fsize;
          break;
        }
      }
      free(sub);
      break; //break as no need to search other unmatching directories
    }
  }

  free(directory);
  free(filename);
  free(extension);
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
  printf("Getting attr for path: %s\n",path);
	memset(stbuf, 0, sizeof(struct stat));

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
    size_t filesize = 0;
    //Check if name is subdirectory
    if(is_directory(path)&&level_find(path)==1){
      printf("%s is a valid directory\n",path);
      //Might want to return a structure with these fields
  		stbuf->st_mode = S_IFDIR | 0755;
  		stbuf->st_nlink = 2;
  		res = 0; //no error
    }
    //Check if name is a regular file

    else if(is_file(path,&filesize)){

      printf("%s is a valid file\n",path);
      //regular file, probably want to be read and write
      stbuf->st_mode = S_IFREG | 0666;
      stbuf->st_nlink = 1;
      stbuf->st_size = filesize;
      res = 0;
    }
    else{
      //Else return that path doesn't exist
      res = -ENOENT;
    }

	//Check if name is a regular file
	/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
	*/


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

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  printf("Readdir on path: %s\n",path);

	//check if directory
	if(!is_directory(path)&&strcmp(path,"/")){
    printf("Error %s not found\n",path);
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
  FILE *disk_image = fopen(".disk","rb+");

  //if root do list the directories
  if(!strcmp(path,"/")){
    rewind(disk_image);
    struct cs1550_root_directory *root = (struct cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));
    fread(root,sizeof(cs1550_root_directory),1,disk_image);

    int i =0;
    printf("Adding %d directories to ls\n",root->nDirectories);
    for(i = 0;i<root->nDirectories;i++){
      filler(buf,root->directories[i].dname,NULL,0);
    }
  }

  //if a subdirectory
  else{
    //read the root
    rewind(disk_image);
    struct cs1550_root_directory *root = (struct cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));
    fread(root,sizeof(cs1550_root_directory),1,disk_image);

    //loop variables
    int i =0;
    int j=0;
    //loop over directories
    for(i = 0;i<root->nDirectories;i++){
      if(!strcmp(root->directories[i].dname,directory)){
        //read in the subdirectory's files
        fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
        struct cs1550_directory_entry *sub = (struct cs1550_directory_entry *)calloc(1,sizeof(struct cs1550_directory_entry));
        fread(sub,sizeof(cs1550_directory_entry),1,disk_image);

        for(j=0;j<sub->nFiles;j++){
          filler(buf,concat(concat(sub->files[j].fname,"."),sub->files[j].fext),NULL,0);
        }
        break;
      }
    }
  }

  fclose(disk_image);
	return 0;
}


/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;


  if(level_find(path)>1){
    printf("Error: Attempting to create directory outside of the root directory\n");
    return -EPERM;
  }
  if(strlen(path)-1 > MAX_FILENAME){ //check if too big giving 1 extra for the root /
    return -ENAMETOOLONG;
  }
  if(is_directory(path)){
    printf("Error directory already exists\n");
    return -EEXIST;
  }

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  FILE *disk_image = fopen(".disk","rb+");
  if(disk_image==NULL){
    printf("Error file not opened");
  }
  fseek(disk_image,(DISK_SIZE)-BLOCK_SIZE,SEEK_SET); //seek to the last block
  struct cs1550_in_use *final_node = (cs1550_in_use *)calloc(1,sizeof(struct cs1550_in_use));
  fread(final_node,sizeof(struct cs1550_in_use),1,disk_image);
  long next_free_node = final_node->next_free_node;

  //if temp == 0 then this is the first time anytinh has been added meaning we need to initialize the root
  if(next_free_node == 0){
    printf("Initializing the root\n");
    //increment the next free node
    final_node->next_free_node++;
    fseek(disk_image,(DISK_SIZE)-BLOCK_SIZE,SEEK_SET);
    fwrite(final_node,sizeof(cs1550_in_use),1,disk_image);

    //create the root
    struct cs1550_root_directory *root = (struct cs1550_root_directory *)calloc(1,sizeof(cs1550_root_directory));
    rewind(disk_image);
    fwrite(root,sizeof(cs1550_root_directory),1,disk_image);

    free(root);
    next_free_node++;
  }
  printf("done with root part %d\n",next_free_node);
  //need to reopen the file if it was closed to initialize stuff
  fclose(disk_image);
  printf("reopening file\n");
  disk_image = fopen(".disk","rb+");
  if(disk_image==NULL){
      printf("Error file not opened");
  }


  printf("Past file reopening\n");
  struct cs1550_root_directory *root = (struct cs1550_root_directory *)calloc(1,BLOCK_SIZE);
  rewind(disk_image); //seek the file to the start of the root node
  fread(root,sizeof(cs1550_root_directory),1,disk_image);
  printf("past root reread\n");
  printf("root node %d\n",root->nDirectories);
  //check if root is full
  int next_location = root->nDirectories;
  if(next_location==MAX_DIRS_IN_ROOT){
    printf("Error cannot create new directory, root full\n");
    return -ENOSPC;
  }

  strcpy(root->directories[next_location].dname,directory);
  long freeblock = next_free_node;
  if(freeblock==DISK_SIZE/BLOCK_SIZE){
    fclose(disk_image);
    printf("Error cant make directory, out of free blocks\n");
    return -ENOSPC;
  }
  root->directories[next_location].nStartBlock = freeblock;
  //  printf("directories: %d\n",root->nDirectories);
  root->nDirectories++;
  //  printf("directories: %d\n",root->nDirectories);

  //write back the new root node
  rewind(disk_image);
  fwrite(root,sizeof(cs1550_root_directory),1,disk_image);

  struct cs1550_directory_entry *new_directory = (cs1550_directory_entry *)calloc(1,sizeof(cs1550_directory_entry));
  new_directory->nFiles = 0;

  //write the new directory
  fseek(disk_image,freeblock*BLOCK_SIZE,SEEK_SET);
  fwrite(new_directory,sizeof(cs1550_root_directory),1,disk_image);

  //update the last used block
  final_node->next_free_node++;
  fseek(disk_image,DISK_SIZE-BLOCK_SIZE,SEEK_SET);
  fwrite(final_node,sizeof(cs1550_in_use),1,disk_image);

  //close the disk
  fclose(disk_image);

  printf("Finished mkdir\n");
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

  //loop variables
  int i;

  //check to make sure the directory exists and the file does not exist before continuing
  if(!is_directory(path)){
    return -ENOENT;
  }

  //complain if already a file
  size_t filesize;
  if(is_file(path,&filesize)){
    return -EEXIST;
  }

  //complain if making in root
  if(level_find(path)<2){
    return -EPERM;
  }

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  //complain if the name is too long
  if(strlen(filename)>MAX_FILENAME+1 || strlen(extension)>MAX_EXTENSION+1){
    free(directory);
    free(filename);
    free(extension);
    return -ENAMETOOLONG;
  }

  //open the file and then down to the subdirectory
  FILE *disk_image = fopen(".disk","rb+");
  if(disk_image==NULL){
    printf("Error file not opened");
  }

  //malloc space for all of the disk blocks
  struct cs1550_root_directory * root = (struct cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));
  struct cs1550_directory_entry * sub = (struct cs1550_directory_entry *)calloc(1,sizeof(struct cs1550_directory_entry));
  struct cs1550_disk_block * new_file = (struct cs1550_disk_block *)calloc(1,sizeof(struct cs1550_disk_block));
  struct cs1550_in_use * last_block = (struct cs1550_in_use *)calloc(1,sizeof(struct cs1550_in_use));


  rewind(disk_image);
  fread(root,sizeof(cs1550_root_directory),1,disk_image);
  for(i=0;i<root->nDirectories;i++){
    if(!strcmp(root->directories[i].dname,directory)){
      //read the subdirectory
      fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
      fread(sub,sizeof(struct cs1550_directory_entry),1,disk_image);

      //get the next free node
      fseek(disk_image,DISK_SIZE-BLOCK_SIZE,SEEK_SET);
      fread(last_block,sizeof(struct cs1550_in_use),1,disk_image);
      long next_free_node = last_block->next_free_node;

      if(next_free_node == DISK_SIZE/BLOCK_SIZE){
        printf("Error: no free disk blocks\n");
        free(directory);
        free(filename);
        free(extension);
        free(last_block);
        free(root);
        free(sub);
        free(new_file);
        fclose(disk_image);
        return -ENOSPC;
      }

      //now that we found a matching directory, add the new file if possible
      fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
      fread(sub,sizeof(struct cs1550_directory_entry),1,disk_image);

      if(sub->nFiles<MAX_FILES_IN_DIR){
        strcpy(sub->files[sub->nFiles].fname,filename);
        strcpy(sub->files[sub->nFiles].fext,extension);
        sub->files[sub->nFiles].fsize = 0;
        sub->files[sub->nFiles].nStartBlock = next_free_node;
        sub->nFiles++;

        //create a new block for the file
        new_file->nNextBlock = -1; //mark -1 so read knows theres nothing more here
        fseek(disk_image,next_free_node*BLOCK_SIZE,SEEK_SET);
        fwrite(new_file,sizeof(struct cs1550_disk_block),1,disk_image);

        //write the updated files
        fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
        fwrite(sub,sizeof(struct cs1550_directory_entry),1,disk_image);

        //write the updated next free block
        last_block->next_free_node++;
        fseek(disk_image,DISK_SIZE-BLOCK_SIZE,SEEK_SET);
        fwrite(last_block,sizeof(struct cs1550_in_use),1,disk_image);
      }
      //complain if not enough space for a new file
      else{
        printf("Error no space for new file in this directory\n");
        free(directory);
        free(filename);
        free(extension);
        free(root);
        free(sub);
        free(new_file);
        free(last_block);
        fclose(disk_image);
        return -ENOSPC;
      }
      break; //no reason to continue loopign after finding the directory
    }
  }



  //clean up memory
  free(directory);
  free(filename);
  free(extension);
  free(root);
  free(sub);
  free(last_block);
  free(new_file);
  fclose(disk_image);
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

  size_t filesize;
  if(!is_file(path,&filesize)){
    return -EEXIST;
  }
  if(size<1){
    return 0;
  }
  if(filesize<offset){
    printf("Error: offset bigger than filesize\n");
    return -EPERM;
  }
  //loop variables
  int i,j,k,l;

  //flag marking that this will hit eof
  int eof_flag = 0;

  //hold next node
  long next_node;

  //keep track of place in buf
  long buf_index = 0;

  //current block in file
  int current_block = 0;

  //calculate the first block to contain data we are looking for
  int first_need = ((offset+MAX_DATA_IN_BLOCK-1)/MAX_DATA_IN_BLOCK);

  //calculate the final block that will be needed
  int last_need = ((size+offset)+MAX_DATA_IN_BLOCK-1)/(MAX_DATA_IN_BLOCK);
  if(((filesize+MAX_DATA_IN_BLOCK-1)/MAX_DATA_IN_BLOCK)<last_need){
    last_need = ((filesize+MAX_DATA_IN_BLOCK-1));
  }
  //track number of blocks that need to be read
  long num_to_read = ((size+offset)+MAX_DATA_IN_BLOCK-1)/(MAX_DATA_IN_BLOCK);

  //track bytes read
  size_t amt_read = 0;

  //split the file name
  char *directory = malloc(8*sizeof(char));
  char *filename = malloc(8*sizeof(char));
  char *extension = malloc(3*sizeof(char));
  sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  //open the file and then down to the subdirectory
  FILE *disk_image = fopen(".disk","rb+");
  if(disk_image==NULL){
    printf("Error file not opened");
  }

  //malloc space for all of the disk blocks
  struct cs1550_root_directory * root = (struct cs1550_root_directory *)calloc(1,sizeof(struct cs1550_root_directory));
  struct cs1550_directory_entry * sub = (struct cs1550_directory_entry *)calloc(1,sizeof(struct cs1550_directory_entry));
//  struct cs1550_disk_block * file_start = (struct cs1550_disk_block *)calloc(1,sizeof(struct cs1550_disk_block));
  struct cs1550_disk_block current_block = (struct cs1550_disk_block **)calloc(num_to_read,sizeof(struct cs1550_disk_block));
  //Find the file block
  //read in root
  rewind(disk_image);
  fread(root,sizeof(struct cs1550_root_directory),1,disk_image);
  for(i =0; i<root->nDirectories;i++){
    if(!strcmp(root->directories[i].dname,directory)){

      //now that we found a matching directory, loop over its files
      fseek(disk_image,root->directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
      fread(sub,sizeof(cs1550_directory_entry),1,disk_image);
      for(j=0;j<sub->nFiles;j++){
        if(!strcmp(sub->files[j].fname,filename)&&!strcmp(sub->files[j].fext,extension)){
          fseek(disk_image,sub->files[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
          fread(file[0],sizeof(struct cs1550_disk_block),1,disk_image);

          //add each necessary disk block to the array
          for(k = 0;k < num_to_read-1;k++){
            next_node = file[k]->nNextBlock;
            if(next_node>-1){
              fseek(disk_image,next_node*BLOCK_SIZE,SEEK_SET);
              fread(file[k+1],sizeof(cs1550_disk_block),1,disk_image);
            }
            //if EOF do this stuff
            else{
              num_to_read = k+1;
              eof_flag = 1;
              break;
            }
          }

          //loop over the requisite disk blocks and add their data to buf
          for(k = 0;k < num_to_read;k++){
            //case where none of the data in this block is needed
            if(MAX_DATA_IN_BLOCK*(k+1)<offset){
              continue;
            }
            //case where we will hit eof in this block
            if(k+1==num_to_read && eof_flag){
              int temp = (int)filesize % k; //get the amount of data in this the last block needed

            }


            //this case for some data in this block being needed
            if(MAX_DATA_IN_BLOCK*k<offset){
              for(l=0;l<MAX_DATA_IN_BLOCK;l++){
                buf[buf_index++];
              }
            }
            //this case for if we need all data in this block
          }
          break;
        }
      }
      break; //break as no need to search other unmatching directories
    }
  }

  //return the total number of bytes read
	size = amt_read;

  //clean up after myself
  free(directory);
  free(filename);
  free(extension);
  free(root);
  free(sub);
  free(file);
  free(last_block);
  fclose(disk_image);
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
