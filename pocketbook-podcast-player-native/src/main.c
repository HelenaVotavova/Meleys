#include <inkview.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ROOT "/mnt/ext1/system/config/helciny-podcasty"
#define AUDIO_DIR "/mnt/ext1/Podcasts"
#define DATA ROOT "/catalog.dat"
#define CFG ROOT "/settings"
#define IMAGE_CACHE_V2 ROOT "/images-v2"
#define AUDIO_TMP AUDIO_DIR "/HelcinyPodcasty.mp3"
#define AUDIO_PART AUDIO_DIR "/HelcinyPodcasty.part"
#define DEBUG_LOG ROOT "/playback.log"
#define BASE "http://38.19.198.69:8093/podcasts/"
#define MAXE 180
typedef struct { char feed[16],key[20],title[160],date[40],duration[16],image[80],audio[700]; } Episode;
static Episode ep[MAXE]; static int count,view,feed,page,autoplay,current_audio=-1; static ifont *f_title,*f_body,*f_small;
static pid_t download_pid=-1; static int pending_audio=-1;
static char status_text[100]="";
static const char *feed_ids[]={"karlik","minecraft","otazky"};
static const char *feed_names[]={"Karlikovy minecrafticke pohadky","Minecraft pribehy na dobrou noc","Same otazky"};
static void repaint(void);
static void playback_log(const char*s){FILE*f=fopen(DEBUG_LOG,"a");if(f){fprintf(f,"%ld %s\n",(long)time(NULL),s);fflush(f);fsync(fileno(f));fclose(f);}}
static void load_last_log(void){FILE*f=fopen(DEBUG_LOG,"r");char line[180],last[180]="";if(!f)return;while(fgets(line,sizeof line,f))snprintf(last,sizeof last,"%s",line);fclose(f);last[strcspn(last,"\r\n")]=0;if(last[0])snprintf(status_text,sizeof status_text,"Diagnostika: %.75s",strchr(last,' ')?strchr(last,' ')+1:last);}

