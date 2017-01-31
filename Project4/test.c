

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

//Don't change this.
int main(int argc, char *argv[])
{
	char  test[50] = "a/root/file.txt";
  char * token;

  token = strtok(test,"/");
  while(token!=NULL){
    printf("%s\n",token);
    token = strtok(NULL,"/");
  }
}
