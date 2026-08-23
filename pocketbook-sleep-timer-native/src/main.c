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
static int custom_mode;
static int custom_minutes = 25;
static int shown_remaining_minutes = -1;
static time_t end_time;

static void draw_screen(void);
static void update_countdown(void);
static void open_timer_menu(void);
static void open_control_menu(void);
static void draw_centered(int y, const char *text);
static void draw_custom_screen(void);
static void draw_countdown_value(int partial);

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
    shown_remaining_minutes = -1;
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

    draw_countdown_value(1);

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
    custom_mode = 0;
    shown_remaining_minutes = -1;
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
    {ITEM_ACTIVE, 201, "Zrusit casovac", NULL},
    {ITEM_ACTIVE, 202, "Zmenit cas", NULL},
    {ITEM_ACTIVE, 203, "Zavrit aplikaci", NULL},
    {0, 0, NULL, NULL}
};

static imenu timer_menu[] = {
    {ITEM_ACTIVE, 101, "5 min", NULL},
    {ITEM_ACTIVE, 102, "10 min", NULL},
    {ITEM_ACTIVE, 103, "15 min", NULL},
    {ITEM_ACTIVE, 104, "20 min", NULL},
    {ITEM_ACTIVE, 105, "30 min", NULL},
    {ITEM_ACTIVE, 106, "45 min", NULL},
    {ITEM_ACTIVE, 107, "60 min", NULL},
    {ITEM_ACTIVE, 108, "Vlastni cas", NULL},
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
    case 8:
    case 108:
        custom_mode = 1;
        draw_custom_screen();
        return;
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
    if (!timer_active && !custom_mode) {
        int sw = ScreenWidth();
        int sh = ScreenHeight();
        menu_opened = 1;
        OpenMenu(timer_menu, 1, sw / 2, sh / 3, timer_menu_handler);
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
        int sw = ScreenWidth();
        int sh = ScreenHeight();
        menu_opened = 1;
        OpenMenu(control_menu, 1, sw / 2, sh / 3, control_menu_handler);
    }
}

static void draw_centered(int y, const char *text)
{
    int sw = ScreenWidth();
    int text_w = StringWidth(text);
    int x = (sw - text_w) / 2;
    if (x < 0) {
        x = 0;
    }
    DrawString(x, y, text);
}

static void clear_rect(int x, int y, int w, int h)
{
    FillArea(x, y, w, h, WHITE);
}

static void draw_countdown_value(int partial)
{
    char label[64];
    int left = remaining_seconds();
    int min = (left + 59) / 60;

    if (partial && min == shown_remaining_minutes) {
        return;
    }
    shown_remaining_minutes = min;

    clear_rect(0, 350, ScreenWidth(), 125);
    if (font_large) {
        SetFont(font_large, BLACK);
    }
    snprintf(label, sizeof(label), "%d min", min);
    draw_centered(370, label);

    if (partial) {
        PartialUpdate(0, 350, ScreenWidth(), 125);
    }
}

static void draw_button(int x, int y, int w, int h, const char *text)
{
    DrawRect(x, y, w, h, BLACK);
    if (font_body) {
        SetFont(font_body, BLACK);
    }
    DrawString(x + (w - StringWidth(text)) / 2, y + (h - 38) / 2, text);
}

static void custom_layout(int *x, int *w, int *y_value, int *y_row1, int *y_row2, int *y_row3)
{
    int sw = ScreenWidth();
    int sh = ScreenHeight();
    *w = sw > 760 ? 560 : sw - 80;
    *x = (sw - *w) / 2;
    *y_value = sh / 4;
    *y_row1 = *y_value + 135;
    *y_row2 = *y_row1 + 95;
    *y_row3 = *y_row2 + 115;
}

static void draw_custom_minutes(int partial)
{
    char label[64];
    int x, w, y_value, y_row1, y_row2, y_row3;
    (void)y_row1;
    (void)y_row2;
    (void)y_row3;
    custom_layout(&x, &w, &y_value, &y_row1, &y_row2, &y_row3);

    (void)x;
    (void)w;
    clear_rect(0, y_value - 10, ScreenWidth(), 110);
    if (font_large) {
        SetFont(font_large, BLACK);
    }
    snprintf(label, sizeof(label), "%d min", custom_minutes);
    draw_centered(y_value, label);

    if (partial) {
        PartialUpdate(0, y_value - 10, ScreenWidth(), 110);
    }
}

