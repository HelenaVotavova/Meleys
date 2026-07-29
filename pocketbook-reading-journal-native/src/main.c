#include <inkview.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define MAX_BOOKS 300
#define MAX_FILES 180
#define PATH_LEN 512
#define DB_PATH "/mnt/ext1/system/config/helcin-ctenarsky-denik.csv"
#define IMPORT_PATH "/mnt/ext1/HelcinDenik-import.csv"
#define EXPORT_PATH "/mnt/ext1/HelcinDenik-export.csv"

typedef enum { HOME, BOOK_LIST, DETAIL, EDIT, FILES } view_t;
typedef enum { ALL, READING, FINISHED } filter_t;

typedef struct {
    char type[16], title[128], author[96], narrator[96], status[20];
    char started[16], finished[16], note[256], path[PATH_LEN];
    int rating, progress;
} entry_t;

typedef struct { char name[256]; int dir, selected; } file_t;

static entry_t books[MAX_BOOKS];
static file_t files[MAX_FILES];
static int book_count, file_count, selected_count;
static int current = -1, list_page, file_page;
static view_t view = HOME;
static filter_t filter = ALL;
static char folder[PATH_LEN] = "/mnt/ext1";
static ifont *font_title, *font_head, *font_body, *font_small;
static char keyboard_buf[256];
static int edit_field;

static int inside(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

static void text_center(const char *s, int y) {
    DrawString((ScreenWidth() - StringWidth(s)) / 2, y, s);
}

static void button(int x, int y, int w, int h, const char *label, int dark) {
    if (dark) { FillArea(x, y, w, h, BLACK); SetFont(font_body, WHITE); }
    else { DrawRect(x, y, w, h, BLACK); SetFont(font_body, BLACK); }
    DrawString(x + (w - StringWidth(label)) / 2, y + (h - 34) / 2, label);
}

static const char *safe(const char *s) { return s ? s : ""; }

static void clean_field(char *s) {
    for (; *s; ++s) if (*s == ';' || *s == '\n' || *s == '\r') *s = ' ';
}

static void copy_field(char *dst, size_t n, const char *src) {
    snprintf(dst, n, "%s", safe(src));
    clean_field(dst);
}

static int duplicate_path(const char *path) {
    int i;
    for (i = 0; i < book_count; ++i)
        if (path[0] && !strcmp(books[i].path, path)) return 1;
    return 0;
}

static void save_db_to(const char *path) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return;
    fputs("typ;nazev;autor;interpret;stav;hodnoceni;postup;zacatek;dokonceni;poznamka;soubor\n", f);
    for (i = 0; i < book_count; ++i) {
        entry_t *b = &books[i];
        fprintf(f, "%s;%s;%s;%s;%s;%d;%d;%s;%s;%s;%s\n",
            b->type, b->title, b->author, b->narrator, b->status,
            b->rating, b->progress, b->started, b->finished, b->note, b->path);
    }
    fclose(f);
}

static int parse_line(char *line, entry_t *b) {
    char *v[11], *p = line;
    int n = 0;
    while (n < 11) {
        v[n++] = p;
        p = strchr(p, ';');
        if (!p) break;
        *p++ = 0;
    }
    if (n < 2 || !v[1][0]) return 0;
    memset(b, 0, sizeof(*b));
    copy_field(b->type, sizeof(b->type), v[0]);
    copy_field(b->title, sizeof(b->title), v[1]);
    if (n > 2) copy_field(b->author, sizeof(b->author), v[2]);
    if (n > 3) copy_field(b->narrator, sizeof(b->narrator), v[3]);
    copy_field(b->status, sizeof(b->status), n > 4 ? v[4] : "Chci cist");
    b->rating = n > 5 ? atoi(v[5]) : 0;
    b->progress = n > 6 ? atoi(v[6]) : 0;
    if (n > 7) copy_field(b->started, sizeof(b->started), v[7]);
    if (n > 8) copy_field(b->finished, sizeof(b->finished), v[8]);
    if (n > 9) copy_field(b->note, sizeof(b->note), v[9]);
    if (n > 10) copy_field(b->path, sizeof(b->path), v[10]);
    return 1;
}

