#define _POSIX_C_SOURCE 200809L

#include <inkview.h>

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#define STATE_DIR "/mnt/ext1/system/config"
#define LOG_FILE STATE_DIR "/meleys-sleep-timer.log"
#define TIMER_NAME "meleys-sleep-timer"
#define REFRESH_TIMER_NAME "meleys-sleep-timer-refresh"

static ifont *font_title;
static ifont *font_body;
static ifont *font_large;
static int active_minutes;
static int timer_active;
static int menu_opened;
static time_t end_time;

static void draw_screen(void);
static void update_countdown(void);
static void open_timer_menu(void);
static void open_control_menu(void);

static void write_log(const char *message)
{
    mkdir(STATE_DIR, 0777);

    FILE *f = fopen(LOG_FILE, "a");
    if (!f) {
        return;
    }

    time_t now = time(NULL);
    fprintf(f, "%ld %s\n", (long)now, message);
    fclose(f);
}

static void poweroff_timer(void)
{
    write_log("native timer expired, calling PocketBook PowerOff()");
    ClearTimer(poweroff_timer);
    ClearTimerByName(REFRESH_TIMER_NAME);
    timer_active = 0;

    PowerOff();

    write_log("PowerOff() returned, forcing sleep fallback");
    ForcingSleep();
    GoSleep(0, 1);
}

static void cancel_timer(void)
{
    ClearTimer(poweroff_timer);
    ClearTimer(update_countdown);
    ClearTimerByName(TIMER_NAME);
    ClearTimerByName(REFRESH_TIMER_NAME);
    timer_active = 0;
    active_minutes = 0;
    end_time = 0;
    write_log("native timer cancelled");
}

static int remaining_seconds(void)
{
    time_t now = time(NULL);
    int seconds = (int)(end_time - now);
    return seconds > 0 ? seconds : 0;
}

static void update_countdown(void)
{
    if (!timer_active) {
        return;
    }

    draw_screen();

    if (remaining_seconds() > 0) {
        SetWeakTimer(REFRESH_TIMER_NAME, update_countdown, 60000);
    }
}

static void start_timer(int minutes)
{
    char msg[128];

    cancel_timer();
    active_minutes = minutes;
    timer_active = 1;
    end_time = time(NULL) + minutes * 60;
    SetHardTimer(TIMER_NAME, poweroff_timer, minutes * 60 * 1000);
    SetWeakTimer(REFRESH_TIMER_NAME, update_countdown, 60000);
    SetAutoPowerOff(1);
    BanSleep(minutes * 60 + 30);
    PostponeTimedPoweroff();

    snprintf(msg, sizeof(msg), "native timer started: %d minutes", minutes);
    write_log(msg);

    snprintf(msg, sizeof(msg), "Casovac nastaven na %d minut.", minutes);
    Message(ICON_INFORMATION, "Helcin casovac na vypnuti", msg, 1500);
    draw_screen();
}

static imenu control_menu[] = {
    {ITEM_HEADER, 0, "Casovac bezi", NULL},
    {ITEM_ACTIVE, 201, "Zrusit casovac", NULL},
    {ITEM_ACTIVE, 202, "Zmenit cas", NULL},
    {ITEM_ACTIVE, 203, "Zavrit aplikaci", NULL},
    {0, 0, NULL, NULL}
};

static imenu timer_menu[] = {
    {ITEM_HEADER, 0, "Vyber cas vypnuti", NULL},
    {ITEM_ACTIVE, 101, "5 min", NULL},
    {ITEM_ACTIVE, 102, "10 min", NULL},
    {ITEM_ACTIVE, 103, "15 min", NULL},
    {ITEM_ACTIVE, 104, "20 min", NULL},
    {ITEM_ACTIVE, 105, "30 min", NULL},
    {ITEM_ACTIVE, 106, "45 min", NULL},
    {ITEM_ACTIVE, 107, "60 min", NULL},
    {0, 0, NULL, NULL}
};

