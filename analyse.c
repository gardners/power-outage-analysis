#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// only needed to generate plot PDFs
#include "ft2build.h"
#include FT_FREETYPE_H
#include "hpdf.h"

typedef struct ts {
  int year;
  int month;
  int mday;

  int hour;
} timestamp;

// only needed to generate plot PDFs
const HPDF_UINT16 DASH_MODE1[] = {3};

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

int ts_set(timestamp *ts,char *s)
{
  if (sscanf(s,"%02d/%02d/%04d",&ts->mday,&ts->month,&ts->year)!=3) {
    fprintf(stderr,"Could not parse date '%s' -- should be DD/MM/YYYY\n",s);
    exit(-1);
  }
  ts->hour=0;
  return 0;
}

int ts_notequal(timestamp *a,timestamp *b)
{
  if (a->year!=b->year) return 1;
  if (a->month!=b->month) return 1;
  if (a->mday!=b->mday) return 1;
  if (a->hour!=b->hour) return 1;
  if (0)
    printf("%02d/%02d/%04d %02d:00 = %02d/%02d/%04d %02d:00\n",
	   a->mday,a->month,a->year,a->hour,
	   b->mday,b->month,b->year,b->hour);
  return 0;
}

int ts_lessthan(timestamp *a,timestamp *b)
{
  if (a->year<b->year) return 1;
  if (a->year>b->year) return 0;
  if (a->month<b->month) return 1;
  if (a->month>b->month) return 0;
  if (a->mday<b->mday) return 1;
  if (a->mday>b->mday) return 0;
  if (a->hour<b->hour) return 1;
  return 0;
}


int ts_advance(timestamp *t)
{
  t->hour++;
  if (t->hour<24) return 0;
  t->hour=0;
  t->mday++;
  if (endofmonth(t->mday,t->month,t->year)) {
    t->mday=1;
    t->month++;
  }
  if (t->month>12) { t->month=1; t->year++; }
  return 0;
}

// only needed to generate plot PDFs
void error_handler(HPDF_STATUS error_number, HPDF_STATUS detail_number,
		   void *data)
{
  fprintf(stderr,"HPDF error: %04x.%u\n",
	  (HPDF_UINT)error_number,(HPDF_UINT)detail_number);
  exit(-1);
}

// make some GLOBAL variables, so they can be used in any function

int battery_life_in_minutes=0;

// three-dimensional array of integers
struct year {
  int counts[13][32][24];
};

// 10,000 of the above 3D arrays
struct year *years[10000]={NULL};

int process_line(char *line,timestamp *start_epoch,timestamp *end_epoch)
{
  int offset=0;
  int commas=5;
  int len=strlen(line);

  while(commas&&(offset<len)) {
    if (line[offset]==',') commas--;
    offset++;
  }

  int duration;
  timestamp start;  int start_min;
  timestamp end; int end_min;
  int customers;
  
  // Parse something like "165,1/01/06 13:45,1/01/06 16:30,1"
  // of format INT_DURATION,FIRST_CUSTOMER_OFF_DATETIME,LAST_CUSTOMER_ON_DATETIME,CUSTOMERS_INT
  if (sscanf(&line[offset],"%d,%d/%d/%d %d:%d,%d/%d/%d %d:%d,%d",
	     &duration,
	     &start.mday,&start.month,&start.year,&start.hour,&start_min,
	     &end.mday,&end.month,&end.year,&end.hour,&end_min,
	     &customers)==12) {

    if (start.year<100) start.year+=2000;
    if (end.year<100) end.year+=2000;

    // Ignore out of range dates
    int ignore=0;
    if (ts_lessthan(&end,start_epoch)) ignore=1;
    if (ts_lessthan(end_epoch,&start)) ignore=2;

    if (0)
      printf("%04d/%02d/%02d %02d:00 -- %04d/%02d/%02d %02d:00 is in %sscope (%d) "
	     "%04d/%02d/%02d %02d:00 -- %04d/%02d/%02d %02d:00"
	     ".\n",
	     start.year,start.month,start.mday,start.hour,
	     end.year,end.month,end.mday,end.hour,
	     
	     ignore?"not ":"",ignore,
	     
	     start_epoch->year,start_epoch->month,start_epoch->mday,start_epoch->hour,
	     end_epoch->year,end_epoch->month,end_epoch->mday,end_epoch->hour
	     );
    

    if (ignore) return 0;
    
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
    if (start.hour<8) initial_charge_level=100.0;
    // 8am to 10pm = discharging
    float minutely_discharge = 100.0 / battery_life_in_minutes;
    float daily_discharge= (22 - 8) * 60.0 * minutely_discharge;
    if ((start.hour>=8)&&(start.hour<22))
      initial_charge_level=100.0 - minutely_discharge * ( (start.hour-8) * 60 + start_min );
    if (initial_charge_level<0) initial_charge_level=0;
    
    // 10pm to midnight = charging
    if (start.hour>=22) {
      initial_charge_level=100.0 - daily_discharge;
      if (initial_charge_level<0) initial_charge_level=0;
      initial_charge_level += ( ( (start.hour-22) * 60 + start_min ) / 120.0 ) * 100.0;
    }
    if (initial_charge_level>100) initial_charge_level=100;

    if (0) printf("Charge level = %0.3f @ %02d:%02d\n",
		  initial_charge_level,start.hour,start_min);

    while (duration>0) {
      if (initial_charge_level <= 0 ) {
	// Mark effect of outage
	if (0)
	  printf("  %d phone(s) went flat at %d/%d/%d %02d:%02d until %d/%d/%04d %02d:%02d  (%d minutes)\n",
	       customers,
	       start.mday,start.month,start.year,
	       start.hour,start_min,
	       end.mday,end.month,end.year,
	       end.hour,end_min,
	       duration
	       );

	// Now mark the hourly bins to record the outage
	while(duration>0) {
	  if (0)
	    printf("    still flat at %d/%d/%d %02d:00\n",
		   start.mday,start.month,start.year,
		   start.hour);
	  
	  if (!years[start.year]) {
	    years[start.year]=calloc(1,sizeof(struct year));
	    if (!years[start.year]) {
	      perror("calloc"); exit(-1);
	    }
	  }
	  years[start.year]->counts[start.month][start.mday][start.hour]+=customers;

	  // Now advance an hour
	  ts_advance(&start);
	  
	  duration-=60;
	}
	
	break;
      } else {
	initial_charge_level -= minutely_discharge;
	duration--;
	start_min++;
	if (start_min>=60) {
	  start_min=0;
	  ts_advance(&start);
	}	  
      }
    }
  }

  return 0;
}

