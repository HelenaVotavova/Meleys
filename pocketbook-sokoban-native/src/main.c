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
    int dir;
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
        "#. ## .##",
        "# #     #",
        "#$    $ #",
        "#      ##",
        "# $#.   #",
        "##@ #   #",
        "#########"
    },
    {
        "#########",
        "# .     #",
        "# $  #  #",
        "#    .  #",
        "### # $ #",
        "#@$    .#",
        "# # # # #",
        "#########"
    },
    {
        "#########",
        "#  # .  #",
        "# #     #",
        "#   $.  #",
        "#   . ###",
        "# $ #$  #",
        "#   #@# #",
        "#########"
    },
    {
        "#########",
        "#    .  #",
        "#  #   ##",
        "#   # # #",
        "#  .  . #",
        "# $ $ $ #",
        "##@#    #",
        "#########"
    },
    {
        "#########",
        "#     # #",
        "#  $ .  #",
        "# #@$   #",
        "# ###  .#",
        "# $ .   #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "#   #   #",
        "#$  # # #",
        "#  #@$  #",
        "#.. $.#.#",
        "##     $#",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# .    ##",
        "# $$.   #",
        "#   ##  #",
        "#.     ##",
        "#     $@#",
        "#    #  #",
        "#########"
    },
    {
        "#########",
        "##  #  ##",
        "# $.    #",
        "#  .    #",
        "##  $ $ #",
        "# ##  @##",
        "#  .   ##",
        "#########"
    },
    {
        "#########",
        "#      .#",
        "#.#   $ #",
        "#  .  # #",
        "# @$   ##",
        "# $   # #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# #   $.#",
        "#@$     #",
        "##  #.. #",
        "#   .#  #",
        "#    $$ #",
        "## #    #",
        "#########"
    },
    {
        "#########",
        "##   #  #",
        "#  #$$  #",
        "#     $ #",
        "#   . @ #",
        "#  ##.  #",
        "#   ##. #",
        "#########"
    },
    {
        "#########",
        "## # @  #",
        "#   #$$##",
        "#      ##",
        "#  #   .#",
        "#  $ . .#",
        "#   #   #",
        "#########"
    },
    {
        "#########",
        "## ###  #",
        "#  .    #",
        "#   #  .#",
        "#  #    #",
        "# $  $$##",
        "#  .# @##",
        "#########"
    },
    {
        "#########",
        "# .#   ##",
        "## # $  #",
        "#  $. $ #",
        "#     @##",
        "# #.    #",
        "#   #   #",
        "#########"
    },
    {
        "#########",
        "#   .#@##",
        "#  . .$ #",
        "# #$    #",
        "# #  #  #",
        "# $  #  #",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# #.    #",
        "#@$   $##",
        "##  #   #",
        "#       #",
        "#  $.  ##",
        "#.     ##",
        "#########"
    },
    {
        "#########",
        "#  .  # #",
        "# .   # #",
        "# $.  $ #",
        "# @#    #",
        "#  $  # #",
        "##     ##",
        "#########"
    },
    {
        "#########",
        "# .     #",
        "##    ###",
        "#.   $@##",
        "# $   # #",
        "#    $  #",
        "# .     #",
        "#########"
    },
    {
        "#########",
        "#      ##",
        "##  $#$ #",
        "#  $....#",
        "#   # ###",
        "# #   $@#",
        "# #   # #",
        "#########"
    },
    {
        "#########",
        "##  #  @#",
        "#  $  #$#",
        "# $  # .#",
        "#   # $.#",
        "#.  #   #",
        "#.      #",
        "#########"
    },
    {
        "#########",
        "#      ##",
        "#   $$$.#",
        "##$.#.  #",
        "# @#   ##",
        "###    .#",
        "#       #",
        "#########"
    },
    {
        "#########",
        "# # #   #",
        "# ..#  ##",
        "#  #    #",
        "# $#$ $ #",
        "# @$ . .#",
        "#       #",
        "#########"
    },
    {
        "#########",
        "#    #. #",
        "#      .#",
        "# @# $ ##",
        "##$     #",
        "#   $## #",
        "#  .    #",
        "#########"
    },
    {
        "#########",
        "# #@ ## #",
        "#.#$$   #",
        "#    #  #",
        "#       #",
        "# $##   #",
        "# ..$ . #",
        "#########"
    },
    {
        "#########",
        "#    @  #",
        "#  $ $ ##",
        "#  #    #",
        "#.$    .#",
        "#   #   #",
        "#    # .#",
        "#########"
    }
};static const int level_count = sizeof(levels) / sizeof(levels[0]);
static int level_index;
static state_t game;
static state_t undo_stack[MAX_UNDO];
static int undo_len;
static ifont *font_title;
static ifont *font_body;
static ifont *font_small;