static void timer_menu_handler(int index)
{
    int minutes = 0;
    menu_opened = 0;

    switch (index) {
    case 1:
    case 101:
        minutes = 5;
        break;
    case 2:
    case 102:
        minutes = 10;
        break;
    case 3:
    case 103:
        minutes = 15;
        break;
    case 4:
    case 104:
        minutes = 20;
        break;
    case 5:
    case 105:
        minutes = 30;
        break;
    case 6:
    case 106:
        minutes = 45;
        break;
    case 7:
    case 107:
        minutes = 60;
        break;
    default:
        break;
    }

    if (minutes > 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "menu selected: index %d, %d minutes", index, minutes);
        write_log(msg);
        start_timer(minutes);
    } else {
        write_log("timer menu closed without selection");
    }
}

static void open_timer_menu(void)
{
    if (!timer_active) {
        menu_opened = 1;
        OpenMenu(timer_menu, 1, ScreenWidth() / 12, 120, timer_menu_handler);
    }
}

static void control_menu_handler(int index)
{
    menu_opened = 0;

    switch (index) {
    case 1:
    case 201:
        cancel_timer();
        Message(ICON_INFORMATION, "Helcin casovac na vypnuti", "Casovac zrusen.", 1500);
        draw_screen();
        break;
    case 2:
    case 202:
        cancel_timer();
        draw_screen();
        open_timer_menu();
        break;
    case 3:
    case 203:
        CloseApp();
        break;
    default:
        break;
    }
}

static void open_control_menu(void)
{
    if (timer_active) {
        menu_opened = 1;
        OpenMenu(control_menu, 1, ScreenWidth() / 12, 120, control_menu_handler);
    }
}

static void draw_screen(void)
{
    int sw = ScreenWidth();
    int margin = sw / 14;
    char label[64];

    ClearScreen();

    if (font_title) {
        SetFont(font_title, BLACK);
    }
    DrawString(margin, 58, "Helcin casovac na vypnuti");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    if (timer_active) {
        int left = remaining_seconds();
        int min = (left + 59) / 60;

        DrawString(margin, 140, "Zbyva do vypnuti");

        if (font_large) {
            SetFont(font_large, BLACK);
        }
        snprintf(label, sizeof(label), "%d min", min);
        DrawString(margin, 220, label);

        if (font_body) {
            SetFont(font_body, BLACK);
        }
        snprintf(label, sizeof(label), "Nastaveno: %d min", active_minutes);
        DrawString(margin, 340, label);
        DrawString(margin, 420, "Klepnutim otevres volby.");

        FullUpdate();
        return;
    }

    DrawString(margin, 145, "Vyber delku casovace");
    DrawString(margin, 215, "v systemovem menu.");
    DrawString(margin, 315, "Klepnutim otevres");
    DrawString(margin, 365, "nabidku casu.");

    FullUpdate();
}

static int main_handler(int type, int par1, int par2)
{
    switch (type) {
    case EVT_INIT:
        OpenScreen();
        font_title = OpenFont(DEFAULTFONTB, 42, 1);
        font_body = OpenFont(DEFAULTFONT, 34, 1);
        font_large = OpenFont(DEFAULTFONTB, 78, 1);
        draw_screen();
        open_timer_menu();
        break;

    case EVT_REPAINT:
        draw_screen();
        if (!timer_active && !menu_opened) {
            open_timer_menu();
        }
        break;

    case EVT_POINTERDOWN:
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "touch down: x=%d y=%d timer_active=%d", par1, par2, timer_active);
            write_log(msg);
        }

        if (timer_active) {
            open_control_menu();
            return 0;
        } else {
            open_timer_menu();
            return 0;
        }
        break;

    case EVT_EXIT:
        if (font_title) {
            CloseFont(font_title);
            font_title = NULL;
        }
        if (font_body) {
            CloseFont(font_body);
            font_body = NULL;
        }
        if (font_large) {
            CloseFont(font_large);
            font_large = NULL;
        }
        break;
    }

    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    InkViewMain(main_handler);
    return 0;
}