static int load_csv(const char *path, int merge) {
    FILE *f = fopen(path, "r");
    char line[1400];
    int added = 0, first = 1;
    if (!f) return -1;
    if (!merge) book_count = 0;
    while (book_count < MAX_BOOKS && fgets(line, sizeof(line), f)) {
        entry_t b;
        line[strcspn(line, "\r\n")] = 0;
        if (first && strstr(line, "nazev")) { first = 0; continue; }
        first = 0;
        if (parse_line(line, &b) && !duplicate_path(b.path)) {
            books[book_count++] = b; added++;
        }
    }
    fclose(f);
    return added;
}

static int is_audio(const char *name) {
    const char *e = strrchr(name, '.');
    return e && (!strcasecmp(e, ".mp3") || !strcasecmp(e, ".m4b") ||
        !strcasecmp(e, ".ogg") || !strcasecmp(e, ".flac") ||
        !strcasecmp(e, ".aac") || !strcasecmp(e, ".wav"));
}

static int supported(const char *name) {
    const char *e = strrchr(name, '.');
    if (is_audio(name)) return 1;
    return e && (!strcasecmp(e, ".epub") || !strcasecmp(e, ".pdf") ||
        !strcasecmp(e, ".fb2") || !strcasecmp(e, ".mobi") ||
        !strcasecmp(e, ".djvu") || !strcasecmp(e, ".txt"));
}

static int file_cmp(const void *a, const void *b) {
    const file_t *x = a, *y = b;
    if (x->dir != y->dir) return y->dir - x->dir;
    return strcasecmp(x->name, y->name);
}

static void read_folder(void) {
    DIR *d = opendir(folder);
    struct dirent *de;
    file_count = selected_count = file_page = 0;
    if (!d) return;
    while ((de = readdir(d)) && file_count < MAX_FILES) {
        char path[PATH_LEN]; struct stat st;
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        snprintf(path, sizeof(path), "%s/%s", folder, de->d_name);
        if (stat(path, &st)) continue;
        if (!S_ISDIR(st.st_mode) && !supported(de->d_name)) continue;
        copy_field(files[file_count].name, sizeof(files[file_count].name), de->d_name);
        files[file_count].dir = S_ISDIR(st.st_mode);
        files[file_count].selected = 0;
        file_count++;
    }
    closedir(d);
    qsort(files, file_count, sizeof(files[0]), file_cmp);
}

static void add_selected(void) {
    int i, added = 0;
    for (i = 0; i < file_count && book_count < MAX_BOOKS; ++i) {
        char path[PATH_LEN]; entry_t *b; bookinfo *info;
        if (!files[i].selected || files[i].dir) continue;
        snprintf(path, sizeof(path), "%s/%s", folder, files[i].name);
        if (duplicate_path(path)) continue;
        b = &books[book_count]; memset(b, 0, sizeof(*b));
        info = GetBookInfo(path);
        copy_field(b->title, sizeof(b->title), info && info->title && info->title[0] ? info->title : files[i].name);
        if (info) copy_field(b->author, sizeof(b->author), info->author);
        copy_field(b->type, sizeof(b->type), is_audio(path) ? "Audiokniha" : "E-kniha");
        copy_field(b->status, sizeof(b->status), is_audio(path) ? "Posloucham" : "Ctu");
        copy_field(b->path, sizeof(b->path), path);
        book_count++; added++;
    }
    save_db_to(DB_PATH);
    view = HOME;
    Message(ICON_INFORMATION, "Import dokoncen", added ? "Vybrane tituly byly pridany." : "Nebyl pridan zadny novy titul.", 2500);
}

static int visible_count(void) {
    int i, n = 0;
    for (i = 0; i < book_count; ++i)
        if (filter == ALL || (filter == READING && strcmp(books[i].status, "Dokonceno")) ||
            (filter == FINISHED && !strcmp(books[i].status, "Dokonceno"))) n++;
    return n;
}

static int visible_index(int pos) {
    int i, n = 0;
    for (i = 0; i < book_count; ++i) {
        int ok = filter == ALL || (filter == READING && strcmp(books[i].status, "Dokonceno")) ||
            (filter == FINISHED && !strcmp(books[i].status, "Dokonceno"));
        if (ok && n++ == pos) return i;
    }
    return -1;
}

