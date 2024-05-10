/************************************************************************/
/*                                                                      */
/*             CP/M Hardware Emulator Card Support Program              */
/*                       CPM-BIOS.C Ver 1.10                            */
/*                 Copyright (c) By C.J.Chen NTUEE 1988                 */
/*                        All Right Reserved                            */
/*                                                                      */
/************************************************************************/
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>

#ifdef WIN
#include <synchapi.h>  // for Windows Sleep() function
#include <conio.h> // Windows _kbhit()
#endif //Windows

#ifdef LINUX
#include <unistd.h> // for usleep()
extern void _putch(const char);
extern int _kbhit(void);
extern int _getch(void);
extern int _getche(void);
#define Sleep(x) usleep(1000*x)
#endif //Linux

#include "cpmemu.h"
#include "cpmglob.h"

/*----------------------------------------------------------------------*/
void initialbios(void)
{
    return;
}

/*----------------------------------------------------------------------*/
void bios01(void)        /* cold boot */
{
    dmaaddr = 0x0080;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios02(void)        /* warm boot */
{
    dmaaddr = 0x0080;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios03(void)        /* console status */
{
    int tmp;
    //printf("bios03(console status)\n"); //#debug
    tmp = _kbhit();
    if (tmp  != 0) tmp = 0xff;
    *regl = *rega = (char)tmp;

    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios04(void)        /* console input */
{
    extern int ctrlc_flag;
    //printf("bios04(console input)\n"); //#debug
    while (1)          // wait key stroke, pause to reduce CPU utilization
    {
        Sleep(4);     // sleep 4ms
        if (ctrlc_flag)     // don't process ctrl-c. to allow ctrl-c pass to z80 program
        {
            ctrlc_flag = 0;      // clear ctrl-c flag (not to warmboot for this control-c)
            //    longjmp(ctrl_c,0); // process control-c event
        }
        if (_kbhit()) break;    // exit while when key pressed

		extern char pending_flag;
		if (pending_flag) break; // Still have char in GetKey() to read
    }
    //*rega = _getch();            // read char (no echo)
	extern int GetKey(void);
	*rega = GetKey();

    if (stream != NULL)
    {
        fprintf(stream, "<0x%02x>", *rega); // input use different format for easy debugging
    }
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios05(void)            /* console output */
{
    //printf("bios05(console output) [%c]\n", *rega); //#debug
    _putch(*rega);
    if (stream != NULL) fputc(*rega, stream);
    return;
}
/*----------------------------------------------------------------------*/
void bios06(void)        /* lister output */
{
    //printf("bios06(lister output) [%c]\n", *rega); //#debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios07(void)        /* punch output */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios08(void)        /* reader input */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios09(void)        /* home disk */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios10(void)        /* select disk */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios11(void)        /* set track */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios12(void)        /* set sector */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios13(void)        /* set dma */
{
    dmaaddr = *regbc;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios14(void)        /* read sector */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios15(void)        /* write sector */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bios16(void)        /* list status */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void cpmbios(void)
{
    in_bios = 1;
    switch (*bioscode)
    {
    case 1:
        bios01();
        break;       /* coldboot */
    case 2:
        bios02();
        break;       /* warmboot */
    case 3:
        bios03();
        break;       /* console status */
    case 4:
        bios04();
        break;       /* console input */
    case 5:
        bios05();
        break;       /* console output */
    case 6:
        bios06();
        break;       /* lister output */
    case 7:
        bios07();
        break;       /* punch output */
    case 8:
        bios08();
        break;       /* reader input */
    case 9:
        bios09();
        break;       /* home disk */
    case 10:
        bios10();
        break;      /* select disk */
    case 11:
        bios11();
        break;      /* set track */
    case 12:
        bios12();
        break;      /* set sector */
    case 13:
        bios13();
        break;      /* set dma */
    case 14:
        bios14();
        break;      /* read sector */
    case 15:
        bios15();
        break;      /* write sector */
    case 16:
        bios16();
        break;      /* list status */
    default:
        if (bdosdbgflag[0])printf("unknown BIOS call %d\n", *bioscode);
        break;
    }
    in_bios = 0;
    return;
}