static void draw_custom_screen(void)
{
    int x, w, y_value, y_row1, y_row2, y_row3;
    int gap = 18;
    int half;
    custom_layout(&x, &w, &y_value, &y_row1, &y_row2, &y_row3);
    half = (w - gap) / 2;

    ClearScreen();
    if (font_title) {
        SetFont(font_title, BLACK);
    }
    draw_centered(70, "Vlastni cas vypnuti");

    draw_custom_minutes(0);

    draw_button(x, y_row1, half, 72, "-10");
    draw_button(x + half + gap, y_row1, half, 72, "+10");
    draw_button(x, y_row2, half, 72, "-1");
    draw_button(x + half + gap, y_row2, half, 72, "+1");
    draw_button(x, y_row3, w, 82, "Start");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    draw_centered(ScreenHeight() - 70, "Klepnutim mimo tlacitka se vratis.");
    FullUpdate();
}

static int in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

static int handle_custom_touch(int px, int py)
{
    int x, w, y_value, y_row1, y_row2, y_row3;
    int gap = 18;
    int half;
    (void)y_value;
    custom_layout(&x, &w, &y_value, &y_row1, &y_row2, &y_row3);
    half = (w - gap) / 2;

    if (in_rect(px, py, x, y_row1, half, 72)) {
        custom_minutes -= 10;
    } else if (in_rect(px, py, x + half + gap, y_row1, half, 72)) {
        custom_minutes += 10;
    } else if (in_rect(px, py, x, y_row2, half, 72)) {
        custom_minutes -= 1;
    } else if (in_rect(px, py, x + half + gap, y_row2, half, 72)) {
        custom_minutes += 1;
    } else if (in_rect(px, py, x, y_row3, w, 82)) {
        start_timer(custom_minutes);
        return 1;
    } else {
        custom_mode = 0;
        draw_screen();
        return 1;
    }

    if (custom_minutes < 1) {
        custom_minutes = 1;
    } else if (custom_minutes > 999) {
        custom_minutes = 999;
    }
    draw_custom_minutes(1);
    return 1;
}

static void draw_screen(void)
{
    int sw = ScreenWidth();
    char label[64];
    static const int values[] = {5, 10, 15, 20, 30, 45, 60};
    int i, bw = sw > 760 ? 560 : sw - 80, bx = (sw - bw) / 2;

    ClearScreen();

    if (font_title) {
        SetFont(font_title, BLACK);
    }
    draw_centered(70, "Helcin casovac na vypnuti");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    if (timer_active) {
        DrawCircle(sw / 2, 210, 54, BLACK);
        FillArea(sw / 2 + 4, 145, 65, 105, WHITE);
        draw_centered(315, "Zbyva do vypnuti");
        draw_countdown_value(0);

        if (font_body) {
            SetFont(font_body, BLACK);
        }
        snprintf(label, sizeof(label), "Nastaveno: %d min", active_minutes);
        draw_centered(475, label);
        draw_button(bx, 565, bw, 92, "ZMENIT CAS");
        draw_button(bx, 685, bw, 92, "ZRUSIT CASOVAC");

        FullUpdate();
        return;
    }

    draw_centered(145, "Vyber cas vypnuti");
    for (i = 0; i < 7; ++i) {
        snprintf(label, sizeof(label), "%d minut", values[i]);
        draw_button(bx, 220 + i * 105, bw, 82, label);
    }
    draw_button(bx, 965, bw, 88, "VLASTNI CAS");

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
        break;

    case EVT_REPAINT:
        if (custom_mode) {
            draw_custom_screen();
        } else {
            draw_screen();
        }
        break;

    case EVT_POINTERDOWN:
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "touch down: x=%d y=%d timer_active=%d", par1, par2, timer_active);
            write_log(msg);
        }

        if (custom_mode) {
            handle_custom_touch(par1, par2);
            return 0;
        } else if (timer_active) {
            int sw = ScreenWidth();
            int bw = sw > 760 ? 560 : sw - 80;
            int bx = (sw - bw) / 2;
            if (in_rect(par1, par2, bx, 565, bw, 92)) {
                cancel_timer();
                draw_screen();
            } else if (in_rect(par1, par2, bx, 685, bw, 92)) {
                cancel_timer();
                Message(ICON_INFORMATION, "Helcin casovac na vypnuti", "Casovac zrusen.", 1500);
                draw_screen();
            }
            return 0;
        } else {
            static const int values[] = {5, 10, 15, 20, 30, 45, 60};
            int sw = ScreenWidth();
            int bw = sw > 760 ? 560 : sw - 80;
            int bx = (sw - bw) / 2;
            int i;
            for (i = 0; i < 7; ++i) {
                if (in_rect(par1, par2, bx, 220 + i * 105, bw, 82)) {
                    start_timer(values[i]);
                    return 0;
                }
            }
            if (in_rect(par1, par2, bx, 965, bw, 88)) {
                custom_mode = 1;
                draw_custom_screen();
            }
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
