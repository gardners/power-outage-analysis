#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int battery_life_in_minutes=0;

int process_line(char *line)
{
  int offset=0;
  int commas=5;
  int len=strlen(line);

  while(commas&&(offset<len)) {
    if (line[offset]==',') commas--;
    offset++;
  }

  int duration;
  int start_mday, start_month, start_year, start_hour, start_min;
  int end_mday, end_month, end_year, end_hour, end_min;
  int customers;
  
  // Parse something like "165,1/01/06 13:45,1/01/06 16:30,1"
  // of format INT_DURATION,FIRST_CUSTOMER_OFF_DATETIME,LAST_CUSTOMER_ON_DATETIME,CUSTOMERS_INT
  if (sscanf(&line[offset],"%d,%d/%d/%d %d:%d,%d/%d/%d %d:%d,%d",
	     &duration,
	     &start_mday,&start_month,&start_year,&start_hour,&start_min,
	     &end_mday,&end_month,&end_year,&end_hour,&end_min,
	     &customers)==12) {
    printf("%d\n",customers);
  }

  return 0;
}

int main(int argc,char **argv)
{
  if (argc!=3) {
    fprintf(stderr,"usage: analyse <battery life in minutes> <data file>\n");
    exit(-1);
  }

  battery_life_in_minutes=atoi(argv[1]);
  char *data_file=argv[2];

  FILE *f=fopen(data_file,"r");

  char line[1024];
  int line_len=0;
  int c;
  
  while ((c=fgetc(f))!=EOF) {
    if (c>=' ') {
      line[line_len++]=c;
      line[line_len]=0;
    } else {    
      process_line(line);
      line_len=0;
    }
  }
  if (line_len) process_line(line);    
  
  return 0;
}
