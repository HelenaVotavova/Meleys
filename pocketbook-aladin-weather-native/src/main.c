#include <inkview.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA "/mnt/ext1/system/config/helcino-pocasi.dat"
#define TMP  "/mnt/ext1/system/config/helcino-pocasi.tmp"
#define URL  "http://38.19.198.69:8093/weather.dat"
#define MAXH 73
typedef struct { char date[8], hour[4]; float temp,rain,wind,gust,dir; int icon; } Hour;
static Hour hours[MAXH]; static int count; static char updated[32]="-", rec[240]="Predpoved zatim neni stazena.";
static ifont *title,*body,*small,*tiny,*advice;

static void text(int x,int y,int w,int h,const char *s,ifont *f,int flags){ SetFont(f,BLACK); DrawTextRect(x,y,w,h,s,flags); }
static int inside(int x,int y,int bx,int by,int bw,int bh){return x>=bx&&x<bx+bw&&y>=by&&y<by+bh;}
static void sun(int x,int y,int r){ int i; DrawCircle(x,y,r,BLACK); for(i=0;i<8;i++){double a=i*3.14159/4;DrawLine(x+cos(a)*(r+5),y+sin(a)*(r+5),x+cos(a)*(r+15),y+sin(a)*(r+15),BLACK);}}
static void cloud(int x,int y){ DrawCircle(x-12,y,15,BLACK);DrawCircle(x+8,y-7,19,BLACK);DrawRect(x-29,y,59,18,BLACK); }
static void icon(int x,int y,int kind){if(kind==0)sun(x,y,16);else if(kind==1){sun(x-15,y-12,13);cloud(x+5,y+4);}else{cloud(x,y);if(kind==3){DrawLine(x-18,y+24,x-24,y+37,BLACK);DrawLine(x,y+24,x-6,y+37,BLACK);DrawLine(x+18,y+24,x+12,y+37,BLACK);}}}
static void button(int x,int y,int w,int h,const char*s){DrawRect(x,y,w,h,BLACK);text(x,y,w,h,s,body,ALIGN_CENTER|VALIGN_MIDDLE);}

static int load(void){
 FILE*f=fopen(DATA,"r");char line[512];count=0;if(!f)return 0;
 while(fgets(line,sizeof line,f)){
  if(!strncmp(line,"META|",5)){char city[32],run[32];sscanf(line,"META|%31[^|]|%31[^|]|%31[^\n]",city,run,updated);}
  else if(!strncmp(line,"REC|",4)){strncpy(rec,line+4,sizeof(rec)-1);rec[strcspn(rec,"\r\n")]=0;}
  else if(!strncmp(line,"H|",2)&&count<MAXH){Hour*h=&hours[count];if(sscanf(line,"H|%7[^|]|%3[^|]|%f|%f|%f|%f|%f|%d",h->date,h->hour,&h->temp,&h->rain,&h->wind,&h->gust,&h->dir,&h->icon)==8)count++;}
 } fclose(f);return count;
}
static void refresh(void){
 char cmd[800];snprintf(cmd,sizeof cmd,"rm -f '%s'; /bin/busybox wget -q -T 25 -O '%s' '%s' || wget -q -T 25 -O '%s' '%s'; test -s '%s' && mv '%s' '%s'",TMP,TMP,URL,TMP,URL,TMP,TMP,DATA);
 if(system(cmd)==0)load();else snprintf(updated,sizeof(updated),"Chyba spojeni - zkontroluj WiFi");
}
static void repaint(void){
 int w=ScreenWidth(),left=78,right=w-38,top=340,bottom=605,n=count,i,x,px,py,minT=99,maxT=-99;
 if(n>12)n=12;ClearScreen();text(38,30,w-76,65,"Pocasi Brno",title,ALIGN_LEFT);button(w-270,24,230,65,"OBNOVIT");
 {char s[80];snprintf(s,sizeof s,"Aktualizace: %s   Zdroj: CHMI ALADIN",updated);text(40,105,w-80,45,s,small,ALIGN_LEFT);}
 DrawLine(38,160,w-38,160,BLACK);
 if(!count){text(50,260,w-100,150,"Pripoj WiFi a klepni na OBNOVIT.",body,ALIGN_CENTER|VALIGN_MIDDLE);FullUpdate();return;}
 for(i=0;i<n;i++){x=left+i*(right-left)/(n-1);icon(x,205,hours[i].icon);text(x-38,245,76,40,hours[i].hour,small,ALIGN_CENTER);if(hours[i].temp<minT)minT=hours[i].temp;if(hours[i].temp>maxT)maxT=hours[i].temp;}
 if(maxT-minT<3)maxT=minT+3; text(25,top-45,250,35,"TEPLOTA (C)",small,ALIGN_LEFT);DrawLine(left,top,left,bottom,BLACK);DrawLine(left,bottom,right,bottom,BLACK);
 for(i=0;i<n;i++){Hour*q=&hours[i];x=left+i*(right-left)/(n-1);py=bottom-(int)((q->temp-minT)/(maxT-minT)*(bottom-top));if(i){Hour*p=&hours[i-1];px=left+(i-1)*(right-left)/(n-1);int ppy=bottom-(int)((p->temp-minT)/(maxT-minT)*(bottom-top));DrawLine(px,ppy,x,py,BLACK);}FillArea(x-4,py-4,9,9,BLACK);{char s[16];snprintf(s,sizeof s,"%.0f",q->temp);text(x-25,py-43,50,34,s,small,ALIGN_CENTER);}}
 {int ry=675,rh=140;float maxR=1;for(i=0;i<n;i++)if(hours[i].rain>maxR)maxR=hours[i].rain;text(25,ry-40,300,35,"SRAZKY (mm/h)",small,ALIGN_LEFT);DrawLine(left,ry+rh,right,ry+rh,BLACK);for(i=0;i<n;i++){x=left+i*(right-left)/(n-1);int bh=(int)(hours[i].rain/maxR*rh);if(bh)FillArea(x-12,ry+rh-bh,24,bh,BLACK);}}
 {int wy=875;text(25,wy-42,350,35,"VITR / NARAZY (m/s)",small,ALIGN_LEFT);for(i=0;i<n;i++){char s[24];x=left+i*(right-left)/(n-1);snprintf(s,sizeof s,"%.0f/%.0f",hours[i].wind,hours[i].gust);text(x-38,wy,76,38,s,tiny,ALIGN_CENTER);}}
 {int by=995;DrawRect(35,by,w-70,485,BLACK);text(65,by+28,w-130,48,"CO SI VZIT NA SEBE",body,ALIGN_LEFT);text(65,by+90,w-130,355,rec,advice,ALIGN_CENTER|VALIGN_MIDDLE);}
 FullUpdate();
}
static int handler(int type,int p1,int p2){if(type==EVT_INIT){title=OpenFont(DEFAULTFONTB,46,1);body=OpenFont(DEFAULTFONT,32,1);small=OpenFont(DEFAULTFONTB,25,1);tiny=OpenFont(DEFAULTFONT,22,1);advice=OpenFont(DEFAULTFONTB,56,1);load();}else if(type==EVT_REPAINT){repaint();return 1;}else if(type==EVT_POINTERUP){int w=ScreenWidth();if(inside(p1,p2,w-270,24,230,75)){refresh();FlushEvents();repaint();}return 1;}else if(type==EVT_KEYDOWN&&p1==IV_KEY_BACK)CloseApp();else if(type==EVT_EXIT){CloseFont(title);CloseFont(body);CloseFont(small);CloseFont(tiny);CloseFont(advice);}return 0;}
int main(void){InkViewMain(handler);return 0;}