static void header(const char *title, int back) {
    SetFont(font_title, BLACK);
    if (back) { DrawString(28, 28, "<"); DrawString(80, 28, title); }
    else text_center(title, 30);
    DrawLine(28, 92, ScreenWidth() - 28, 92, BLACK);
}

static void draw_home(void) {
    int w = ScreenWidth(), cardw = w - 100;
    header("Helcin ctenarsky denik", 0);
    SetFont(font_head, BLACK); text_center("Moje knihovna", 135);
    SetFont(font_small, BLACK); text_center("Knihy, e-knihy i audioknihy na jednom miste", 188);
    button(50, 255, cardw, 92, "Rozectene a rozposlouchane", 1);
    button(50, 371, cardw, 92, "Dokoncene tituly", 0);
    button(50, 487, cardw, 92, "+ Pridat titul", 0);
    button(50, 603, cardw, 92, "Vybrat soubory ve ctecce", 0);
    button(50, 719, (cardw - 20) / 2, 82, "Import CSV", 0);
    button(70 + cardw / 2, 719, (cardw - 20) / 2, 82, "Export CSV", 0);
    SetFont(font_small, BLACK);
    { char s[80]; snprintf(s, sizeof(s), "V deniku: %d titulu", book_count); text_center(s, 850); }
}

static void draw_list(void) {
    int i, n = visible_count(), start = list_page * 7;
    header(filter == FINISHED ? "Dokoncene" : filter == READING ? "Rozectene" : "Vsechny tituly", 1);
    for (i = 0; i < 7 && start + i < n; ++i) {
        int idx = visible_index(start + i), y = 120 + i * 112; entry_t *b = &books[idx];
        SetFont(font_head, BLACK); DrawString(42, y, b->title);
        SetFont(font_small, BLACK); DrawString(42, y + 44, b->author[0] ? b->author : "Autor neuveden");
        DrawString(ScreenWidth() - 190, y + 44, b->type);
        DrawLine(35, y + 94, ScreenWidth() - 35, y + 94, BLACK);
    }
    SetFont(font_small, BLACK);
    if (!n) text_center("Zatim tu nejsou zadne tituly.", 350);
    button(35, ScreenHeight() - 90, 150, 62, "Predchozi", 0);
    button(ScreenWidth() - 185, ScreenHeight() - 90, 150, 62, "Dalsi", 0);
}

static void draw_detail(void) {
    entry_t *b = &books[current]; char s[300]; int y = 140;
    header("Detail titulu", 1);
    SetFont(font_title, BLACK); DrawString(45, y, b->title); y += 65;
    SetFont(font_body, BLACK); DrawString(45, y, b->author[0] ? b->author : "Autor neuveden"); y += 62;
    snprintf(s, sizeof(s), "Typ: %s", b->type); DrawString(45, y, s); y += 52;
    snprintf(s, sizeof(s), "Stav: %s", b->status); DrawString(45, y, s); y += 52;
    snprintf(s, sizeof(s), "Postup: %d %%", b->progress); DrawString(45, y, s); y += 52;
    snprintf(s, sizeof(s), "Hodnoceni: %d / 5", b->rating); DrawString(45, y, s); y += 70;
    DrawRect(45, y, ScreenWidth() - 90, 28, BLACK);
    if (b->progress) FillArea(48, y + 3, (ScreenWidth() - 96) * b->progress / 100, 22, BLACK);
    SetFont(font_small, BLACK); DrawString(45, y + 70, b->note[0] ? b->note : "Bez poznamky");
    button(45, ScreenHeight() - 110, ScreenWidth() - 90, 72, "Upravit zaznam", 1);
}

static void draw_edit(void) {
    entry_t *b = &books[current]; char s[300]; int w = ScreenWidth() - 90;
    header("Upravit zaznam", 1);
    SetFont(font_small, BLACK); DrawString(48, 126, "Nazev");
    button(45, 160, w, 68, b->title, 0);
    SetFont(font_small, BLACK); DrawString(48, 245, "Autor / interpret");
    button(45, 279, w, 68, b->author[0] ? b->author : "Doplnit autora", 0);
    snprintf(s, sizeof(s), "Typ: %s", b->type); button(45, 380, w, 68, s, 0);
    snprintf(s, sizeof(s), "Stav: %s", b->status); button(45, 470, w, 68, s, 0);
    snprintf(s, sizeof(s), "Hodnoceni: %d / 5", b->rating); button(45, 560, w, 68, s, 0);
    snprintf(s, sizeof(s), "Postup: %d %%", b->progress); button(45, 650, w, 68, s, 0);
    SetFont(font_small, BLACK); DrawString(48, 740, "Poznamka");
    button(45, 774, w, 68, b->note[0] ? b->note : "Pridat poznamku", 0);
    button(45, ScreenHeight() - 100, w, 68, "Ulozit", 1);
}