static void draw_game(void);
static void redraw_after_move(int old_px, int old_py, int new_px, int new_py, int box_x, int box_y);

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
    game.dir = DIR_DOWN;
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
    int old_px = game.px;
    int old_py = game.py;
    int box_moved = 0;
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
        game.dir = dir;
        game.cells[y2][x2] = with_box(c2);
        game.cells[y1][x1] = with_player(floor_of(c1));
        game.cells[game.py][game.px] = floor_of(game.cells[game.py][game.px]);
        game.px = x1;
        game.py = y1;
        game.moves++;
        game.pushes++;
        box_moved = 1;
    } else {
        push_undo();
        game.dir = dir;
        game.cells[y1][x1] = with_player(c1);
        game.cells[game.py][game.px] = floor_of(game.cells[game.py][game.px]);
        game.px = x1;
        game.py = y1;
        game.moves++;
    }

    redraw_after_move(old_px, old_py, game.px, game.py, box_moved ? x2 : -1, box_moved ? y2 : -1);
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

static void draw_player(int x, int y, int size)
{
    int cx = x + size / 2;
    int cy = y + size / 2;
    int hx = cx;
    int hy = cy;
    int shoulder_x1;
    int shoulder_y1;
    int shoulder_x2;
    int shoulder_y2;

    if (game.dir == DIR_UP) {
        hy = y + size / 3;
        shoulder_x1 = cx - size / 4;
        shoulder_y1 = cy + size / 10;
        shoulder_x2 = cx + size / 4;
        shoulder_y2 = cy + size / 10;
        DrawLine(cx - size / 10, cy + size / 4, cx - size / 5, cy + size / 2, BLACK);
        DrawLine(cx + size / 10, cy + size / 4, cx + size / 5, cy + size / 2, BLACK);
    } else if (game.dir == DIR_DOWN) {
        hy = y + size * 2 / 3;
        shoulder_x1 = cx - size / 4;
        shoulder_y1 = cy - size / 10;
        shoulder_x2 = cx + size / 4;
        shoulder_y2 = cy - size / 10;
        DrawLine(cx - size / 10, cy - size / 4, cx - size / 5, y + size / 8, BLACK);
        DrawLine(cx + size / 10, cy - size / 4, cx + size / 5, y + size / 8, BLACK);
    } else if (game.dir == DIR_LEFT) {
        hx = x + size / 3;
        shoulder_x1 = cx + size / 10;
        shoulder_y1 = cy - size / 4;
        shoulder_x2 = cx + size / 10;
        shoulder_y2 = cy + size / 4;
        DrawLine(cx + size / 4, cy - size / 10, x + size * 7 / 8, cy - size / 5, BLACK);
        DrawLine(cx + size / 4, cy + size / 10, x + size * 7 / 8, cy + size / 5, BLACK);
    } else {
        hx = x + size * 2 / 3;
        shoulder_x1 = cx - size / 10;
        shoulder_y1 = cy - size / 4;
        shoulder_x2 = cx - size / 10;
        shoulder_y2 = cy + size / 4;
        DrawLine(cx - size / 4, cy - size / 10, x + size / 8, cy - size / 5, BLACK);
        DrawLine(cx - size / 4, cy + size / 10, x + size / 8, cy + size / 5, BLACK);
    }

    DrawCircle(hx, hy, size / 8, BLACK);
    DrawCircle(cx, cy, size / 5, BLACK);
    DrawLine(shoulder_x1, shoulder_y1, shoulder_x2, shoulder_y2, BLACK);
}

static void draw_tile(int x, int y, int size, char c)
{
    DrawRect(x, y, size, size, BLACK);
    if (is_wall(c)) {
        fill_rect(x + 2, y + 2, size - 4, size - 4);
        return;
    }
    if (is_goal(c)) {
        int mark = size / 7;
        int cx = x + size / 2;
        int cy = y + size / 2;
        DrawLine(cx - mark, cy - mark, cx + mark, cy + mark, BLACK);
        DrawLine(cx + mark, cy - mark, cx - mark, cy + mark, BLACK);
    }
    if (is_box(c)) {
        DrawRect(x + size / 5, y + size / 5, size * 3 / 5, size * 3 / 5, BLACK);
        if (c == '*') {
            DrawLine(x + size / 5, y + size / 5, x + size * 4 / 5, y + size * 4 / 5, BLACK);
            DrawLine(x + size * 4 / 5, y + size / 5, x + size / 5, y + size * 4 / 5, BLACK);
        }
    }
    if (is_player(c)) {
        draw_player(x, y, size);
    }
}

