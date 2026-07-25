#define _POSIX_C_SOURCE 200809L

#include <inkview.h>

#include <stdio.h>
#include <string.h>

#define MAP_W 9
#define MAP_H 8
#define MAX_UNDO 256

enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
};

typedef struct {
    char cells[MAP_H][MAP_W + 1];
    int px;
    int py;
    int moves;
    int pushes;
} state_t;

typedef struct {
    short type;
    short index;
    char *text;
    struct imenu_s *submenu;
} menu_item_t;

static const char *levels[][MAP_H] = {
    {
        "#########",
        "#   .   #",
        "#   $   #",
        "#   @   #",
        "#       #",
        "#       #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "#  . .  #",
        "#  $$   #",
        "#   @   #",
        "#       #",
        "#       #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# .   . #",
        "# $$    #",
        "#  #    #",
        "#   @   #",
        "#       #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "#   ..  #",
        "#  #$   #",
        "#   $   #",
        "#   @   #",
        "#       #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# . . . #",
        "# $$$   #",
        "#   #   #",
        "#   @   #",
        "#       #",
        "#       #",
        "#########"
    }
};

static const int level_count = sizeof(levels) / sizeof(levels[0]);
static int level_index;
static state_t game;
static state_t undo_stack[MAX_UNDO];
static int undo_len;
static ifont *font_title;
static ifont *font_body;
static ifont *font_small;

static void draw_game(void);

static int is_player(char c) { return c == '@' || c == '+'; }
static int is_box(char c) { return c == '$' || c == '*'; }
static int is_goal(char c) { return c == '.' || c == '*' || c == '+'; }
static int is_wall(char c) { return c == '#'; }

static char floor_of(char c)
{
    return (c == '+' || c == '*') ? '.' : ' ';
}

static char with_player(char floor)
{
    return floor == '.' ? '+' : '@';
}

static char with_box(char floor)
{
    return floor == '.' ? '*' : '$';
}

static void push_undo(void)
{
    if (undo_len >= MAX_UNDO) {
        memmove(undo_stack, undo_stack + 1, sizeof(undo_stack[0]) * (MAX_UNDO - 1));
        undo_len = MAX_UNDO - 1;
    }
    undo_stack[undo_len++] = game;
}

static void load_level(int idx)
{
    if (idx < 0) {
        idx = level_count - 1;
    }
    if (idx >= level_count) {
        idx = 0;
    }
    level_index = idx;
    memset(&game, 0, sizeof(game));
    for (int y = 0; y < MAP_H; ++y) {
        strncpy(game.cells[y], levels[level_index][y], MAP_W);
        game.cells[y][MAP_W] = 0;
        for (int x = 0; x < MAP_W; ++x) {
            if (is_player(game.cells[y][x])) {
                game.px = x;
                game.py = y;
            }
        }
    }
    undo_len = 0;
    draw_game();
}

static int completed(void)
{
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            if (game.cells[y][x] == '$') {
                return 0;
            }
        }
    }
    return 1;
}

static void move_player(int dir)
{
    int dx = 0, dy = 0;
    if (dir == DIR_UP) dy = -1;
    if (dir == DIR_DOWN) dy = 1;
    if (dir == DIR_LEFT) dx = -1;
    if (dir == DIR_RIGHT) dx = 1;

    int x1 = game.px + dx;
    int y1 = game.py + dy;
    int x2 = game.px + 2 * dx;
    int y2 = game.py + 2 * dy;
    char c1 = game.cells[y1][x1];
    char c2 = game.cells[y2][x2];

    if (is_wall(c1)) {
        return;
    }

    if (is_box(c1)) {
        if (is_wall(c2) || is_box(c2)) {
            return;
        }
        push_undo();
        game.cells[y2][x2] = with_box(c2);
        game.cells[y1][x1] = with_player(floor_of(c1));
        game.cells[game.py][game.px] = floor_of(game.cells[game.py][game.px]);
        game.px = x1;
        game.py = y1;
        game.moves++;
        game.pushes++;
    } else {
        push_undo();
        game.cells[y1][x1] = with_player(c1);
        game.cells[game.py][game.px] = floor_of(game.cells[game.py][game.px]);
        game.px = x1;
        game.py = y1;
        game.moves++;
    }

    draw_game();
    if (completed()) {
        Message(ICON_INFORMATION, "Helcin Sokoban", "Level hotovy.", 1200);
        load_level(level_index + 1);
    }
}

static void undo_move(void)
{
    if (undo_len <= 0) {
        return;
    }
    game = undo_stack[--undo_len];
    draw_game();
}

static void draw_centered(int y, const char *text)
{
    int x = (ScreenWidth() - StringWidth(text)) / 2;
    if (x < 0) {
        x = 0;
    }
    DrawString(x, y, text);
}

static void fill_rect(int x, int y, int w, int h)
{
    for (int i = 0; i < h; ++i) {
        DrawLine(x, y + i, x + w, y + i, BLACK);
    }
}