// only needed to generate plot PDFs
int filled_rectange(HPDF_Page *page,
		    float r,float g,float b,
		    float x1,float y1, float w, float h)
{
  HPDF_Page_SetLineWidth(*page,0);
  HPDF_Page_SetLineCap(*page,HPDF_BUTT_END);
  HPDF_Page_SetLineJoin(*page,HPDF_MITER_JOIN);
  HPDF_Page_SetDash(*page,NULL,0,0);
  
  HPDF_Page_SetRGBFill (*page, r,g,b);
  HPDF_Page_SetRGBStroke (*page, r,g,b);
  HPDF_Page_Rectangle(*page, x1,y1,w,h);
  HPDF_Page_FillStroke(*page);
  
  return 0;
}

// only needed to generate plot PDFs
int line(HPDF_Page *page,
	 float r,float g,float b, float width,
	 float x1,float y1, float x2,float y2)
{
  HPDF_Page_SetLineWidth(*page,width);
  HPDF_Page_SetLineCap(*page,HPDF_BUTT_END);
  HPDF_Page_SetLineJoin(*page,HPDF_MITER_JOIN);
  HPDF_Page_SetDash(*page,NULL,0,0);
  
  HPDF_Page_SetRGBFill (*page, r,g,b);
  HPDF_Page_SetRGBStroke (*page, r,g,b);
  HPDF_Page_MoveTo(*page, x1,y1);
  HPDF_Page_LineTo(*page, x2,y2);
  HPDF_Page_Stroke(*page);
  return 0;
}

// only needed to generate plot PDFs
int dashed_line(HPDF_Page *page,
		float r,float g,float b, float width,
		float x1,float y1, float x2,float y2)
{
  HPDF_Page_SetLineWidth(*page,width);
  HPDF_Page_SetLineCap(*page,HPDF_BUTT_END);
  HPDF_Page_SetLineJoin(*page,HPDF_MITER_JOIN);
  HPDF_Page_SetDash(*page, DASH_MODE1, 1, 1);
  
  HPDF_Page_SetRGBFill (*page, r,g,b);
  HPDF_Page_SetRGBStroke (*page, r,g,b);
  HPDF_Page_MoveTo(*page, x1,y1);
  HPDF_Page_LineTo(*page, x2,y2);
  HPDF_Page_Stroke(*page);
  return 0;
}

// only needed to generate plot PDFs
float fig_width=6*72;
float fig_height=5*72;
float x_left=0.75*72;
float y_bottom=0.75*72;
float x_right=0.50*72;
float y_top=0.25*72;
HPDF_Font font_helvetica=NULL;