static void draw_files(void) {
    int i, start = file_page * 7; char s[PATH_LEN + 40];
    header("Vyber soubory", 1);
    SetFont(font_small, BLACK); snprintf(s, sizeof(s), "Slozka: %s", folder); DrawString(35, 108, s);
    for (i = 0; i < 7 && start + i < file_count; ++i) {
        file_t *f = &files[start + i]; int y = 155 + i * 94;
        DrawRect(38, y + 10, 38, 38, BLACK);
        if (f->dir) { SetFont(font_body, BLACK); DrawString(45, y + 6, ">"); }
        else if (f->selected) { SetFont(font_body, BLACK); DrawString(45, y + 6, "X"); }
        SetFont(font_body, BLACK); DrawString(95, y, f->name);
        DrawLine(35, y + 70, ScreenWidth() - 35, y + 70, BLACK);
    }
    snprintf(s, sizeof(s), "Oznaceno: %d", selected_count); SetFont(font_small, BLACK); DrawString(40, ScreenHeight() - 142, s);
    button(35, ScreenHeight() - 92, 145, 62, "Nahoru", 0);
    button(195, ScreenHeight() - 92, 145, 62, "Dalsi", 0);
    button(ScreenWidth() - 250, ScreenHeight() - 92, 215, 62, "Pridat vybrane", 1);
}

static void repaint(void) {
    ClearScreen();
    switch (view) {
        case HOME: draw_home(); break; case BOOK_LIST: draw_list(); break;
        case DETAIL: draw_detail(); break; case EDIT: draw_edit(); break;
        case FILES: draw_files(); break;
    }
    FullUpdate();
}

static void keyboard_done(char *text) {
    entry_t *b = &books[current];
    if (text) {
        if (edit_field == 1) copy_field(b->title, sizeof(b->title), text);
        else if (edit_field == 2) copy_field(b->author, sizeof(b->author), text);
        else copy_field(b->note, sizeof(b->note), text);
    }
    view = EDIT; repaint();
}

static void open_keyboard_field(int field, const char *value, const char *title) {
    edit_field = field; copy_field(keyboard_buf, sizeof(keyboard_buf), value);
    OpenKeyboard(title, keyboard_buf, sizeof(keyboard_buf) - 1, 0, keyboard_done);
}

static void new_book(void) {
    if (book_count >= MAX_BOOKS) return;
    current = book_count++; memset(&books[current], 0, sizeof(books[current]));
    copy_field(books[current].type, sizeof(books[current].type), "Kniha");
    copy_field(books[current].status, sizeof(books[current].status), "Chci cist");
    copy_field(books[current].title, sizeof(books[current].title), "Novy titul");
    view = EDIT;
}

static void go_parent(void) {
    char *p;
    if (!strcmp(folder, "/mnt/ext1")) return;
    p = strrchr(folder, '/');
    if (p && p != folder) *p = 0;
    if (!strncmp(folder, "/mnt/ext1", 9)) read_folder();
    else strcpy(folder, "/mnt/ext1");
}

