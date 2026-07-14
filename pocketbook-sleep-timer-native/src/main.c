#define _POSIX_C_SOURCE 200809L

#include <inkview.h>

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#define STATE_DIR "/mnt/ext1/system/config"
#define LOG_FILE STATE_DIR "/meleys-sleep-timer.log"
#define TIMER_NAME "meleys-sleep-timer"
#define REFRESH_TIMER_NAME "meleys-sleep-timer-refresh"

typedef struct {
    int minutes;
    int x;
    int y;
    int w;
    int h;
} button_t;

static const int presets[] = {5, 10, 15, 20, 30, 45, 60};
static button_t buttons[sizeof(presets) / sizeof(presets[0])];
static button_t cancel_button;
static ifont *font_title;
static ifont *font_body;
static ifont *font_large;
static int active_minutes;
static int timer_active;
static time_t end_time;

static void draw_screen(void);
static void update_countdown(void);

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

static int in_button(const button_t *button, int x, int y)
{
    return x >= button->x && x <= button->x + button->w &&
           y >= button->y && y <= button->y + button->h;
}

static void draw_button(const button_t *button, const char *label)
{
    int text_w;
    int text_x;
    int text_y;

    DrawRect(button->x, button->y, button->w, button->h, BLACK);
    text_w = StringWidth(label);
    text_x = button->x + (button->w - text_w) / 2;
    text_y = button->y + button->h / 2 - 12;
    DrawString(text_x, text_y, label);
}

static void draw_screen(void)
{
    int sw = ScreenWidth();
    int sh = ScreenHeight();
    int margin = sw / 12;
    int gap = 12;
    int count = (int)(sizeof(presets) / sizeof(presets[0]));
    int bw = sw - 2 * margin;
    int start_y = 155;
    int available = sh - start_y - 125;
    int bh = (available - gap * (count - 1)) / count;
    char label[64];

    ClearScreen();

    if (font_title) {
        SetFont(font_title, BLACK);
    }
    DrawString(margin, 58, "Helcin casovac na vypnuti");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    if (bh > 72) {
        bh = 72;
    }
    if (bh < 44) {
        bh = 44;
    }

    cancel_button.minutes = 0;
    cancel_button.x = margin;
    cancel_button.y = sh - 110;
    cancel_button.w = sw - 2 * margin;
    cancel_button.h = 66;

    if (timer_active) {
        int left = remaining_seconds();
        int min = (left + 59) / 60;

        DrawString(margin, 112, "Zbyva do vypnuti:");

        if (font_large) {
            SetFont(font_large, BLACK);
        }
        snprintf(label, sizeof(label), "%d min", min);
        DrawString(margin, 185, label);

        if (font_body) {
            SetFont(font_body, BLACK);
        }
        snprintf(label, sizeof(label), "Nastaveno: %d min", active_minutes);
        DrawString(margin, 285, label);

        draw_button(&cancel_button, "Cancel");
        FullUpdate();
        return;
    }

    DrawString(margin, 112, "Vyber cas, po kterem se ctecka vypne.");

    for (int i = 0; i < count; ++i) {
        buttons[i].minutes = presets[i];
        buttons[i].x = margin;
        buttons[i].y = start_y + i * (bh + gap);
        buttons[i].w = bw;
        buttons[i].h = bh;

        snprintf(label, sizeof(label), "%d min", presets[i]);
        draw_button(&buttons[i], label);
    }

    draw_button(&cancel_button, "Cancel");

    FullUpdate();
}

static int main_handler(int type, int par1, int par2)
{
    switch (type) {
    case EVT_INIT:
        OpenScreen();
        font_title = OpenFont(DEFAULTFONTB, 34, 1);
        font_body = OpenFont(DEFAULTFONT, 24, 1);
        font_large = OpenFont(DEFAULTFONTB, 58, 1);
        draw_screen();
        break;

    case EVT_REPAINT:
        draw_screen();
        break;

    case EVT_POINTERDOWN:
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "touch down: x=%d y=%d timer_active=%d", par1, par2, timer_active);
            write_log(msg);
        }

        if (in_button(&cancel_button, par1, par2)) {
            cancel_timer();
            Message(ICON_INFORMATION, "Helcin casovac na vypnuti", "Casovac zrusen.", 2000);
            CloseApp();
            return 0;
        }

        if (!timer_active) {
            for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); ++i) {
                if (in_button(&buttons[i], par1, par2)) {
                    char msg[96];
                    snprintf(msg, sizeof(msg), "button selected: index %d, %d minutes, y=%d range=%d-%d",
                             i, buttons[i].minutes, par2, buttons[i].y, buttons[i].y + buttons[i].h);
                    write_log(msg);
                    active_minutes = buttons[i].minutes;
                    start_timer(active_minutes);
                    return 0;
                }
            }
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
