typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_line(int x1, int y1, int x2, int y2, color_t c);

int main(int argc, char** argv)
{
	int i,x_dist;

	init_graphics();

	char key[2];
	int x = (640-20)/2;
	int y = (480-20)/2;
  
  //draws several horizontal pixel lines
  for(i=0;i<10;i++){
    for(x_dist=0;x_dist<200;x_dist++){
      draw_pixel(x+x_dist,y,15);
    }
    y+=i;
  }  
  //prints a input string message
  draw_text(20,20,"Enter a character:",32000);
  key[0] = getkey();
  key[1] = '\0';
  draw_text(20,40,key,15); //prints a user entered character
  
  //repeated sleep testing
  sleep_ms(200);
  sleep_ms(200);
  sleep_ms(200);
  sleep_ms(200);
  sleep_ms(200);
  
  //tests clear screen
  clear_screen();
  
  //draws a screen clipping diagonal line
  draw_line(300,200,100,100,15);
  draw_line(20,20,80,80,1500);
	exit_graphics();

	return 0;

}