// only needed to generate plot PDFs
int draw_text(HPDF_Page *page,
	      char *text, float size,
	      float r, float g, float b,
	      float x, float y, float angle_degrees,
	      int halign, int valign)
{
  float radians = angle_degrees / 180 * 3.141592;
  
  HPDF_Page_SetFontAndSize (*page, font_helvetica, size);

  float text_width=HPDF_Page_TextWidth(*page,text);
  float text_height=HPDF_Font_GetCapHeight(font_helvetica);
  text_height=text_height*size/1000.0;

  if (halign==+1) x-=text_width;
  if (halign==0) x-=text_width/2;
  if (valign==+1) y-=text_height;
  if (valign==0) y-=text_height/2;
  
  HPDF_Page_BeginText (*page);
  HPDF_Page_SetTextRenderingMode (*page, HPDF_FILL);
  HPDF_Page_SetRGBFill (*page, r,g,b);
  HPDF_Page_SetTextMatrix (*page,
			   cos(radians), sin(radians),
			   -sin(radians), cos(radians),
			   x, y);
  //  HPDF_MoveTo(x,y);
  HPDF_Page_ShowText (*page, text);
  HPDF_Page_EndText (*page);

  return 0;
}

// only needed to generate plot PDFs
int y_tick(HPDF_Page *page,int value,float barscale)
{
  char value_string[1024];
  snprintf(value_string,1024,"%d",value);

  float v=value;
  
  line(page,0,0,0,1,
       x_left-4.5,y_bottom+(v*barscale),
       x_left,y_bottom+(v*barscale));    
  
  draw_text(page,
	    value_string,10,
	    0,0,0,	   
	    x_left-4.5-2.0,y_bottom+(value*barscale),
	    0,
	    +1,0);
  
  return 0;
}

// only needed to generate plot PDFs
int x_tick(HPDF_Page *page,char *text1, char *text2,
	   int value,float barwidth, int phase)
{
  line(page,0,0,0,1,
       x_left+value*barwidth,y_bottom,
       x_left+value*barwidth,y_bottom-6.0);

  int offset=0;
  if (phase&1) offset+=12;
  
  draw_text(page,
	    text1,10,
	    0,0,0,	   
	    x_left+value*barwidth,y_bottom-6.0-3.0-offset,
	    0,
	    0,+1);
  draw_text(page,
	    text2,10,
	    0,0,0,	   
	    x_left+value*barwidth,y_bottom-6.0-3.0-offset
	    -11, // font inter-line height
	    0,
	    0,+1);
  
  return 0;
}

// only needed to generate plot PDFs
int draw_event(HPDF_Page *page,char *text,timestamp event_ts,
	       timestamp *start,timestamp *end,
	       float barwidth,int number)
{
  int barnumber=0;
  timestamp t=*start;
  while(ts_notequal(&t,&event_ts)) {
    barnumber++;
    ts_advance(&t);
  }

  dashed_line(page,0,0,0,1,
	      x_left+barnumber*barwidth,y_bottom,
	      x_left+barnumber*barwidth,
	      (fig_height-y_top)-number*(2*12)
	      );
  draw_text(page,
	    text,10,
	    0,0,0,	   
	    x_left+barnumber*barwidth+3,
	    (fig_height-y_top)-number*(2*12),
	    0,
	    -1,+1);
  char timelabel[1024];
  snprintf(timelabel,1024,"%04d/%02d/%02d %02d:%02d",
	   event_ts.year,event_ts.month,event_ts.mday,
	   event_ts.hour,0);
  draw_text(page,
	    timelabel,10,
	    0,0,0,	   
	    x_left+barnumber*barwidth+3,
	    (fig_height-y_top)-number*(2*12)-12,
	    0,
	    -1,+1);

  
  return 0;
}