static void draw_tile(int x, int y, int size, char c)
{
    DrawRect(x, y, size, size, BLACK);
    if (is_wall(c)) {
        fill_rect(x + 2, y + 2, size - 4, size - 4);
        return;
    }
    if (is_goal(c)) {
        DrawCircle(x + size / 2, y + size / 2, size / 6, BLACK);
    }
    if (is_box(c)) {
        DrawRect(x + size / 5, y + size / 5, size * 3 / 5, size * 3 / 5, BLACK);
        if (c == '*') {
            DrawLine(x + size / 5, y + size / 5, x + size * 4 / 5, y + size * 4 / 5, BLACK);
            DrawLine(x + size * 4 / 5, y + size / 5, x + size / 5, y + size * 4 / 5, BLACK);
        }
    }
    if (is_player(c)) {
        DrawCircle(x + size / 2, y + size / 2, size / 4, BLACK);
        DrawLine(x + size / 2, y + size / 3, x + size / 2, y + size * 2 / 3, BLACK);
    }
}

static void draw_game(void)
{
    int sw = ScreenWidth();
    int sh = ScreenHeight();
    int usable_w = sw - 80;
    int usable_h = sh - 250;
    int tile = usable_w / MAP_W;
    if (tile > usable_h / MAP_H) {
        tile = usable_h / MAP_H;
    }
    if (tile > 92) {
        tile = 92;
    }
    int board_w = tile * MAP_W;
    int bx = (sw - board_w) / 2;
    int by = 150;
    char line[96];

    ClearScreen();
    if (font_title) {
        SetFont(font_title, BLACK);
    }
    draw_centered(45, "Helcin Sokoban");

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    snprintf(line, sizeof(line), "Level %d/%d   Tahy %d   Tlaky %d", level_index + 1, level_count, game.moves, game.pushes);
    draw_centered(100, line);

    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            draw_tile(bx + x * tile, by + y * tile, tile, game.cells[y][x]);
        }
    }

    if (font_small) {
        SetFont(font_small, BLACK);
    }
    draw_centered(sh - 82, "Klepni kolem hrace pro pohyb. Nahore menu.");
    FullUpdate();
}

static imenu game_menu[] = {
    {ITEM_ACTIVE, 101, "Undo", NULL},
    {ITEM_ACTIVE, 102, "Restart levelu", NULL},
    {ITEM_ACTIVE, 103, "Dalsi level", NULL},
    {ITEM_ACTIVE, 104, "Predchozi level", NULL},
    {ITEM_ACTIVE, 105, "Zavrit", NULL},
    {0, 0, NULL, NULL}
};

static void game_menu_handler(int index)
{
    switch (index) {
    case 1:
    case 101:
        undo_move();
        break;
    case 2:
    case 102:
        load_level(level_index);
        break;
    case 3:
    case 103:
        load_level(level_index + 1);
        break;
    case 4:
    case 104:
        load_level(level_index - 1);
        break;
    case 5:
    case 105:
        CloseApp();
        break;
    default:
        break;
    }
}

static void open_game_menu(void)
{
    OpenMenu(game_menu, 1, ScreenWidth() / 4, ScreenHeight() / 4, game_menu_handler);
}

static void handle_touch(int x, int y)
{
    if (y < 130) {
        open_game_menu();
        return;
    }

    int sw = ScreenWidth();
    int sh = ScreenHeight();
    int usable_w = sw - 80;
    int usable_h = sh - 250;
    int tile = usable_w / MAP_W;
    if (tile > usable_h / MAP_H) {
        tile = usable_h / MAP_H;
    }
    if (tile > 92) {
        tile = 92;
    }
    int board_w = tile * MAP_W;
    int bx = (sw - board_w) / 2;
    int by = 150;
    int cx = bx + game.px * tile + tile / 2;
    int cy = by + game.py * tile + tile / 2;
    int adx = x > cx ? x - cx : cx - x;
    int ady = y > cy ? y - cy : cy - y;

    if (adx > ady) {
        move_player(x > cx ? DIR_RIGHT : DIR_LEFT);
    } else {
        move_player(y > cy ? DIR_DOWN : DIR_UP);
    }
}

static int main_handler(int type, int par1, int par2)
{
    switch (type) {
    case EVT_INIT:
        OpenScreen();
        font_title = OpenFont(DEFAULTFONTB, 44, 1);
        font_body = OpenFont(DEFAULTFONT, 30, 1);
        font_small = OpenFont(DEFAULTFONT, 24, 1);
        load_level(0);
        break;
    case EVT_REPAINT:
        draw_game();
        break;
    case EVT_POINTERDOWN:
        handle_touch(par1, par2);
        break;
    case EVT_KEYDOWN:
        if (par1 == IV_KEY_UP) move_player(DIR_UP);
        else if (par1 == IV_KEY_DOWN) move_player(DIR_DOWN);
        else if (par1 == IV_KEY_LEFT) move_player(DIR_LEFT);
        else if (par1 == IV_KEY_RIGHT) move_player(DIR_RIGHT);
        else if (par1 == IV_KEY_BACK) undo_move();
        else if (par1 == IV_KEY_MENU) open_game_menu();
        break;
    case EVT_EXIT:
        if (font_title) CloseFont(font_title);
        if (font_body) CloseFont(font_body);
        if (font_small) CloseFont(font_small);
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