static void board_geometry(int *tile_out, int *bx, int *by)
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
    *tile_out = tile;
    *bx = (sw - board_w) / 2;
    *by = 150;
}

static void clear_rect(int x, int y, int w, int h)
{
    for (int i = 0; i < h; ++i) {
        DrawLine(x, y + i, x + w, y + i, WHITE);
    }
}

static void draw_status_line(void)
{
    char line[96];

    if (font_body) {
        SetFont(font_body, BLACK);
    }
    clear_rect(0, 96, ScreenWidth(), 42);
    snprintf(line, sizeof(line), "Level %d/%d   Tahy %d   Tlaky %d", level_index + 1, level_count, game.moves, game.pushes);
    draw_centered(100, line);
}

static void draw_game(void)
{
    int sh = ScreenHeight();
    int tile, bx, by;
    board_geometry(&tile, &bx, &by);

    ClearScreen();
    if (font_title) {
        SetFont(font_title, BLACK);
    }
    draw_centered(45, "Helcin Sokoban");

    draw_status_line();

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

static void redraw_cell(int mx, int my)
{
    int tile, bx, by;
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) {
        return;
    }
    board_geometry(&tile, &bx, &by);
    int x = bx + mx * tile;
    int y = by + my * tile;
    clear_rect(x, y, tile + 1, tile + 1);
    draw_tile(x, y, tile, game.cells[my][mx]);
}

static void redraw_after_move(int old_px, int old_py, int new_px, int new_py, int box_x, int box_y)
{
    draw_status_line();
    redraw_cell(old_px, old_py);
    redraw_cell(new_px, new_py);
    redraw_cell(box_x, box_y);
    PartialUpdate(0, 90, ScreenWidth(), ScreenHeight() - 90);
}

static imenu game_menu[] = {
    {ITEM_ACTIVE, 101, "Undo", NULL},
    {ITEM_ACTIVE, 102, "Restart levelu", NULL},
    {ITEM_ACTIVE, 103, "Dalsi level", NULL},
    {ITEM_ACTIVE, 104, "Predchozi level", NULL},
    {ITEM_ACTIVE, 105, "Vybrat level", NULL},
    {ITEM_ACTIVE, 106, "Zavrit", NULL},
    {0, 0, NULL, NULL}
};

static imenu level_menu[] = {
    {ITEM_ACTIVE, 201, "Level 1", NULL},
    {ITEM_ACTIVE, 202, "Level 2", NULL},
    {ITEM_ACTIVE, 203, "Level 3", NULL},
    {ITEM_ACTIVE, 204, "Level 4", NULL},
    {ITEM_ACTIVE, 205, "Level 5", NULL},
    {ITEM_ACTIVE, 206, "Level 6", NULL},
    {ITEM_ACTIVE, 207, "Level 7", NULL},
    {ITEM_ACTIVE, 208, "Level 8", NULL},
    {ITEM_ACTIVE, 209, "Level 9", NULL},
    {ITEM_ACTIVE, 210, "Level 10", NULL},
    {ITEM_ACTIVE, 211, "Level 11", NULL},
    {ITEM_ACTIVE, 212, "Level 12", NULL},
    {ITEM_ACTIVE, 213, "Level 13", NULL},
    {ITEM_ACTIVE, 214, "Level 14", NULL},
    {ITEM_ACTIVE, 215, "Level 15", NULL},
    {ITEM_ACTIVE, 216, "Level 16", NULL},
    {ITEM_ACTIVE, 217, "Level 17", NULL},
    {ITEM_ACTIVE, 218, "Level 18", NULL},
    {ITEM_ACTIVE, 219, "Level 19", NULL},
    {ITEM_ACTIVE, 220, "Level 20", NULL},
    {ITEM_ACTIVE, 221, "Level 21", NULL},
    {ITEM_ACTIVE, 222, "Level 22", NULL},
    {ITEM_ACTIVE, 223, "Level 23", NULL},
    {ITEM_ACTIVE, 224, "Level 24", NULL},
    {ITEM_ACTIVE, 225, "Level 25", NULL},
    {0, 0, NULL, NULL}
};

static void level_menu_handler(int index)
{
    int selected = 0;

    if (index >= 201 && index <= 225) {
        selected = index - 200;
    } else if (index >= 1 && index <= level_count) {
        selected = index;
    }

    if (selected > 0) {
        load_level(selected - 1);
    }
}

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
        OpenMenu(level_menu, level_index + 1, ScreenWidth() / 4, ScreenHeight() / 4, level_menu_handler);
        break;
    case 6:
    case 106:
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
