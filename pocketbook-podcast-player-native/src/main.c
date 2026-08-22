#include <inkview.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ROOT "/mnt/ext1/system/config/helciny-podcasty"
#define DATA ROOT "/catalog.dat"
#define CFG ROOT "/settings"
#define BASE "http://38.19.198.69:8094/"
#define MAXE 180
typedef struct { char feed[16],key[20],title[160],date[40],duration[16],image[80],audio[700]; } Episode;
static Episode ep[MAXE]; static int count,view,feed,page,autoplay; static ifont *f_title,*f_body,*f_small;
static const char *feed_ids[]={"karlik","minecraft","otazky"};
static const char *feed_names[]={"Karlikovy minecrafticke pohadky","Minecraft pribehy na dobrou noc","Same otazky"};

static void txt(int x,int y,int w,int h,const char*s,ifont*f,int flags){SetFont(f,BLACK);DrawTextRect(x,y,w,h,s,flags|DOTS);}
static void btn(int x,int y,int w,int h,const char*s,int dark){if(dark){FillArea(x,y,w,h,BLACK);SetFont(f_body,WHITE);}else{DrawRect(x,y,w,h,BLACK);SetFont(f_body,BLACK);}DrawTextRect(x,y+8,w,h-12,s,ALIGN_CENTER|VALIGN_MIDDLE);}
static int hit(int x,int y,int bx,int by,int bw,int bh){return x>=bx&&x<bx+bw&&y>=by&&y<by+bh;}
static void mkdirs(void){mkdir(ROOT,0777);mkdir(ROOT "/images",0777);}
static void save_cfg(void){FILE*f=fopen(CFG,"w");if(f){fprintf(f,"%d\n",autoplay);fclose(f);}}
static void load_cfg(void){FILE*f=fopen(CFG,"r");if(f){fscanf(f,"%d",&autoplay);fclose(f);}}
static int split(char*s,char**v,int max){int n=0;while(n<max&&(v[n]=strsep(&s,"|"))!=NULL)n++;return n;}
static int load_catalog(void){FILE*f=fopen(DATA,"r");char line[1300];count=0;if(!f)return 0;while(fgets(line,sizeof line,f)){char*p[10];line[strcspn(line,"\r\n")]=0;if(line[0]!='E'||split(line,p,10)<8||count>=MAXE)continue;Episode*e=&ep[count++];snprintf(e->feed,sizeof e->feed,"%s",p[1]);snprintf(e->key,sizeof e->key,"%s",p[2]);snprintf(e->title,sizeof e->title,"%s",p[3]);snprintf(e->date,sizeof e->date,"%s",p[4]);snprintf(e->duration,sizeof e->duration,"%s",p[5]);snprintf(e->image,sizeof e->image,"%s",p[6]);snprintf(e->audio,sizeof e->audio,"%s",p[7]);}fclose(f);return count;}
static void refresh(void){char cmd[900];mkdirs();snprintf(cmd,sizeof cmd,"/bin/busybox wget -q -T 30 -O '%s.tmp' '%scatalog.dat' && mv '%s.tmp' '%s'",DATA,BASE,DATA,DATA);Message(ICON_INFORMATION,"Podcasty","Nacitam nove epizody...",2500);if(system(cmd)||!load_catalog())Message(ICON_ERROR,"Podcasty","Nacitani selhalo. Zkontroluj WiFi.",4000);}
static int indices(int*out){int i,n=0,start=page*5;for(i=0;i<count;i++)if(!strcmp(ep[i].feed,feed_ids[feed])){if(n>=start&&n<start+5)out[n-start]=i;n++;}return n;}
static void fetch_image(Episode*e){char path[300],cmd[1000];if(!e->image[0])return;snprintf(path,sizeof path,ROOT "/images/%s.jpg",e->key);if(!access(path,R_OK))return;snprintf(cmd,sizeof cmd,"/bin/busybox wget -q -T 15 -O '%s.tmp' '%s%s' && mv '%s.tmp' '%s'",path,BASE,e->image,path,path);system(cmd);}
static void draw_home(void){int w=ScreenWidth(),h=ScreenHeight(),i;txt(40,35,w-80,70,"Helciny podcasty",f_title,ALIGN_LEFT);txt(40,115,w-80,48,"Online poslech pres WiFi",f_small,ALIGN_LEFT);for(i=0;i<3;i++)btn(55,245+i*180,w-110,125,feed_names[i],0);btn(55,h-300,w-110,95,autoplay?"Dalsi dil: ZAPNUTO":"Dalsi dil: VYPNUTO",autoplay);btn(55,h-175,w-110,90,"OBNOVIT KATALOG",0);}
static void draw_list(void){int w=ScreenWidth(),h=ScreenHeight(),ids[5],total=indices(ids),shown=total-page*5,i;char s[100];if(shown>5)shown=5;txt(35,28,w-70,58,feed_names[feed],f_title,ALIGN_LEFT);snprintf(s,sizeof s,"Epizody %d-%d z %d",page*5+1,page*5+shown,total);txt(38,95,w-76,42,s,f_small,ALIGN_LEFT);for(i=0;i<shown;i++){Episode*e=&ep[ids[i]];int y=155+i*260;char path[300],meta[80];fetch_image(e);snprintf(path,sizeof path,ROOT "/images/%s.jpg",e->key);ibitmap*bm=LoadJPEG(path,190,190,0,0,1);DrawRect(40,y,190,190,BLACK);if(bm){DrawBitmap(40,y,bm);free(bm);}txt(260,y,w-300,105,e->title,f_body,ALIGN_LEFT|VALIGN_MIDDLE);snprintf(meta,sizeof meta,"%s   %s",e->duration,e->date);txt(260,y+125,w-300,50,meta,f_small,ALIGN_LEFT);DrawLine(40,y+220,w-40,y+220,BLACK);}btn(35,h-125,175,80,"ZPET",0);btn(w-420,h-125,175,80,"<",0);btn(w-220,h-125,175,80,">",0);}
static void play_episode(int selected){int ids[5],total=indices(ids),all[MAXE],n=0,i,pos=page*5+selected;char **playlist;if(pos>=total)return;for(i=0;i<count;i++)if(!strcmp(ep[i].feed,feed_ids[feed]))all[n++]=i;playlist=calloc(n+1,sizeof(char*));if(!playlist)return;for(i=0;i<n;i++)playlist[i]=ep[all[i]].audio;LoadPlaylist(playlist);SetPlayerMode(autoplay?MP_CONTINUOUS:MP_ONCE);PlayTrack(pos);OpenPlayer();free(playlist);}
static void repaint(void){ClearScreen();if(view==0)draw_home();else draw_list();FullUpdate();}
static void touch(int x,int y){int w=ScreenWidth(),h=ScreenHeight(),i;if(view==0){for(i=0;i<3;i++)if(hit(x,y,55,245+i*180,w-110,125)){feed=i;page=0;view=1;repaint();return;}if(hit(x,y,55,h-300,w-110,95)){autoplay=!autoplay;save_cfg();repaint();}else if(hit(x,y,55,h-175,w-110,90)){refresh();repaint();}}else{if(y>=155&&y<1455){i=(y-155)/260;if(i<5)play_episode(i);}else if(hit(x,y,35,h-140,190,100)){view=0;repaint();}else if(hit(x,y,w-430,h-140,190,100)&&page>0){page--;repaint();}else if(hit(x,y,w-230,h-140,200,100)){int ids[5];if((page+1)*5<indices(ids)){page++;repaint();}}}}
static int handler(int type,int p1,int p2){if(type==EVT_INIT){f_title=OpenFont(DEFAULTFONTB,42,1);f_body=OpenFont(DEFAULTFONT,30,1);f_small=OpenFont(DEFAULTFONT,23,1);mkdirs();load_cfg();load_catalog();repaint();}else if(type==EVT_REPAINT)repaint();else if(type==EVT_POINTERUP)touch(p1,p2);else if(type==EVT_KEYDOWN&&p1==IV_KEY_BACK){if(view){view=0;repaint();}else CloseApp();}else if(type==EVT_EXIT){CloseFont(f_title);CloseFont(f_body);CloseFont(f_small);}return 0;}
int main(void){InkViewMain(handler);return 0;}
