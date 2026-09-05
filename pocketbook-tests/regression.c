#include <assert.h>
#define main device_main
#include APP_SOURCE
#undef main

int main(void) {
#if defined(TEST_SOKOBAN)
    for(int level=0;level<level_count;level++){
        load_level(level);
        for(int i=0;i<5000;i++)move_player(i%4);
    }
    memset(&game,0,sizeof(game));
    game.px=0;game.py=0;game.cells[0][0]='@';
    move_player(DIR_UP);move_player(DIR_LEFT);
    assert(game.px==0 && game.py==0);
    game.px=1;game.py=1;updates=0;
    redraw_after_move(1,1,2,1,3,1);
    assert(updates==2);
    assert(update_rects[1][2]<=3*92+1 && update_rects[1][3]<=93);
#elif defined(TEST_JOURNAL)
    entry_t b;char line[]="Audiokniha;Test;;;;99;999;;;;";
    assert(parse_line(line,&b));assert(b.rating==5 && b.progress==100);
    assert(is_audio("Kapitola.M4A"));
    books[0]=b;book_count=1;
    assert(save_db_to("journal.csv"));book_count=0;
    assert(load_csv("journal.csv",0)==1);
    assert(!save_db_to("/nonexistent-pocketbook-test/db.csv"));assert(warnings==1);
    remove("journal.csv");
#elif defined(TEST_WEATHER)
    FILE *f=fopen("forecast.dat","w");assert(f);
    fputs("META|Brno|20260905|05.09. 16:00\nH|05.09.|16|21|0|2|4|0|0\nREC|Tricko\n",f);fclose(f);
    assert(load_from("forecast.dat")==1);repaint();
    f=fopen("forecast.dat","w");fputs("invalid data\n",f);fclose(f);
    assert(load_from("forecast.dat")==0 && count==1 && hours[0].temp==21);
    remove("forecast.dat");
#elif defined(TEST_TIMER)
    custom_mode=1;custom_minutes=25;
    handle_custom_touch(0,0);assert(custom_mode==1 && custom_minutes==25);
    int x,w,y,a,b,c;custom_layout(&x,&w,&y,&a,&b,&c);
    handle_custom_touch(x+2,b+2);assert(custom_minutes==24);
    handle_custom_touch(x+2,c+107);assert(custom_mode==0);
#elif defined(TEST_PODCAST)
    strcpy(status_text,"before");download_pid=100;refresh();
    assert(strstr(status_text,"Nejprve")!=NULL);
    download_pid=-1;player_pid=100;refresh();
    assert(strstr(status_text,"Nejprve")!=NULL);
#endif
    puts("PASS");return 0;
}
