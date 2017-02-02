#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int battery_life_in_minutes=0;

struct year {
  int count[12][31][24];
};

struct year *years[10000]={NULL};

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

    /* Our algorithm is simple:

       Assumptions:

       1. We assume that batteries are charged from 10pm each night, and take
          2 hours to fully charge.
       2. We assume that phones are removed from charge at 8am each morning, and
          last <battery_life_in_minutes> before going flat.
       3. All customers are without power for the full duration of the outage.
      
       We therefore, for each outage workout how full the batteries were when the
       outage begins, and work out how long they will last.  With that information,
       we then work out if they batteries would go flat before the outage ends.
       If so, we indicate the number of flat batteries in each hourly bin.
    */

    float initial_charge_level=0;

    // midnight to 8am = full
    if (start_hour<8) initial_charge_level=100.0;
    // 8am to 10pm = discharging
    float minutely_discharge = 100.0 / battery_life_in_minutes;
    float daily_discharge= (22 - 8) * 60.0 * minutely_discharge;
    if ((start_hour>=8)&&(start_hour<22))
      initial_charge_level=100.0 - minutely_discharge * ( (start_hour-8) * 60 + start_min );
    if (initial_charge_level<0) initial_charge_level=0;
    
    // 10pm to midnight = charging
    if (start_hour>=22) {
      initial_charge_level=100.0 - daily_discharge;
      if (initial_charge_level<0) initial_charge_level=0;
      initial_charge_level += ( ( (start_hour-22) * 60 + start_min ) / 120.0 ) * 100.0;
    }
    if (initial_charge_level>100) initial_charge_level=100;

    printf("Charge level = %0.3f @ %02d:%02d\n",
	   initial_charge_level,start_hour,start_min);
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
