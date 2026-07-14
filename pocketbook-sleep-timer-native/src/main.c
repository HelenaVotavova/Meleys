#define _POSIX_C_SOURCE 200809L

#include <inkview.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define STATE_DIR "/mnt/ext1/system/config"
#define PID_FILE STATE_DIR "/meleys-sleep-timer.pid"
#define LOG_FILE STATE_DIR "/meleys-sleep-timer.log"

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
static int active_minutes;

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

static void save_pid(pid_t pid)
{
    mkdir(STATE_DIR, 0777);

    FILE *f = fopen(PID_FILE, "w");
    if (!f) {
        return;
    }

    fprintf(f, "%ld\n", (long)pid);
    fclose(f);
}

static pid_t read_pid(void)
{
    FILE *f = fopen(PID_FILE, "r");
    long pid = 0;

    if (!f) {
        return 0;
    }

    if (fscanf(f, "%ld", &pid) != 1) {
        pid = 0;
    }
    fclose(f);

    return (pid_t)pid;
}

static void cancel_timer(void)
{
    pid_t pid = read_pid();
    if (pid > 1) {
        kill(pid, SIGTERM);
    }
    unlink(PID_FILE);
    write_log("timer cancelled");
}

static void try_poweroff(void)
{
    const char *commands[] = {
        "/sbin/poweroff",
        "/bin/poweroff",
        "poweroff",
        "/sbin/shutdown -h now",
        "shutdown -h now",
        NULL
    };

    for (int i = 0; commands[i]; ++i) {
        int rc = system(commands[i]);
        if (rc == 0) {
            _exit(0);
        }
    }

    write_log("poweroff failed");
}

static void timer_child(int minutes)
{
    char msg[96];

    setsid();
    snprintf(msg, sizeof(msg), "timer child sleeping for %d minutes", minutes);
    write_log(msg);

    sleep((unsigned int)minutes * 60U);
    unlink(PID_FILE);
    write_log("timer expired");
    try_poweroff();
    _exit(1);
}

static void start_timer(int minutes)
{
    char msg[128];
    pid_t pid;

    cancel_timer();

    pid = fork();
    if (pid < 0) {
        snprintf(msg, sizeof(msg), "fork failed: %d", errno);
        write_log(msg);
        Message(ICON_ERROR, "Meleys Sleep Timer", "Casovac se nepodarilo spustit.", 3000);
        return;
    }

    if (pid == 0) {
        timer_child(minutes);
    }

    save_pid(pid);
    snprintf(msg, sizeof(msg), "timer started: %d minutes, pid %ld", minutes, (long)pid);
    write_log(msg);

    snprintf(msg, sizeof(msg), "Casovac nastaven na %d minut.", minutes);
    Message(ICON_INFORMATION, "Meleys Sleep Timer", msg, 2000);
    CloseApp();
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
    int gap = 18;
    int cols = 2;
    int bw = (sw - 2 * margin - gap) / cols;
    int bh = 74;
    int start_y = 170;
    char label[32];

    ClearScreen();

    if (font_title) {
        SetFont(font_title, BLACK);
    }
    DrawString(margin, 58, "Meleys Sleep Timer");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    DrawString(margin, 112, "Vyber cas, po kterem se ctecka vypne.");

    for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); ++i) {
        int row = i / cols;
        int col = i % cols;
        buttons[i].minutes = presets[i];
        buttons[i].x = margin + col * (bw + gap);
        buttons[i].y = start_y + row * (bh + gap);
        buttons[i].w = bw;
        buttons[i].h = bh;

        snprintf(label, sizeof(label), "%d min", presets[i]);
        draw_button(&buttons[i], label);
    }

    cancel_button.minutes = 0;
    cancel_button.x = margin;
    cancel_button.y = sh - 110;
    cancel_button.w = sw - 2 * margin;
    cancel_button.h = 66;
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
        draw_screen();
        break;

    case EVT_REPAINT:
        draw_screen();
        break;

    case EVT_POINTERUP:
    case EVT_TOUCHUP:
        for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); ++i) {
            if (in_button(&buttons[i], par1, par2)) {
                active_minutes = buttons[i].minutes;
                start_timer(active_minutes);
                return 0;
            }
        }

        if (in_button(&cancel_button, par1, par2)) {
            cancel_timer();
            Message(ICON_INFORMATION, "Meleys Sleep Timer", "Casovac zrusen.", 2000);
            CloseApp();
            return 0;
        }
        break;

    case EVT_KEYDOWN:
        if (par1 == KEY_BACK || par1 == KEY_POWER) {
            CloseApp();
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
