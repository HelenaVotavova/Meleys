#ifndef TEST_INKVIEW_H
#define TEST_INKVIEW_H
/* Host-only UI substitutes. Device builds continue to use the real SDK. */
typedef struct { int unused; } ifont;
typedef struct { int unused; } ibitmap;
typedef struct { char *title, *author; } bookinfo;
typedef struct imenu_s { int type, index; char *text; struct imenu_s *submenu; } imenu;
enum { BLACK, WHITE, ALIGN_LEFT=1, ALIGN_CENTER=2, ALIGN_RIGHT=4, VALIGN_MIDDLE=8,
       DOTS=16, ICON_WARNING=1, ICON_INFORMATION=2, ITEM_ACTIVE=1,
       EVT_INIT=1, EVT_REPAINT, EVT_POINTERDOWN, EVT_POINTERUP, EVT_KEYDOWN, EVT_EXIT,
       IV_KEY_BACK=1, IV_KEY_UP, IV_KEY_DOWN, IV_KEY_LEFT, IV_KEY_RIGHT, IV_KEY_MENU,
       NET_OK=0, NET_CONNECTED=1, NET_ENETWORK=2 };
#define DEFAULTFONT "normal"
#define DEFAULTFONTB "bold"
static int updates, update_rects[8][4], warnings;
static void PartialUpdate(int x,int y,int w,int h) {
    if(updates<8){update_rects[updates][0]=x;update_rects[updates][1]=y;update_rects[updates][2]=w;update_rects[updates][3]=h;}updates++;
}
#define ScreenWidth() 1072
#define ScreenHeight() 1448
#define OpenFont(...) ((ifont*)0)
#define LoadJPEG(...) ((ibitmap*)0)
#define GetBookInfo(...) ((bookinfo*)0)
#define StringWidth(...) 100
#define QueryNetwork() NET_CONNECTED
#define NetConnect(...) NET_OK
#define DownloadTo(...) NET_ENETWORK
#define Message(...) (++warnings)
#define SetFont(...) ((void)0)
#define DrawTextRect(...) ((void)0)
#define DrawString(...) ((void)0)
#define DrawLine(...) ((void)0)
#define DrawRect(...) ((void)0)
#define FillArea(...) ((void)0)
#define DrawCircle(...) ((void)0)
#define DrawBitmap(...) ((void)0)
#define ClearScreen(...) ((void)0)
#define FullUpdate(...) ((void)0)
#define OpenScreen(...) ((void)0)
#define CloseFont(...) ((void)0)
#define OpenMenu(...) ((void)0)
#define OpenKeyboard(...) ((void)0)
#define CloseApp(...) ((void)0)
#define InkViewMain(...) ((void)0)
#define ClearTimer(...) ((void)0)
#define ClearTimerByName(...) ((void)0)
#define SetWeakTimer(...) ((void)0)
#define SetHardTimer(...) ((void)0)
#define SetAutoPowerOff(...) ((void)0)
#define BanSleep(...) ((void)0)
#define PostponeTimedPoweroff(...) ((void)0)
#define PowerOff(...) ((void)0)
#define ForcingSleep(...) ((void)0)
#define GoSleep(...) ((void)0)
#define FlushEvents(...) ((void)0)
#define iv_sleepmode(...) ((void)0)
#endif
