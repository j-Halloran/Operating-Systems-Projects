/*
** CS 1550 Project 1
** Jake Halloran (jph74@pitt.edu)
** Last Edited: 9/26/16
*/


#include "iso_font.h" //font source file
#include <sys/types.h>//system types header
#include <sys/ioctl.h> //Used to change terminal functionality
#include <unistd.h> //Used to make necessary system calls
#include <termios.h> //Used to change terminal settings
#include <sys/mman.h> //Used to create mmap for frame buffer
#include <linux/fb.h> //Lets use of screen info macros
#include <sys/time.h> //used for sleeping
#include <fcntl.h> //used to get constants for syscalls

typedef unsigned short color_t;
void clear_screen(); //declare so can use in init
void draw_char(int, int, char, color_t);//same but for drawing text
const char* buffer_path = "/dev/fb0"; //easy storage of path to frame buffer
unsigned int * fb_addr; //storage of address to mmap
size_t mmap_size; //size_t is jsut an unsigned int defined in types.h
struct termios start_settings; //global struct so we can restore the original terminal settings
int buffer_fd;//file descriptor for fb0

void init_graphics(){
  struct fb_var_screeninfo resolution;//structure to hold screen resolution
  struct fb_fix_screeninfo bit_depth;//holds bit depth
  struct termios no_echo;
  
  buffer_fd = open(buffer_path, O_RDWR); //opens the fb0 buffer as read and write
  ioctl(buffer_fd, FBIOGET_VSCREENINFO, &resolution);//gets the resolution of the screen
  ioctl(buffer_fd, FBIOGET_FSCREENINFO, &bit_depth);//gets the bit depth of the screen
  mmap_size = resolution.yres_virtual * bit_depth.line_length;//calc for memory size memory=total space of the screen
  fb_addr = mmap(NULL,mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer_fd, 0); //maps shared fb0 memory

  
  ioctl(0, TCGETS, &start_settings); //read the current settings and save them
  no_echo = start_settings; //duplicate the original settings  
  no_echo.c_lflag &= ~ICANON; //turn off cannoical mode by bitwise anding w/ its inverse
  no_echo.c_lflag &= ~ECHO; //turn echo off by anding with its inverse
  ioctl(0, TCSETS, &no_echo); //sets the new settings to the terminal;
  
  clear_screen(); //clear the screen to start
}

//unmap memory, reset graphics
void exit_graphics(){
  ioctl(0, TCSETS, &start_settings); //resets terminal settings to original values
  munmap(fb_addr, mmap_size);   //free the frame buffer memory map
  close(buffer_fd); //close the fb0 file
}

//simply wipes the screen of all graphics
void clear_screen(){
  write(1,"\033[2J",7); //writes the terminal clear code, 1 for std output, 7 b/c 7 characters
}

//discovers user input of a single character to stdin and returns it using nonblocking calls
char getkey(){
 char return_value=0;
 fd_set rfds; //fileset that select listens for changes to
 struct timeval timer; //timer select uses to count how long it has waited
 int temp = 0;//temp to store the read return value
 
 //wait up to 1 seconds (taken from man page example)
 timer.tv_sec = 1;
 timer.tv_usec =0;
 
 FD_ZERO(&rfds); //reset file set
 FD_SET(0,&rfds); //adds stdin to the fileset
 return_value=select(0, &rfds, NULL, NULL, &timer); //use select call to wait up to 1 seconds to capture a character
 
 //Only attempts to read if select found valid data
 if(return_value!=-1){
  temp = read(0,&return_value,1); //read if select found data to read
 }
 return return_value;
}

//sleeps for the inputed time in milliseconds
void sleep_ms(long ms){
  struct timespec timer;
  struct timespec time_left;
  timer.tv_sec = 0;
  timer.tv_nsec = ms *1000000;
  nanosleep(&timer,&time_left);
}

//draws a pixel of specific 16-bit color at a given x and y
void draw_pixel(int x, int y,color_t color){
  unsigned short * color_addr;
  unsigned int location;
  
  location = (x%640) + (y%480)*640;//x mod 640 for x distance ymod480 gives the height*640 to count lines to skip
  color_addr = fb_addr; //duplicates address for pointer math
  color_addr = color_addr + location; //adds the offset to the pointer address
  *color_addr = color; //sets the color of the selected index
}
 
 //Thank you stackoverflow  
void draw_line(int x1, int y1, int x2, int y2, color_t color){
  int i,dx,dy,p,x,y,end;
  
  //Special case for vertical lines
  if(x1==x2){
    if(y1<y2){
      for(i=y1;i<y2;i++){
        draw_pixel(x1,i,color);
      }
    }
    if(y2<y1){
      for(i=y2;i<y1;i++){
        draw_pixel(x1,i,color);
      }
    }
  }
  
  //Special case for horizontal lines (not needed but oh well)
  else if(y1==y2){
    if(x1<x2){
      for(i=x1;i<x2;i++){
        draw_pixel(i,y1,color);
      }
    }
    else{
      for(i=x2;i<x1;i++){
        draw_pixel(i,y1,color);
      }
    }    
  }
  
  //Algorthmic implementation for all other lines
  else{
    dx = abs(x1 - x2); //the run of the graph
    dy = abs(y1 - y2); //the rise of the graph
    p = 2 * dy - dx; //error adjusting
    if(x1 > x2){ //coordinates in proper order
      x = x2;
      y = y2;
      end = x1;
    }
    else{ //coordinates passed backwards
        x = x1;
        y = y1;
        end = x2;
     }
    draw_pixel(x, y, color); //draws the first pixel
    
    //adds 1 to x every loop and 1 to y if the error becomes positive
    while(x < end){
      x = x + 1;
      if(p < 0){
        p = p + 2 * dy;
      }
      else{
        y = y + 1;
        p = p + 2 * (dy - dx);
      }
      draw_pixel(x, y, color); //draw the new pixel
    }
  }
}

//Takes an array of characters and passes them to be printed one at a time
void draw_text(int x, int y, const char *text, color_t color){
  //read each line of data
  //increment over each bit with a shift and mask
  //if the bit is one, draw a pixel there'
  int i = 0;
  for(;;){
    if(text[i]==0){ //if attempting to print null terminal break the loop
      break;
    }
    else{
      draw_char(x,y,text[i],color);//pass the actual position and character to draw to another function
      x+=16; //jump the x position over 16 pixels
      i++;//increment the text array
    }
  }
}

//responsible for actually drawing the text
void draw_char(int x, int y, char text, color_t color){
  int i = 0;//y loop counter
  int i2 = 0;//x pos loop counter
  unsigned char temp = 0;//holds the unsigned value given in iso_font for each row
  unsigned char temp2 = 0;//holds the result of the bit masking at each bit
  
  for(i=0;i<16;i++){ //loop of the 16 pixels of height
    temp = iso_font[text*16+i]; //read the unsigned value into temp
      for(i2 = 0;i2<16;i2++,temp>>=1){//loop over the 16 x position bits
        temp2 = temp & 1; //store temp2 as the inplace shift result each time (thanks COE 0147)
        if(temp2 > 0){
          draw_pixel(x+i2,y+i,color); //draw the pixel if the shift results in a >0 bit value
        }
        
      }
  }
}