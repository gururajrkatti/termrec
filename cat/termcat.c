#include <stdio.h>
#include <ttyrec.h>
#include "sys/error.h"
#include "gettext.h"

struct timeval tt;
recorder rec;

void delay(struct timeval *tm, void *arg)
{
    tadd(tt, *tm);
}

void print(char *buf, int len, void *arg)
{
    ttyrec_w_write(rec, &tt, buf, len);
}

int main(int argc, char **argv)
{
    int i;
    
    if (argc<3)
        die("%s termcat <%s> ... <dest>\n", _("Usage:"), _("filename"));
    tt.tv_sec=tt.tv_usec=0;
    if (!(rec=ttyrec_w_open(-1, 0, argv[argc-1], 0)))
        die("Failed to open destination file");
    for(i=1;i<argc-1;i++)
        ttyrec_r_play(-1, 0, argv[i], 0, delay, print, 0);
    ttyrec_w_close(rec);
    return 0;
}