// only needed to generate plot PDFs
int draw_pdf_barplot_flatbatteries_vs_time(char *filename,
					   int battery_life_in_hours,
					   timestamp *start,timestamp *end,
					   char *events[])
{
  HPDF_Doc pdf;

  HPDF_Page page;
  
  pdf = HPDF_New(error_handler,NULL);
  if (!pdf) {
    fprintf(stderr,"Call to HPDF_New() failed.\n"); exit(-1); 
  }
  HPDF_SetCompressionMode (pdf, HPDF_COMP_ALL);
  HPDF_SetPageLayout(pdf,HPDF_PAGE_LAYOUT_TWO_COLUMN_LEFT);
  HPDF_AddPageLabel(pdf, 1, HPDF_PAGE_NUM_STYLE_DECIMAL, 1, "");

  HPDF_UseUTFEncodings(pdf); 
  HPDF_SetCurrentEncoder(pdf, "UTF-8"); 

  font_helvetica = HPDF_GetFont(pdf,"Helvetica","CP1250");
  
  page = HPDF_AddPage(pdf);

  // Page size in points
  // XXX - adjust accordingly
  HPDF_Page_SetWidth(page,fig_width);
  HPDF_Page_SetHeight(page,fig_height);

  // How many bars to draw? (and what is peak value?)
  int timespan_in_hours=0;
  timestamp cursor=*start;
  int peak=0;
  while(ts_notequal(&cursor,end)) {
    if (years[cursor.year]) {
      int count=years[cursor.year]->counts[cursor.month][cursor.mday][cursor.hour];
      if (count>peak) peak=count;
    }
    ts_advance(&cursor); timespan_in_hours++;    
  }

  //  int temp=peak;
  //peak=1;
  // while(peak<temp) peak=peak<<1;  
  
  float barwidth=(fig_width-x_left-x_right)/timespan_in_hours;

  // Allow 24 points of vertical space for each time event.
  int event_count=0;
  if (events) for(int e=0;events[e];e++) event_count++;
  int event_y_space=12*2*event_count;

  float barscale=(fig_height-y_bottom-y_top-event_y_space)/peak;

  
  fprintf(stderr,"Drawing barplot spanning %d hours, bardwidth=%f, scale=%f\n",
	  timespan_in_hours,barwidth,barscale);
  
  cursor=*start;
  int barnumber=0;  
  while(ts_notequal(&cursor,end)) {
    float x = x_left + barwidth*barnumber;
    int count=0;
    if (years[cursor.year])
      count=years[cursor.year]->counts[cursor.month][cursor.mday][cursor.hour];

    float height=count*barscale;

    if (0)
      printf("Plotting %04d/%02d/%02d %02d:00 (%d) as %f pixels high\n",
	     cursor.year,cursor.month,cursor.mday,cursor.hour,
	     count,height);
    
    filled_rectange(&page,0.5,0.5,0.5,
		    x,y_bottom,
		    barwidth,height);
    
    ts_advance(&cursor);
    barnumber++;
  }

  // Draw furniture

  // Axis lines
  line(&page,0,0,0,1,x_left,y_bottom,fig_width-x_right+4.5,y_bottom);
  line(&page,0,0,0,1,x_left,y_bottom,x_left,fig_height-y_top-event_y_space+4.5);

  // Y-axis scale ticks
  for (int n=0;n<=8;n++) y_tick(&page,n*peak/8,barscale);

  for (int n=0;n<=5;n++) {
    char label1[1024], label2[1024];
    timestamp c2 = *start;
    for (int j=0;j<timespan_in_hours*n/5;j++) ts_advance(&c2);
    snprintf(label1,1024,"%04d/%02d/%02d",
	     c2.year,c2.month,c2.mday);
    snprintf(label2,1024,"%02d:00",c2.hour);
    x_tick(&page,label1,label2,timespan_in_hours*n/5,barwidth,0);
  }

  // Axis labels
  char *ylabel="Number of phones with flat batteries according to model";
  if (!battery_life_in_hours) ylabel="Number of customers without electricity supply";
  draw_text(&page,
	    ylabel,10,
	    0,0,0,
	    // XXX - Why can't we calculate this relative to figure size etc
	    // and have it end up in the right place?
	    // 10,y_bottom+(fig_height-y_top-y_bottom)/2,90,
	    10,y_bottom+25,90,
	    -1,0);
  draw_text(&page,
	    "Point in time (hourly resolution)",10,
	    0,0,0,
	    // XXX - Why can't we calculate this relative to figure size etc
	    // and have it end up in the right place?
	    // 10,y_bottom+(fig_height-y_top-y_bottom)/2,90,
	    x_left+(fig_width-x_left-x_right)/2,10,0,
	    0,0);

  // Draw events
  if (events) {
    for(int e=0;events[e];e++) {
      char event_name[1024];
      timestamp event_ts;
      if (sscanf(events[e],"%02d/%02d/%04d %02d:%*d=%[^\n]",
		 &event_ts.mday,&event_ts.month,&event_ts.year,
		 &event_ts.hour,event_name)==5) {
	// Draw dashed line, time stamp and event description
	draw_event(&page,event_name,event_ts,
		   start,end,
		   barwidth,e);
      }
    }
  }

  
  HPDF_SaveToFile(pdf,filename);

  HPDF_Free(pdf);
  pdf=NULL;
  
  return 0;
}