static void txt(int x,int y,int w,int h,const char*s,ifont*f,int flags){SetFont(f,BLACK);DrawTextRect(x,y,w,h,s,flags|DOTS);}
static void btn(int x,int y,int w,int h,const char*s,int dark){if(dark){FillArea(x,y,w,h,BLACK);SetFont(f_body,WHITE);}else{DrawRect(x,y,w,h,BLACK);SetFont(f_body,BLACK);}DrawTextRect(x,y+8,w,h-12,s,ALIGN_CENTER|VALIGN_MIDDLE);}
static int hit(int x,int y,int bx,int by,int bw,int bh){return x>=bx&&x<bx+bw&&y>=by&&y<by+bh;}
static void mkdirs(void){mkdir(ROOT,0777);mkdir(ROOT "/images",0777);mkdir(AUDIO_DIR,0777);}
static void migrate_image_cache(void){DIR*d;struct dirent*de;char path[400];FILE*f;if(!access(IMAGE_CACHE_V2,R_OK))return;d=opendir(ROOT "/images");if(d){while((de=readdir(d)))if(strstr(de->d_name,".jpg")){snprintf(path,sizeof path,ROOT "/images/%s",de->d_name);remove(path);}closedir(d);}f=fopen(IMAGE_CACHE_V2,"w");if(f){fputs("2\n",f);fclose(f);}}
static void save_cfg(void){FILE*f=fopen(CFG,"w");if(f){fprintf(f,"%d\n",autoplay);fclose(f);}}
static void load_cfg(void){FILE*f=fopen(CFG,"r");if(f){fscanf(f,"%d",&autoplay);fclose(f);}}
static int split(char*s,char**v,int max){int n=0;while(n<max&&(v[n]=strsep(&s,"|"))!=NULL)n++;return n;}
static int load_catalog(void){FILE*f=fopen(DATA,"r");char line[2048];count=0;if(!f)return 0;while(fgets(line,sizeof line,f)){char*p[10];int complete=strchr(line,'\n')||feof(f);if(!complete){while(fgets(line,sizeof line,f)&&!strchr(line,'\n')){}continue;}line[strcspn(line,"\r\n")]=0;if(line[0]!='E'||split(line,p,10)<8||count>=MAXE)continue;if(strncmp(p[7],"http://",7)&&strncmp(p[7],"https://",8))continue;Episode*e=&ep[count++];snprintf(e->feed,sizeof e->feed,"%s",p[1]);snprintf(e->key,sizeof e->key,"%s",p[2]);snprintf(e->title,sizeof e->title,"%s",p[3]);snprintf(e->date,sizeof e->date,"%s",p[4]);snprintf(e->duration,sizeof e->duration,"%s",p[5]);snprintf(e->image,sizeof e->image,"%s",p[6]);snprintf(e->audio,sizeof e->audio,"%s",p[7]);}fclose(f);return count;}
static void refresh(void){int rc;char cmd[1200],url[300];mkdirs();FillArea(35,95,ScreenWidth()-70,55,WHITE);txt(40,100,ScreenWidth()-80,42,"Obnovuji katalog pres WiFi...",f_small,ALIGN_LEFT);PartialUpdate(35,95,ScreenWidth()-70,55);snprintf(url,sizeof url,"%scatalog.dat",BASE);snprintf(cmd,sizeof cmd,"rm -f '%s.tmp'; (/bin/busybox wget -q -T 30 -O '%s.tmp' '%s' || /usr/bin/wget -q -T 30 -O '%s.tmp' '%s' || wget -q -T 30 -O '%s.tmp' '%s')",DATA,DATA,url,DATA,url,DATA,url);remove(DATA ".tmp");rc=DownloadTo(0,url,NULL,DATA ".tmp",30);if(rc!=NET_OK)rc=system(cmd);if(!rc&&access(DATA ".tmp",R_OK)==0)rename(DATA ".tmp",DATA);else rc=-1;if(rc)snprintf(status_text,sizeof status_text,"Obnova selhala. Zkontroluj WiFi.");else if(!load_catalog())snprintf(status_text,sizeof status_text,"Stazeny katalog je poskozeny.");else snprintf(status_text,sizeof status_text,"Katalog byl uspesne obnoven.");}
static int indices(int*out){int i,n=0,start=page*5;for(i=0;i<count;i++)if(!strcmp(ep[i].feed,feed_ids[feed])){if(n>=start&&n<start+5)out[n-start]=i;n++;}return n;}
static void fetch_image(Episode*e){char path[300],url[400],cmd[1400];struct stat st;int rc;if(!e->image[0])return;snprintf(path,sizeof path,ROOT "/images/%s.jpg",e->key);if(!stat(path,&st)&&st.st_size>200)return;remove(path);snprintf(url,sizeof url,"%s%s",BASE,e->image);remove(strcat(strcpy(cmd,path),".tmp"));rc=DownloadTo(0,url,NULL,cmd,15);if(rc!=NET_OK){snprintf(cmd,sizeof cmd,"/bin/busybox wget -q -T 15 -O '%s.tmp' '%s' && mv '%s.tmp' '%s'",path,url,path,path);system(cmd);}else rename(cmd,path);}
static void prepare_page_images(void){int ids[5],total=indices(ids),shown=total-page*5,i;if(shown>5)shown=5;for(i=0;i<shown;i++)fetch_image(&ep[ids[i]]);}
static void draw_home(void){int w=ScreenWidth(),i;txt(40,30,w-80,65,"Helciny podcasty",f_title,ALIGN_LEFT);txt(40,100,w-80,42,"Online poslech pres WiFi",f_small,ALIGN_LEFT);for(i=0;i<3;i++)btn(55,170+i*98,w-110,88,feed_names[i],0);btn(55,480,w-110,88,autoplay?"Dalsi dil: ZAPNUTO":"Dalsi dil: VYPNUTO",autoplay);btn(55,578,w-110,88,"OBNOVIT KATALOG",0);if(status_text[0])txt(55,690,w-110,55,status_text,f_small,ALIGN_CENTER);}
static void draw_list(void){int w=ScreenWidth(),ids[5],total=indices(ids),shown=total-page*5,i;char s[100];if(shown>5)shown=5;txt(35,25,w-70,55,feed_names[feed],f_title,ALIGN_LEFT);snprintf(s,sizeof s,"Epizody %d-%d z %d",page*5+1,page*5+shown,total);txt(38,82,w-76,38,s,f_small,ALIGN_LEFT);for(i=0;i<shown;i++){Episode*e=&ep[ids[i]];int y=130+i*205;char path[300],meta[80];snprintf(path,sizeof path,ROOT "/images/%s.jpg",e->key);ibitmap*bm=LoadJPEG(path,145,145,100,100,1);DrawRect(40,y,145,145,BLACK);if(bm){DrawBitmap(40,y,bm);free(bm);}else remove(path);txt(215,y,w-255,92,e->title,f_body,ALIGN_LEFT|VALIGN_MIDDLE);snprintf(meta,sizeof meta,"%s   %s",e->duration,e->date);txt(215,y+105,w-255,40,meta,f_small,ALIGN_LEFT);DrawLine(40,y+175,w-40,y+175,BLACK);}btn(35,1180,175,80,"ZPET",0);btn(w-420,1180,175,80,"<",0);btn(w-220,1180,175,80,">",0);}
static int valid_mp3(const char*path){unsigned char b[3];FILE*f=fopen(path,"rb");if(!f)return 0;if(fread(b,1,3,f)!=3){fclose(f);return 0;}fclose(f);return !memcmp(b,"ID3",3)||(b[0]==0xff&&(b[1]&0xe0)==0xe0);}
static void open_system_audio(void){char path[300],msg[380],*handler=GetFileHandler(AUDIO_TMP);int err;if(handler&&handler[0]){snprintf(msg,sizeof msg,"05 exec handler=%s",handler);playback_log(msg);if(handler[0]=='/')snprintf(path,sizeof path,"%s",handler);else snprintf(path,sizeof path,"/ebrmain/bin/%s",handler);}else{playback_log("05 no handler, trying play.app");snprintf(path,sizeof path,"/ebrmain/bin/play.app");}execl(path,path,AUDIO_TMP,(char*)NULL);err=errno;execl("/ebrmain/bin/audiobooks.app","audiobooks.app",AUDIO_TMP,(char*)NULL);execl("/ebrmain/bin/play.app","play.app",AUDIO_TMP,(char*)NULL);snprintf(msg,sizeof msg,"06 exec failed handler=%s path=%s errno=%d",handler?handler:"(none)",path,err);playback_log(msg);snprintf(status_text,sizeof status_text,"Handler: %.45s  chyba: %d",handler?handler:"(zadny)",err);repaint();}
static void finish_download(void){int status;char msg[120];struct stat st;pid_t done=waitpid(download_pid,&status,WNOHANG);if(done==0){SetWeakTimer("podcast-download",finish_download,1000);return;}download_pid=-1;if(done<0||!WIFEXITED(status)||WEXITSTATUS(status)||stat(AUDIO_PART,&st)||st.st_size<1024||!valid_mp3(AUDIO_PART)){remove(AUDIO_PART);playback_log("03 download failed");snprintf(status_text,sizeof status_text,"Epizodu se nepodarilo nacist.");repaint();return;}snprintf(msg,sizeof msg,"03 valid mp3 bytes=%ld",(long)st.st_size);playback_log(msg);remove(AUDIO_TMP);if(rename(AUDIO_PART,AUDIO_TMP)){playback_log("04 rename failed");snprintf(status_text,sizeof status_text,"Docasny soubor nelze ulozit.");repaint();return;}playback_log("04 local file ready");current_audio=pending_audio;open_system_audio();}
static void play_index(int idx){char cmd[1900];if(idx<0||idx>=count||download_pid>0)return;FillArea(35,78,ScreenWidth()-70,48,WHITE);txt(38,82,ScreenWidth()-76,40,"Stahuji docasnou kopii epizody...",f_small,ALIGN_LEFT);FullUpdate();remove(DEBUG_LOG);playback_log("01 download start");remove(AUDIO_PART);snprintf(cmd,sizeof cmd,"(/bin/busybox wget -q -T 300 -O '%s' '%s' || /usr/bin/wget -q -T 300 -O '%s' '%s' || wget -q -T 300 -O '%s' '%s') 2>>'%s'",AUDIO_PART,ep[idx].audio,AUDIO_PART,ep[idx].audio,AUDIO_PART,ep[idx].audio,DEBUG_LOG);pending_audio=idx;download_pid=fork();if(download_pid==0){execl("/bin/sh","sh","-c",cmd,(char*)NULL);_exit(127);}if(download_pid<0){playback_log("02 fork failed");snprintf(status_text,sizeof status_text,"Stahovani nelze spustit.");repaint();return;}playback_log("02 download process started");SetWeakTimer("podcast-download",finish_download,1000);}
static void play_episode(int selected){int ids[5],total=indices(ids);if(selected<0||selected>=5||page*5+selected>=total)return;play_index(ids[selected]);}
static void play_next_downloaded(void){int i;if(!autoplay||current_audio<0)return;for(i=current_audio+1;i<count;i++)if(!strcmp(ep[i].feed,ep[current_audio].feed)){play_index(i);return;}}
static void repaint(void){ClearScreen();if(view==0)draw_home();else draw_list();FullUpdate();}
static void touch(int x,int y){int w=ScreenWidth(),i;if(view==0){for(i=0;i<3;i++)if(hit(x,y,55,170+i*98,w-110,88)){feed=i;page=0;view=1;prepare_page_images();repaint();return;}if(hit(x,y,55,480,w-110,88)){autoplay=!autoplay;save_cfg();repaint();}else if(hit(x,y,55,578,w-110,88)){refresh();repaint();}}else{if(y>=130&&y<1155&&x>=40&&x<w-40&&(y-130)%205<175){i=(y-130)/205;if(i<5)play_episode(i);}else if(hit(x,y,35,1165,190,110)){view=0;repaint();}else if(hit(x,y,w-430,1165,190,110)&&page>0){page--;prepare_page_images();repaint();}else if(hit(x,y,w-230,1165,200,110)){int ids[5];if((page+1)*5<indices(ids)){page++;prepare_page_images();repaint();}}}}
static int handler(int type,int p1,int p2){if(type==EVT_INIT){f_title=OpenFont(DEFAULTFONTB,42,1);f_body=OpenFont(DEFAULTFONT,30,1);f_small=OpenFont(DEFAULTFONT,23,1);mkdirs();migrate_image_cache();load_cfg();load_catalog();load_last_log();repaint();}else if(type==EVT_REPAINT)repaint();else if(type==EVT_POINTERUP)touch(p1,p2);else if(type==EVT_MP_STATECHANGED&&p1==MP_TRACK_FINISHED)play_next_downloaded();else if(type==EVT_KEYDOWN&&p1==IV_KEY_BACK){if(view){view=0;repaint();}else CloseApp();}else if(type==EVT_EXIT){CloseFont(f_title);CloseFont(f_body);CloseFont(f_small);}return 0;}
int main(void){InkViewMain(handler);return 0;}