static void touch(int x, int y) {
    int w = ScreenWidth(), h = ScreenHeight();
    if (y < 100 && x < 90 && view != HOME) {
        view = (view == DETAIL || view == FILES || view == BOOK_LIST) ? HOME : DETAIL; repaint(); return;
    }
    if (view == HOME) {
        if (inside(x,y,50,255,w-100,92)) { filter=READING; list_page=0; view=BOOK_LIST; }
        else if (inside(x,y,50,371,w-100,92)) { filter=FINISHED; list_page=0; view=BOOK_LIST; }
        else if (inside(x,y,50,487,w-100,92)) new_book();
        else if (inside(x,y,50,603,w-100,92)) { strcpy(folder,"/mnt/ext1"); read_folder(); view=FILES; }
        else if (inside(x,y,50,719,(w-120)/2,82)) {
            int n=load_csv(IMPORT_PATH,1); if(n>=0){save_db_to(DB_PATH); Message(ICON_INFORMATION,"Import CSV","Import byl dokoncen.",2000);} else Message(ICON_WARNING,"Import CSV","Soubor HelcinDenik-import.csv nebyl nalezen.",3000);
        } else if (y >= 719) { save_db_to(EXPORT_PATH); Message(ICON_INFORMATION,"Export CSV","Soubor byl ulozen do korenove slozky ctecky.",2500); }
    } else if (view == BOOK_LIST) {
        if (y >= 120 && y < 904) { int row=(y-120)/112, idx=visible_index(list_page*7+row); if(idx>=0){current=idx;view=DETAIL;} }
        else if (y > h-110 && x < w/2 && list_page) list_page--;
        else if (y > h-110 && x >= w/2 && (list_page+1)*7 < visible_count()) list_page++;
    } else if (view == DETAIL) {
        if (y > h-140) view=EDIT;
    } else if (view == EDIT) {
        entry_t *b=&books[current];
        if (inside(x,y,45,160,w-90,68)) open_keyboard_field(1,b->title,"Nazev titulu");
        else if (inside(x,y,45,279,w-90,68)) open_keyboard_field(2,b->author,"Autor nebo interpret");
        else if (inside(x,y,45,380,w-90,68)) copy_field(b->type,sizeof(b->type),!strcmp(b->type,"Kniha")?"E-kniha":!strcmp(b->type,"E-kniha")?"Audiokniha":"Kniha");
        else if (inside(x,y,45,470,w-90,68)) copy_field(b->status,sizeof(b->status),!strcmp(b->status,"Chci cist")?(!strcmp(b->type,"Audiokniha")?"Posloucham":"Ctu"):(!strcmp(b->status,"Ctu")||!strcmp(b->status,"Posloucham"))?"Dokonceno":"Chci cist");
        else if (inside(x,y,45,560,w-90,68)) b->rating=(b->rating+1)%6;
        else if (inside(x,y,45,650,w-90,68)) b->progress=(b->progress+10)%110;
        else if (inside(x,y,45,774,w-90,68)) open_keyboard_field(3,b->note,"Poznamka");
        else if (y > h-130) { save_db_to(DB_PATH); view=DETAIL; }
    } else if (view == FILES) {
        if (y >= 155 && y < 813) {
            int idx=file_page*7+(y-155)/94;
            if(idx<file_count){
                if(files[idx].dir){
                    char next[PATH_LEN];
                    snprintf(next,sizeof(next),"%s/%s",folder,files[idx].name);
                    copy_field(folder,sizeof(folder),next);
                    read_folder();
                }else{
                    files[idx].selected=!files[idx].selected;
                    selected_count+=files[idx].selected?1:-1;
                }
            }
        } else if (y > h-120 && x < 185) go_parent();
        else if (y > h-120 && x < 360) { if((file_page+1)*7<file_count)file_page++; else file_page=0; }
        else if (y > h-120) add_selected();
    }
    repaint();
}

static int handler(int type, int par1, int par2) {
    if (type == EVT_INIT) {
        font_title=OpenFont(DEFAULTFONTB,36,1); font_head=OpenFont(DEFAULTFONTB,30,1);
        font_body=OpenFont(DEFAULTFONT,28,1); font_small=OpenFont(DEFAULTFONT,22,1);
        load_csv(DB_PATH,0); repaint();
    } else if (type == EVT_REPAINT) repaint();
    else if (type == EVT_POINTERUP) touch(par1,par2);
    else if (type == EVT_KEYDOWN && par1 == IV_KEY_BACK) {
        if(view==HOME) CloseApp(); else {view=HOME;repaint();}
    } else if (type == EVT_EXIT) {
        save_db_to(DB_PATH); CloseFont(font_title); CloseFont(font_head); CloseFont(font_body); CloseFont(font_small);
    }
    return 0;
}

int main(void) { InkViewMain(handler); return 0; }