int main(int argc,char **argv)
{
  // check input arguments
  if (argc<3) {
    fprintf(stderr,"usage: analyse <battery life in hours> <data file> [start date] [end date]\n");
    exit(-1);
  }

  // create pointers to time-strings
  char *start_epoch="01/01/2005";
  char *end_epoch="31/12/2016";

  // check input args, and replace time-strings accordingly
  if (argc>3) start_epoch=argv[3];
  if (argc>4) end_epoch=argv[4];

  // create array of pointers to chars
  // BG: undocumented feature here, seems the events[] array can be initially populated using CLI input argc
  char *events[1024];
  int event_count=0;
  for(int e=5;e<argc;e++) {
    events[event_count++]=argv[e];
  }
  events[event_count]=NULL;
  

  // create time structures to allow easy time-comparisons  
  timestamp ts;
  ts_set(&ts,start_epoch);
  timestamp end_ts;
  ts_set(&end_ts,end_epoch);
  end_ts.hour=23;

  fprintf(stderr,"Analysing data from %02d/%02d/%04d to %02d/%02d/%04d\n",
	  ts.mday,ts.month,ts.year,
	  end_ts.mday,end_ts.month,end_ts.year);
    
  // initialise all the values in the 'years' array to 'nothing'
  for(int i=0;i<10000;i++) years[i]=NULL;

  // seems to just delete the file.
  // NOTE that this file is later opened and reused  
  unlink("flatbatteryhours_versus_batterylife.csv");
  
  // convert the input argument of X hours, to be Y minutes, Y=X*60
  // create a pointer to a string, which is the input data-file
  int max_battery_life_in_minutes=atoi(argv[1])*60;
  char *data_file=argv[2];

  battery_life_in_minutes=0;
  while(battery_life_in_minutes<=max_battery_life_in_minutes) {

    // here we do a loop, starting at zero, and going up to the desired battery life.
    // the desired battery life was entered-in when you started the program, then x60.

    // first time round this seems to do nothing because we already initialised these to zero, but
    // within this loop, each iteration, we need to release the memory used pointed to by the years[]
    for(int i=0;i<10000;i++) {
      if (years[i]) free(years[i]);
      years[i]=NULL;
    }
    
    // create some variables

    FILE *f=fopen(data_file,"r");
    
    char line[1024];
    int line_len=0;
    int c;

    // read in the input data-file, 
    // at the end of each line, call the "process_line" function    
    while ((c=fgetc(f))!=EOF) {
      if (c>=' ') {
	line[line_len++]=c;
	line[line_len]=0;
      } else {    
	process_line(line,&ts,&end_ts);
	line_len=0;
      }
    }
    // do one last function call to "process_line" with the last line of the file
    if (line_len) process_line(line,&ts,&end_ts);

    // close the file.
    fclose(f);

    // we are now finished reading the data-file, and have processed the incoming data
    // insofar as we can now/later analyse the data.
    
    long long total=0;

    // create a string representing the filename of the output file
    char filename[1024];
    snprintf(filename,1024,"numberofflatphones_by_hour_batterylife=%dhours.csv",
	     battery_life_in_minutes/60);

    // oopen the output file for writing
    f=fopen(filename,"w");
    
    // Write hourly impact histogram (just the first line of the output file)
    fprintf(f,"time,flat_phones\n");
    int peak=0;

    timestamp cursor=ts;


    while(ts_notequal(&cursor,&end_ts)) {
      if (years[cursor.year]) {
	int count=years[cursor.year]->counts[cursor.month][cursor.mday][cursor.hour];
	if (count>peak) peak=count;
	total+=count;
	fprintf(f,"%04d-%02d-%02d %02d:00:00,%d\n",
		cursor.year,cursor.month,cursor.mday,cursor.hour,count);	
      }
      ts_advance(&cursor);
    }
    fclose(f);

    snprintf(filename,1024,"flatbatteries_vs_time_batterlife=%dhours.pdf",
	     battery_life_in_minutes/60);
    draw_pdf_barplot_flatbatteries_vs_time(filename,
					   battery_life_in_minutes/60,
					   &ts,&end_ts,
					   events);
    
    f=fopen("flatbatteryhours_versus_batterylife.csv","a");
    if (!battery_life_in_minutes)
      fprintf(f,"batterylifeinhours,totalflatbatteryhours\n");
    fprintf(f,"%d,%lld\n",battery_life_in_minutes/60,total);
    fclose(f);

    battery_life_in_minutes+=60;
    printf("Analysing situation with battery life = %d hours.\n",
	   battery_life_in_minutes/60);
  }
  
  return 0;
}
