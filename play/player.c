#include "config.h"
#include <vt100.h>
#include <ttyrec.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "error.h"
#include "gettext.h"
#include "sys/threads.h"
#include "play/player.h"

static ttyrec_frame fr;
static struct timeval t0, tc;


static int player(void *arg)
{
    ttyrec_frame nf;
    struct timeval tt, wall;
    
    do
    {
        while((nf=ttyrec_next_frame(tr, fr)))
        {
            fr=nf;
            tt=nf->t;
            tdiv1000(tt, speed);
            tadd(tt, t0);
            gettimeofday(&wall, 0);
            tsub(tt, wall);
            if (tt.tv_sec>0 || tt.tv_usec>0)
                select(0, 0, 0, 0, &tt);
            vt100_write(term, fr->data, fr->len);
        }
        mutex_lock(waitm);
        if (!(nf=ttyrec_next_frame(tr, fr)) && !loaded)
            waiting=1;
        if (waiting)
        {
            cond_wait(waitc, waitm);
            nf=ttyrec_next_frame(tr, fr);
        }
        mutex_unlock(waitm);
    }
    while(nf || !loaded);
    return 0;
}


static thread_t playth;
static int play_state;

static void replay_start()
{
    struct timeval tm;
    if (play_state)
        return;
    play_state=1;
    gettimeofday(&t0, 0);
    tm=tc;
    tdiv1000(tc, speed);
    tsub(t0, tc);
    if (thread_create_joinable(&playth, player, 0))
        die("Cannot create thread");
}


static void replay_stop()
{
    if (!play_state)
        return;
    gettimeofday(&tc, 0);	/* calc where we stopped */
    tsub(tc, t0);
    tmul1000(tc, speed);
    play_state=0;
    pthread_cancel(playth);
    thread_join(playth);
    if (loaded && !ttyrec_next_frame(tr, fr))
        tc=fr->t;		/* no stopping past the end */
}


static void adjust_pos(int diff)
{
    int old_state=play_state;
    
    replay_stop();
    tc.tv_sec+=diff;
    if (tc.tv_sec<0)
        tc.tv_sec=tc.tv_usec=0;
    fr=ttyrec_seek(tr, &tc, &term);
    vtvt_attach(term, stdout, 1);
    if (old_state)
        replay_start();
}


static void adjust_speed(int dir)
{
    int old_state=play_state;
    
    replay_stop();
    switch(dir)
    {
    case 0:
        speed=1000;
        break;
    case -1:
        if ((speed/=2)==0)
            speed=1;
        break;
    case +1:
        if ((speed*=2)>1000000)
            speed=1000000;
        break;
    }
    if (old_state)
        replay_start();
}


static void replay_toggle()
{
    if (play_state)
        replay_stop();
    else
        replay_start();
}


static void replay_rewind()
{
    replay_stop();
    tc.tv_sec=tc.tv_usec=0;
}

static void adv_frame()
{
    ttyrec_frame nf;
    
    replay_stop();
    if (!(nf=ttyrec_next_frame(tr, fr)))
        return;
    tc=nf->t;
    vt100_write(term, nf->data, nf->len);
    fr=nf;
}

static struct bind
{
    char *keycode;
    void(*func)(int);
    int arg;
} binds[]=
{
{"q",0/*quit*/,0},	/* q/Q		-> quit */
{"Q",0/*quit*/,0},
{" ",replay_toggle,0},	/* space	-> play/pause */
{"\e[D",adjust_pos,-10},/* left arrow	-> -10sec */
{"\eOD",adjust_pos,-10},
{"\eOt",adjust_pos,-10},
{"\e[C",adjust_pos,+10},/* right arrow	-> +10sec */
{"\eOC",adjust_pos,+10},
{"\eOv",adjust_pos,+10},
{"\e[B",adjust_pos,-60},/* down arrow	-> -1min */
{"\eOB",adjust_pos,-60},
{"\eOr",adjust_pos,-60},
{"\e[A",adjust_pos,+60},/* up arrow	-> +1min */
{"\eOA",adjust_pos,+60},
{"\eOx",adjust_pos,+60},
{"\e[6~",adjust_pos,-600},	/* PgDn	-> -10min */
{"\eOs", adjust_pos,-600},
{"\e[5~",adjust_pos,+600},	/* PgUp -> +10min */
{"\eOy", adjust_pos,+600},
{"r",replay_rewind,0},	/* r/R		-> rewind */
{"R",replay_rewind,0},
{"1",adjust_speed,0},	/* 1		-> speed 1.0 */
{"s",adjust_speed,-1},	/* s/S/-	-> slow down */
{"S",adjust_speed,-1},
{"-",adjust_speed,-1},
{"f",adjust_speed,+1},	/* f/F/+	-> speed up */
{"F",adjust_speed,+1},
{"+",adjust_speed,+1},
{"\n",adv_frame,+1},	/* Enter	-> next frame */
{"\r",adv_frame,+1},
{"\eOM",adv_frame,+1},
{0,0,0},
};

static char keycode[10];

#define GETKEY			\
        if ((ch=getchar())==EOF)\
            goto end;		\
        *kptr++=ch;		\
        switch(ch)
void replay()
{
    char *kptr;
    int ch;
    struct bind *bp;
    
    play_state=0;
    tc.tv_sec=tc.tv_usec=0;
    fr=ttyrec_seek(tr, 0, &term);
    vtvt_attach(term, stdout, 1);
    replay_start();
    
    while (1)
    {
        kptr=keycode;
        GETKEY
        {
        case 27:
            GETKEY
            {
            case 'O':
                GETKEY{}
                /* ESC O A */
                break;
            case '[':
                GETKEY
                {
                case 'O':
                    GETKEY{}
                    /* ESC [ O A */
                    break;
                case '[':
                    GETKEY{}
                    /* ESC [ [ A */
                    break;
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    while(ch>='0' && ch<='9' && kptr-keycode<6)
                    {
                        GETKEY{}
                        /* ESC [ 11 ~ */
                    }
                    break;
                /* ESC [ A */
                }
                break;
            }
            break;
        }
        *kptr=0;
        for(bp=binds; bp->keycode; bp++)
            if (!strcmp(bp->keycode, keycode))
            {
                if (bp->func)
                    bp->func(bp->arg);
                else
                    goto end;
            }
            
    }
end:
    replay_stop();
    vt100_printf(term, "\e[f\e[200B");
    vt100_free(term);
}
