/*
* Header file for prodcons.c
* Doesn't do much just included for completeness */


/*Node for the semaphore queue*/
struct My_Node{
  struct task_struct *task;
  struct My_Node *next;
};
//semaphore struct
struct cs1550_sem{
  int value;
  //My_Node is defined within sys.c
  struct My_Node *head; //head of the process link list
  struct My_Node *tail; //tail of the linked list
};
void up(struct cs1550_sem *); //my up call
void down(struct cs1550_sem *);//my down call
