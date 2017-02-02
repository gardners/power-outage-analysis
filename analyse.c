#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int battery_life_in_minutes=0;

struct year {
  int counts[12][31][24];
};

struct year *years[10000]={NULL};


int endofmonth(int mday,int month, int year) {
  if (month<1||month>12) return 1;
  int days_in_month=31;
  switch(month) {
  case 4: case 6: case 9: case 11: days_in_month=30; break;
  case 2:
    if (year%4) days_in_month=28; else days_in_month=29;
    if ((!(year%100))&&(!(year%400))) days_in_month=28;
  }
  if (mday>days_in_month) return 1; else return 0;
}

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

    if (0) printf("Charge level = %0.3f @ %02d:%02d\n",
		  initial_charge_level,start_hour,start_min);

    while (duration>0) {
      if (initial_charge_level <= 0 ) {
	// Mark effect of outage
	printf("  %d phone(s) went flat at %d/%d/%d %02d:%02d until %d/%d/%04d %02d:%02d  (%d minutes)\n",
	       customers,
	       start_mday,start_month,start_year,
	       start_hour,start_min,
	       end_mday,end_month,end_year,
	       end_hour,end_min,
	       duration
	       );

	// Now mark the hourly bins to record the outage
	while(duration>0) {
	  printf("    still flat at %d/%d/%d %02d:00\n",
		 start_mday,start_month,start_year,
		 start_hour);
	  
	  if (!years[start_year]) {
	    years[start_year]=calloc(1,sizeof(struct year));
	    if (!years[start_year]) {
	      perror("calloc"); exit(-1);
	    }
	  }
	  years[start_year]->counts[start_month][start_mday][start_hour]+=customers;

	  // Now advance an hour
	  start_hour++;
	  if (start_hour>=24) {
	    start_hour=0; start_mday++;
	    if (endofmonth(start_mday,start_month,start_year)) {
	      start_mday=1; start_month++;
	      if (start_month>12) {
		start_month=1; start_year++;
	      }
	    }
	  }
	  
	  duration-=60;
	}
	
	break;
      } else {
	if (start_year<100) start_year+=2000;
	if (end_year<100) end_year+=2000;
	initial_charge_level -= minutely_discharge;
	duration--;
	start_min++;
	if (start_min>=60) {
	  start_min=0; start_hour++;
	  if (start_hour>=24) {
	    start_hour=0; start_mday++;
	    if (endofmonth(start_mday,start_month,start_year)) {
	      start_mday=1; start_month++;
	      if (start_month>12) {
		start_month=1; start_year++;
	      }
	    }
	  }
	}
	  
      }
    }
  }

  return 0;
}

int main(int argc,char **argv)
{
  if (argc!=3) {
    fprintf(stderr,"usage: analyse <battery life in minutes> <data file>\n");
    exit(-1);
  }

  for(int i=0;i<10000;i++) years[i]=NULL;

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
