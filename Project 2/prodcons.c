/*Jake Halloran (jph74@pitt.edu)
  CS 1550 Project 2
  Last Edited 10/16/16*/

#include <stdio.h>
#include <stdlib.h>
#include <linux/unistd.h> //include the syscalls
#include <sys/wait.h> //to easily wait at code end
#include <sys/types.h> //don't think this is used anymore but dont want to break the code through removal
#include <sys/mman.h> //for mmap
#include <string.h> //for memset

//Struct for the semaphores redefined to avoid passing headers around
struct cs1550_sem{
  int value;
  //My_Node is defined within sys.c
  struct My_Node *head; //head of the process link list
  struct My_Node *tail; //tail of the linked list
};

//make using up pretty
void up(struct cs1550_sem *sem){
  syscall(__NR_cs1550_up, sem);
}

//make using down pretty
void down(struct cs1550_sem *sem){
  syscall(__NR_cs1550_down, sem);
}

int main(int argc, char *argv[]){
  //Make sure we have the correct arguments
  if(argc != 4){
    printf("Arguments must be in form of ./prodocons [#producers] [#consumers] [buffer_size]\n");
    exit(1);
  }

  //store command args
  int producers = atoi(argv[1]);
  int consumers = atoi(argv[2]);
  int buffer_size = atoi(argv[3]);

  //make sure arguments are reasonable
  if(buffer_size < 1 || producers < 1 || consumers < 1){
    printf("The arguments must be positive values.\n");
    exit(1);
  }

  //Find size of mmap needed
  int map_size = (3*sizeof(struct cs1550_sem))+buffer_size+3;

  //initialize the mmap before forking processes
  void * base_ptr = (void *)mmap(NULL, map_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,0,0);

  //initialize all 3 semaphores
  struct cs1550_sem *empty = (struct cs1550_sem *) base_ptr;
  //was getting weird errors doing it all at once so here is just memory incrementing
  base_ptr+=sizeof(struct cs1550_sem );
  //full semaphore
  struct cs1550_sem *full = (struct cs1550_sem *)base_ptr;
  base_ptr+=sizeof(struct cs1550_sem );
  //mutex semaphore
  struct cs1550_sem *mutex = (struct cs1550_sem *)base_ptr;
  base_ptr+=sizeof(struct cs1550_sem );

  //empty value = buffer size, full = 0, mutex = 1 as initial values
  empty->value = buffer_size;
  empty->head = NULL;
  empty->tail = NULL;
  full->value = 0;
  full->head = NULL;
  full->tail = NULL;
  mutex->value=1;
  mutex->head=NULL;
  mutex->tail=NULL;

  //Initialize tracking pointers for the consumers, producer, and buffer
  int *buffer_size_ptr = (int *)base_ptr;
  base_ptr+=sizeof(int *);

  //tracker for producer number
  int *producer_ptr = (int *)base_ptr;
  base_ptr+=sizeof(int *);

  //tracker for consumer number
  int *consumer_ptr = (int *)base_ptr;
  base_ptr+=sizeof(int *);

  //base of the buffer
  int *buffer_ptr = (int *)base_ptr;
  *buffer_size_ptr = buffer_size;

  //set first producer and consumer to 0
  *producer_ptr = 0;
  *consumer_ptr = 0;

  //0 out the buffer
  memset(buffer_ptr,0,buffer_size);

  int i =0; //loop index

  //Debugging code that can be optionally used to show initial values
  printf("Buffer size %d\n empty->value %d\n full->value %d\n mutex->value %d\n",buffer_size,empty->value,full->value,mutex->value);
  printf("producer: %d\n consumer %d\n",*producer_ptr, *consumer_ptr);

  //Create producers until the requisite number have been forked
  for(i = 0; i<producers; i++){
    if(fork()==0){//if child process do stuff
      int pancake; //storage for the production
      while(1){
        down(empty); //decrement the empty mutex
        down(mutex); //lock the mutex

        pancake = *producer_ptr; //get the number of the pancake being made
        buffer_ptr[pancake] = pancake; //store the created pancake
        printf("Chef %c Produced: Pancake%d\n", i+65, pancake); //add 65 to change the loop index into ascii character
        *producer_ptr = (*producer_ptr+1) % buffer_size; //mark the next value to produce

        up(mutex); //unlock the critical region
        up(full); //increment the full semaphore
      }
    }
  }

  //create consumers until we have as many as are needed
  for(i = 0; i<consumers; i++){
    if(fork()==0){//if child process, do stuff
      int pancake; //storage for the consumption
      while(1){
        down(full); //down on the full semaphore
        down(mutex); //lock the critical region

        pancake = buffer_ptr[*consumer_ptr]; //get the pancake being eaten
        if(pancake == 0){
          printf("empty value %d\n",empty->value);
          printf("mutex value %d\n",mutex->value);
          printf("full value %d\n",full->value);
        }
        printf("Customer %c Consumed: Pancake%d\n",i+65,pancake); //print the eaten pancake
        *consumer_ptr = (*consumer_ptr+1) % buffer_size; //find the next pancake to eat

        up(mutex); //exit the critical region
        up(empty); //increment the empty semaphore
      }
    }
  }

  //properly force the parent process to wait and not end
  int status;
  wait(&status);

  return 0;
}
