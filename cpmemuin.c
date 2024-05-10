/************************************************************************/
/*                                                                      */
/*             CP/M Hardware Emulator Card Support Program              */
/*                       CPMEMUINT.C Ver 1.45 (QC 2.00 only)            */
/*                 Copyright (c) By C.J.Chen NTUEE 1988                 */
/*                         All Right Reserved                           */
/*                                                                      */
/************************************************************************/

int ctrlc_flag = 0;

#ifdef LINUX
//#include <dos.h>
#include <setjmp.h>
#include <stdio.h>
#include <signal.h>

#include "cpmemu.h"
#include "cpmglob.h"

void handle_ctrl_c(int signo)
{
    //	_enable();
    //	if (in_bios == 1) return;
    //	else {
    //#ifdef MSC
    //		_asm    mov     ax,word ptr stack_segment
    //		_asm    mov     ss,ax
    //#endif
	//	}
	if (signo == SIGINT)
	{
		ctrlc_flag = 1;
	}    
}
#endif // LINUX

#ifdef WIN
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
#include "cpmemu.h"
#include "cpmglob.h"

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        //printf("\n");
        //longjmp(ctrl_c,0);
        ctrlc_flag = 1;
        return TRUE;

    // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
        printf("Ctrl-Close event\n\n");
        return FALSE;

    // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
        //printf("Ctrl-Break event\n\n");
        //return FALSE;

        ctrlc_flag = 1;
        //longjmp(ctrl_c,0); longjmp not work in this handler.
        return TRUE;		

    case CTRL_LOGOFF_EVENT:
        printf("Ctrl-Logoff event\n\n");
        return FALSE;

    case CTRL_SHUTDOWN_EVENT:
        printf("Ctrl-Shutdown event\n\n");
        return FALSE;

    default:
        return FALSE;
    }
}
#endif // Windows