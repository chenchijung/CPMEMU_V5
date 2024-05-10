/************************************************************************/
/*																		*/
/*	  		   CP/M Hardware Emulator Card Support Program				*/
/*			  				   BDOS Part								*/
/*						 CPM-BDOS.C Ver 1.20							*/
/*	      		 Copyright (c) By Chen Chi-Jung NTUEE 1988				*/
/*						All Right Reserved								*/
/*																		*/
/************************************************************************/
/*
** Include File Defination
*/
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef WIN
#include <synchapi.h>  	// for Windows Sleep() function
#include <conio.h> 		// Windows _kbhit()
#include <windows.h>
#include <io.h>
#endif

#ifdef LINUX
extern int _putch(const char);
extern int _kbhit(void);
extern int _getch(void);
extern int _getche(void);
inline int32_t max(int32_t a, int32_t b)
{
    return((a) > (b) ? a : b);
}
inline int32_t min(int32_t a, int32_t b)
{
    return((a) < (b) ? a : b);
}
#include <unistd.h> // for usleep()
#define Sleep(x) usleep(1000*x)
#endif

#include "cpmemu.h"
#include "cpmglob.h"

#define BUFSIZE MAX_PATH
/*
**  Grobol Variable Used in This File only
*/
static UINT8 user_code = 0;

//
// FCB Offset read/write APIs (compatible with CPM2.x)
//
#define FCB_S2 14 // bit 0-7 = bit 0-7   of sequential offset
#define FCB_EX 12 // bit 0-5 = bit 8-12  of sequential offset
#define FCB_CR 32 // bit 0-3 = bit 13-15 of seuential offset

#define FCB_R0 33 // bit 0-7 = bit 0-7   of random offset
#define FCB_R1 34 // bit 0-7 = bit 8-15  of random ofset
#define FCC_R2 35 // bit 0 = overflow bit of random offset
UINT32 GetFCBSeqOffset(UINT8 *fcb)
{
    UINT32 offset;
    offset = fcb[FCB_S2] * 524288L + fcb[FCB_EX] * 16384L + fcb[FCB_CR] * 128L;
    return offset;
}
void SetFCBSeqOffset(UINT8 *fcb, UINT32 offset)
{
    fcb[FCB_CR] = (offset % 16384) / 128;
    fcb[FCB_EX] = (offset % 524288) / 16384;
    fcb[FCB_S2] = offset / 524288;
    return;
}
UINT32 GetFCBRandOffset(UINT8 *fcb)
{
    UINT32 offset;
    offset = fcb[FCB_R1] * 32768 + fcb[FCB_R0] * 128;
    return offset;
}
void SetFCBRandOffset(UINT8 *fcb, UINT32 offset)
{
    fcb[FCB_R0] = (offset / 128) % 256;
    fcb[FCB_R1] = (offset / 128) / 256;
    // fcb[FCB_R2] = 1 if offset > 8M // overflow not implemented yet.
    return;
}
/*----------------------------------------------------------------------*/
int isvalidcpmfilename(char* filename)
{
	char *ptr1;
	// check filename for find_first, find_next and delete bdos call is a valid 8+3 format or not
	// filename do not contain drive letter

	//printf("isvalidcpmfilename(%s),", filename); // debug
	// check filename part (length <=8)
	if ((ptr1 = strchr(filename,'.'))!=NULL)
	{
		//filename has both name and ext part
		if ((ptr1-filename) <= 8) // name part length <=8
		{
			if (strlen(ptr1+1) <= 3) // ext part length <=3
			{
				//printf("result 1A\n"); // debug
				return 1; // valid
			}
		}
	}
	else
	{
		// No ext part, only name part
		if (strlen(filename) <= 8)
		{
			//printf("result 1B\n"); // debug
			return 1; // valid
		}
	}
	//printf("result 0\n"); // debug
	return 0; // invalid
}
/*----------------------------------------------------------------------*/
/* fcbtofilename:							*/
/*	 Input: ptr1: pointer indicate the filename in a FCB		*/
/*		ptr2: pointer to a buffer				*/
/*	Output: Filename in the buffer pointer by ptr2			*/
/*----------------------------------------------------------------------*/
void fcbtofilename(char *fcb, char *ptr2)
{
    int i;
    char currentdrive;
#ifdef WIN
    TCHAR Buffer[BUFSIZE];
    DWORD dwRet;

    // #dchen 20240403 only append drive number when drive number != current drive
    if (fcb[0] != 0)
    {
        dwRet = GetCurrentDirectory(BUFSIZE, Buffer);

        if( dwRet == 0 )
        {
            printf("GetCurrentDirectory failed (%lu)\n", GetLastError());
            return;
        }
        currentdrive = Buffer[0] - 'A' + 1;
        if (fcb[0] != currentdrive)
        {
            *ptr2++ = fcb[0] + 64;
            *ptr2++ = ':';
        }
    }
#endif
#ifdef LINUX
    // Linux porting can't support different drive letter since there is no drive letter support in Linux native file system.
    // So, always fill fcb[0] = 0x00;
    //fcb[0] = 0x00; <--- bug! can not change fcb contents here (just read, can't write!!!). Just ignore fcb[0] is enough.
	// This bug cause UNARCA.COM only extract 1 file.
#endif
    for (i = 1 ; i < 9 ; i++)
    {
        if(fcb[i] != ' ') *ptr2++ = fcb[i];
    }
	if (strncmp(&fcb[9],"   ",3)!=0) *ptr2++ = '.';
	for (i = 9 ; i < 12 ; i++)
    {
		if(fcb[i] != ' ') *ptr2++ = fcb[i];
    }
    *ptr2 = 0x00;
    return;
}
/*----------------------------------------------------------------------*/
/*  SearchOpenedFCBTableIdx: use filename to get already opened FCB file table array index       */
/*----------------------------------------------------------------------*/
UINT8 SearchOpenedFCBTableIdx(char *filename)
{
    UINT8 fcbidx = 0;
    while (strcasecmp((char *)fcbused[fcbidx], filename) != 0 &&
            fcbidx <= MAXFILE) fcbidx++;
    return fcbidx;
}
/*----------------------------------------------------------------------*/
/*  SearchUnusedFCBTableIdx: use filename to get unused FCB file table array index       */
/*----------------------------------------------------------------------*/
UINT8 SearchUnusedFCBTableIdx(char *filename)
{
    UINT8 fcbidx = 0;
    while (strcasecmp((char *)fcbused[fcbidx], filename) != 0 &&
            fcbused[fcbidx][0] != 0	&& fcbidx <= MAXFILE) fcbidx++;
    return fcbidx;
}
/*----------------------------------------------------------------------*/
/*  opensuccess: If we open a File required by CP/M Successfully,       */
/*	       Then we call this function to fill the table.	  */
/*----------------------------------------------------------------------*/
void opensuccess(UINT8 fcbidx, char *filename)
{
    fseek(fcbfile[fcbidx], 0L, SEEK_END);	/* Move to EOF */
    fcbfilelen[fcbidx] = ftell(fcbfile[fcbidx]); /* Fill File Length */
    rewind(fcbfile[fcbidx]);		   /* Restore file */
    strcpy((char *)fcbused[fcbidx], filename);	 /* Fill Filename to table */
    return;
}
/*----------------------------------------------------------------------*/
void PrintDebug(void)
{
    char name[20], newname[20];
    if (lastcall == *regc) repeats++;
    else
    {
        if ( *regc >= 15 )
        {
            fcbtofilename((char *)&ram[*regde], name);
            fcbtofilename((char *)&ram[*regde + 16], newname);
        }
        if (lastcall < 39)
        {
            fprintf(lpt, "% 5d  %s%s\n",
                    repeats, debugmess1, debugmess2);

            printf("%5d  %s%s\n", repeats, debugmess1, debugmess2);
            fflush(stdout);
        }
        sprintf(debugmess1, "%04XH    %04XH     %04XH   %02XH "
                , *regip, (*regde & 0xFFFF), (dmaaddr & 0xFFFF), *regc);
        switch ( *regc )
        {
        case 0:
            sprintf(debugmess2, "System reset ");
            break;
        case 1:
            sprintf(debugmess2, "Console input");
            break;
        case 2:
            sprintf(debugmess2, "Console output");
            break;
        case 3:
            sprintf(debugmess2, "Reader input");
            break;
        case 4:
            sprintf(debugmess2, "Punch output");
            break;
        case 5:
            sprintf(debugmess2, "List output");
            break;
        case 6:
            sprintf(debugmess2, "Direct console i/o");
            break;
        case 7:
            sprintf(debugmess2, "Get iobyte");
            break;
        case 8:
            sprintf(debugmess2, "Set iobyte");
            break;
        case 9:
            sprintf(debugmess2, "Print string");
            break;
        case 10:
            sprintf(debugmess2, "Read console buffer");
            break;
        case 11:
            sprintf(debugmess2, "Get console status");
            break;
        case 12:
            sprintf(debugmess2, "Get version number");
            break;
        case 13:
            sprintf(debugmess2, "Reset disk system");
            break;
        case 14:
            sprintf(debugmess2, "Select disk");
            break;
        case 15:
            sprintf(debugmess2, "Open file %s", name);
            break;
        case 16:
            sprintf(debugmess2, "Close file %s", name);
            break;
        case 17:
            sprintf(debugmess2, "Search for first %s", name);
            break;
        case 18:
            sprintf(debugmess2, "Search for next %s", name);
            break;
        case 19:
            sprintf(debugmess2, "Delete file %s", name);
            break;
        case 20:
            sprintf(debugmess2, "Read sequential %s, offset %u"
                    , name, GetFCBSeqOffset(&ram[*regde]));
            break;
        case 21:
            sprintf(debugmess2, "Write sequential %s, offset %u"
                    , name, GetFCBSeqOffset(&ram[*regde]));
            break;
        case 22:
            sprintf(debugmess2, "Make file %s", name);
            break;
        case 23:
            sprintf(debugmess2, "Rename file %s to %s"
                    , name, newname);
            break;
        case 24:
            sprintf(debugmess2, "Get login vector");
            break;
        case 25:
            sprintf(debugmess2, "Get current disk");
            break;
        case 26:
            sprintf(debugmess2, "Set DMA address");
            break;
        case 27:
            sprintf(debugmess2, "Get alloc address");
            break;
        case 28:
            sprintf(debugmess2, "Write protect disk");
            break;
        case 29:
            sprintf(debugmess2, "Get r/o vector");
            break;
        case 30:
            sprintf(debugmess2, "Set file attributes");
            break;
        case 31:
            sprintf(debugmess2, "Get disk parms");
            break;
        case 32:
            sprintf(debugmess2, "Get & set user code");
            break;
        case 33:
            sprintf(debugmess2, "Read random %s, offset %u", name, GetFCBRandOffset(&ram[*regde]));
            break;
        case 34:
            sprintf(debugmess2, "Write random %s, offset %u", name, GetFCBRandOffset(&ram[*regde]));
            break;
        case 35:
            sprintf(debugmess2, "Compute file size");
            break;
        case 36:
            sprintf(debugmess2, "Set random record");
            break;
        case 37:
            sprintf(debugmess2, "Reset drive");
            break;
        case 40:
            sprintf(debugmess2, "Write random (zero)");
            break;
        default:
            sprintf(debugmess2, "Unknown bdos call %2XH", *regc);
            break;
        }
        repeats = 1;
        lastcall = *regc;
    }
    return;
}
/*----------------------------------------------------------------------*/
// dump windows program memory
void dumpmem(UINT8 *ram, UINT16 len)
{
    UINT16 i;
    UINT16 addr = 0;
	UINT8 ch;

    for (addr = 0 ; addr < len; addr += 0x10)
    {
        printf("%04X: %02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X ",
               addr, ram[addr], ram[addr + 1], ram[addr + 2], ram[addr + 3], ram[addr + 4], ram[addr + 5], ram[addr + 6], ram[addr + 7],
               ram[addr + 8], ram[addr + 9], ram[addr + 10], ram[addr + 11], ram[addr + 12], ram[addr + 13], ram[addr + 14], ram[addr + 15]);
        for (i = 0; i <= 15; i++)
        {
			ch = ram[addr + i];
            if ((ch >= 0x20)&&(ch<=0x7F)) printf("%c", ch);
            else printf(".");
        }
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
    return;
}
// dump CP/M RAM space memory
void dumpram(UINT16 address, UINT16 len)
{
    UINT16 addr;
    UINT16 i;
	UINT8 ch;

    for (addr = (address) ; addr < address + len; addr += 0x10)
    {
        printf("%04X: %02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X ",
               addr, ram[addr], ram[addr + 1], ram[addr + 2], ram[addr + 3], ram[addr + 4], ram[addr + 5], ram[addr + 6], ram[addr + 7],
               ram[addr + 8], ram[addr + 9], ram[addr + 10], ram[addr + 11], ram[addr + 12], ram[addr + 13], ram[addr + 14], ram[addr + 15]);
        for (i = 0; i <= 15; i++)
        {
			ch = ram[addr + i];
            if ((ch >= 0x20)&&(ch<=0x7F)) printf("%c", ch);
            else printf(".");
        }
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
    return;
}
void dumpfcb(UINT16 address)
{
    dumpram(address, 0x40);
    return;
}

/*----------------------------------------------------------------------*/
/* CPM BDOS API Services                                                */
/*----------------------------------------------------------------------*/
void initialbdos(void)
{
    return;
}
/*----------------------------------------------------------------------*/
void bdos00(void)	/* system reset -not ready */
{
    if (bdosdbgflag[0]) printf("bdos00(system reset)\n"); //debug
    dmaaddr = 0x0080;		 	/* Set DMA address to 80h */
    *eop = 1;			 		/* End of execution */
    return;
}
/*----------------------------------------------------------------------*/
void bdos01(void)	/* console input */
{
    char ch;
	// No need to implement XSUB on this call -- according to CP/M documents.
    //if (xsubflag)   // xsub support
    //{
    //    // submit file input (xsub on)
    //    ch = fgetc(subfile);
    //    chkclosesubfile();
	//
    //    *regl = *rega = ch;
    //    *regh = 0x00;
    //}
    //else
    {
        // Normal keyboard input
        while (1)          // wait key stroke, pause to reduce CPU utilization
        {
            Sleep(4);     // sleep 4ms
            if (ctrlc_flag)
            {
                ctrlc_flag = 0;    // clear ctrl-c flag
                longjmp(ctrl_c, 0); // process control-c event (ctrl-c to stop and close Z80 program)
            }
            if (_kbhit()) break;    // exit while when key pressed

			extern char pending_flag;
			if (pending_flag) break; //still have char in GetKey() to read
        }

		extern int GetKey(void);
		*regl = *rega = GetKey();
		_putch(*rega);		
        *regh = 0x00;
		//if (*rega == 0x03) longjmp(ctrl_c,0);
		
        if (stream != NULL)
        {
            //    fputc(*rega, stream);
            fprintf(stream, "<0x%02x>", *rega); // input use different format for easy debugging
        }
    }
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos02(void)	    /* character output */
{
    //putchar(*rege);
    _putch(*rege);
    if (ctrlc_flag)   // process ctrl-c
    {
        ctrlc_flag = 0;
        longjmp(ctrl_c, 0);
    }
    if (stream != NULL) fputc(*rege, stream); // for console output stream log
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos03(void)		/* reader input */
{
    if (bdosdbgflag[3]) printf("bdos03(reader input)\n"); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos04(void)	    /* punch output */
{
    if (bdosdbgflag[4]) printf("bdos04(punch output)\n"); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos05(void)	    /* list output */
{
    //UINT16 ch;

    if (bdosdbgflag[5]) printf("bdos05(list output)\n"); //debug
    //ch = (UINT16) * rege;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos06(void)	    /* direct console i/o -not check!*/
{
    UINT8 tmp;
    char ch;

    if (bdosdbgflag[6]) printf("bdos06(direct console i/o) rege= %c(0x%02x),", *rege, *rege); // #debug
    if ((*regde & 0x00ff) == 0x00ff)
    {
		// No need to implement XSUB on this call -- According to CP/M documents.
        //if (xsubflag)
        //{
        //    // submit file input when xsub is active
        //    ch = fgetc(subfile);
        //    chkclosesubfile();
        //    *regl = *rega = ch;
        //    *regh = 0x00;
        //}
        //else
        {
            // normal keyboard input
            tmp = _kbhit() ? _getch() : 0x00;
            *regl = *rega = tmp;	// fix bug for smallc_27 (need to return *rega)
            *regh = 0x00;
        }
    }
    else
    {
        _putch(*regde & 0x00ff);
        if (stream != NULL) fputc(*rege, stream); // for console output stream log
    }
    if (bdosdbgflag[6]) printf(" rega=0x%02x\n", *rega); // #debug
    ReturnZ80;

    return;
}
/*----------------------------------------------------------------------*/
void bdos07(void)	    /* get i/o byte */
{
    char *ptr1;

    if (bdosdbgflag[7]) printf("bdos07(get i/o byte)\n"); //debug
    ptr1 = (char *)(&ram[0x0003]);
    *regl = *rega = *ptr1;
    *regh = 0x00;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos08(void)	    /* set i/o byte */
{
    char *ptr1;

    if (bdosdbgflag[8]) printf("bdos08(set i/o byte)\n"); //debug
    ptr1 = (char *)(&ram[0x0003]);
    *ptr1 = *rege;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos09(void)	    /* print string */
{
    char *addr;

    if (bdosdbgflag[9]) printf("bdos09(print string)\n"); //debug
    addr = (char *)&ram[(*regde)];
    do
    {
        //putchar(*addr++);
        if (stream != NULL) fputc(*addr, stream); // for console out stream debug
        _putch(*addr++);
    }
    while (*addr != '$');
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos10(void)	    /* read console buffer */
{
    UINT8 i = 0;
    char tmpbuffer[CPMCMDBUF]; // can't be enlarged since the max len is only 255
    UINT8 maxlen;

    memset(tmpbuffer, 0x00, CPMCMDBUF); // clear tmpbuffer

    if (bdosdbgflag[10]) printf("bdos10(read console buffer)\n"); //#debug
    //   if(gear_fgets(tmpbuffer,sizeof(tmpbuffer), stdin, 1))
    getstring(tmpbuffer, sizeof(tmpbuffer));	// to support submit/xsub

    for (i = 0; i < strlen(tmpbuffer); i++)
    {
        if (tmpbuffer[i] == 0x0a) tmpbuffer[i] = 0x00; // #debug remove tail 0x0a;
    }
    maxlen = (UINT8)min(ram[*regde], (UINT8)strlen(tmpbuffer));
    ram[*regde + 1] = maxlen;
    strncpy((char *)&ram[*regde + 2], tmpbuffer, maxlen);

    if (bdosdbgflag[10]) dumpram(*regde, ram[*regde] + 2 ); //#debug
    if (bdosdbgflag[10]) printf("bdos10() end\n"); //#debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos11(void)   /* read console status */
{
    int tmp;
    tmp = _kbhit();
    if (tmp  != 0) tmp = 0xff;
    *regl = *regh = *rega = (char)tmp; // hitech-c v3.09-16, need to fill both H and L
    if (bdosdbgflag[11]) printf("bdos11(read console) REGA=0x%02x, REGHL=0x%04x\n", *rega, *reghl);
    ReturnZ80;
    return;

}
/*----------------------------------------------------------------------*/
void bdos12(void)	    /* get version number */
{
    if (bdosdbgflag[12]) printf("bdos12(get version number)\n"); //debug
    *reghl = 0X0022;
    *rega = 0x22;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos13(void)	     /* reset disk system */
{
    if (bdosdbgflag[13]) printf("bdos13(reset disk system)\n"); //debug
    dmaaddr = 0x0080;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos14(void)	     /* select disk */
{
    UINT8 disknumber;
    char path[20];

    disknumber = (UINT8)(*regde & 0x00ff);
    if (bdosdbgflag[14])printf("bdos14(select disk)=%d\n", disknumber); //debug
#ifdef WIN
    sprintf(path, "%c:", (char)disknumber + 'A');
    SetCurrentDirectory(path);
#endif
#ifdef LINUX
    // only one drive in linux FS. No need to implement this part
#endif
    *regh = *regl = *rega = 0x00; // always success
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos15(void)	/* open file by fcb filename */
{
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchUnusedFCBTableIdx(filename);
    if (bdosdbgflag[15])printf("bdos15(open file)=%s,fcbidx=%d\n", filename, fcbidx); //debug
    if (bdosdbgflag[15])dumpfcb(*regde);   // #debug
    if ( fcbidx > MAXFILE ) *regl = *rega = 0xFF;
    else if ( *filename == 0x00 ) *regl = *rega = 0xff; /*Null Filename*/
    else if ( strcasecmp((char *)fcbused[fcbidx], filename) == 0 )
    {
        opensuccess(fcbidx, filename);
        *regl = *rega = 0x00;
    }
    else if ( (fcbfile[fcbidx] = fopen(filename, "r+b")) == NULL )
	{
        *regl = *rega = 0XFF;
		if (bdosdbgflag[15]) printf("open %s, idx %d failed 1\n", filename, fcbidx); // debug
    }
	else
    {
        *regl = *rega = 0x00;
        opensuccess(fcbidx, filename);
    }
    SetFCBSeqOffset(&ram[*regde], 0L);
    *regh = 0x00;
    if (bdosdbgflag[15])printf("bdos15() Return REGA=0x%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos16(void)      /* close file */
{
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchOpenedFCBTableIdx(filename);
    if (bdosdbgflag[16])printf("bdos16(close file)=%s,fcbidx=%d\n", filename, fcbidx); // #debug
    if (fcbidx > MAXFILE) *rega  = 0xff;
    else if (fcbfile[fcbidx] == NULL) *rega = 0xff;
    else
    {
        if (fclose(fcbfile[fcbidx]) == 0)
        {
            *rega = 0x00;
            fcbused[fcbidx][0] = 0;
            fcbfile[fcbidx] = NULL;
        }
        else *rega = 0xff;
    }
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[16]) printf("bdos16() Return REGA=0x%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
UINT8 filesearchingflag = 0;
#ifdef WIN
static   struct _finddata_t c_file;
static   intptr_t hFile;
#endif //Windows

#ifdef LINUX
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
static DIR *dp;
static struct dirent *entry;
static struct stat statbuf;
// compare if input filename match the pattern (include '*' or '?') or not.
// 1 means match, 0 means unmatch
int compare_pattern(char *pattern, char *input_filename)
{
    char adjusted_pattern_fcb[20], filename_fcb[20];
    int i, j, result = 1;

    //printf("Compare_pattern(%s,%s),", pattern, input_filename); // #debug
    // use FCB structure to compare
    fillfcb(filename_fcb, input_filename);
    fillfcb(adjusted_pattern_fcb, pattern);

    // adjust pattern: expand '*' to '?' if exist
    for (i = 1; i < 9; i++)
    {
        if (adjusted_pattern_fcb[i] == '*')
        {
            // expand '*' to '?' till end of filename
            for (j = i; j < 9; j++) adjusted_pattern_fcb[j] = '?';
            break;
        }
    }
    for (i = 9; i < 12; i++)
    {
        if (adjusted_pattern_fcb[i] == '*')
        {
            // expand '*' to '?' till end of fileext
            for (j = i; j < 12; j++) adjusted_pattern_fcb[j] = '?';
            break;
        }
    }
    //dumpmem(adjusted_pattern_fcb,0x10); //debug
    //dumpmem(filename_fcb,0x10); //debug
    // compare filaname vs pattern
    for (i = 1; i < 12; i++)
    {
        if (adjusted_pattern_fcb[i] != '?')
        {
            if(adjusted_pattern_fcb[i] != filename_fcb[i])
            {
                result = 0;
                //printf("not match idx %d,",i);
                break;
            }
        }
    }
    //printf("result=%d\n", result);
    return(result);
}
#endif // Linux
void bdos17(void)	 /* search for first */
{
#ifdef WIN
    char filename[20];
    int findresult;
    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[17])printf("bdos17(search first)=%s\n", filename);

    // repeat search for 1st -> do nothing and return success(for bdsc160 crck.com)

	if ((strchr(filename, '*')==NULL) && (strchr(filename,'?')==NULL)) 
	{
		FILE *fp;
		// filename do not contain '?' or '*'
		if ((fp = fopen(filename,"r"))!=NULL)
		{
			// open filename success -- found
			fclose(fp);
			fillfcb((char *)&ram[dmaaddr], filename);
			*rega = 0x00;
		}
		else
		{
			*rega = 0xff;
		}
    }	
    // Find first file (filename contain '?' or '*') in current directory
    else if( (hFile = _findfirst( filename, &c_file )) == -1L )
    {
        // file not found
        *rega = 0xff;
    }
    else
    {
        // file found (attrib _A_SUBDIR is invalid for CP/M, ignore it and get next one)
        if (bdosdbgflag[17])printf("0x%02x <%s>\n", c_file.attrib, c_file.name);
		if (!(c_file.attrib & _A_SUBDIR))
		{
			// not subdirectory, valid 8+3 filename -> found
			if (isvalidcpmfilename(c_file.name))
			{
				// found next file
				filesearchingflag = 1;
				fillfcb((char *)&ram[dmaaddr], c_file.name);
				*rega = 0x00;
			}
		}
		else
		{
			while ((findresult = _findnext( hFile, &c_file )) == 0)
			{
				// file found (attrib _A_SUBDIR is invalid for CP/M. ignore it and get next one)
				if (bdosdbgflag[17])printf("0x%02x <%s>\n", c_file.attrib, c_file.name);
				if ( c_file.attrib & _A_SUBDIR ) continue; // skip subdirectories
				// valid 8+3 filename -> leave while loop
				if (isvalidcpmfilename(c_file.name)) break;
			}
			if (findresult == 0)
			{
				// found next file
				filesearchingflag = 1;
				fillfcb((char *)&ram[dmaaddr], c_file.name);
				*rega = 0x00;
			}
			else
			{
				// Can't find next file
				_findclose( hFile );
				// Searching complete
				filesearchingflag = 0;
				*rega = 0xff;
			}
		}
	}
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[17])printf("bdos17() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
#endif //Windows
#ifdef LINUX
    char filename[20];
    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[17])printf("bdos17(search first)=%s\n", filename);

    // repeat search for 1st -> do nothing and return success(for bdsc160 crck.com)	
	if ((strchr(filename, '*')==NULL) && (strchr(filename,'?')==NULL)) 
	{
		FILE *fp;
		// filename do not contain '?' or '*'
		if ((fp = fopen(filename,"r"))!=NULL)
		{
			// open filename success
			fclose(fp);
			fillfcb((char *)&ram[dmaaddr], filename);
			*rega = 0x00;
		}
		else
		{
			*rega = 0xff;
		}
    }
	else if ((dp = opendir(".")) == NULL)
    {
		if (bdosdbgflag[17])printf("bdos17() opendir() fail\n");
        *rega = 0xff; // can not open dir --> should not happen
    }
    else
    {
        while ((entry = readdir(dp)) != NULL)
        {
			//if (bdosdbgflag[17]) printf("<%s>", entry->d_name); // #debug
            lstat(entry->d_name, &statbuf);
            if (bdosdbgflag[17]) printf("0x%02x <%s>\n", statbuf.st_mode, entry->d_name);
            if (S_ISDIR(statbuf.st_mode)) continue;
            // invalid 8+3 filename -> skip
			if (!isvalidcpmfilename(entry->d_name)) continue;
            // match wildcard pattern -> leave loop
            if (compare_pattern(filename, entry->d_name)) break; // leave loop if file in directory match pattern
        }
        if (entry != NULL)
        {
            fillfcb((char *)&ram[dmaaddr], entry->d_name);
            filesearchingflag = 1; // set file searching flag
            *rega = 0x00;
        }
        else
        {
            closedir(dp);
            *rega = 0xff; // empty directory
        }
    }
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[17])printf("bdos17() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
#endif //Linux
}
/*----------------------------------------------------------------------*/
void bdos18(void)	 /* search for next */
{
#ifdef WIN
    char filename[20];
	int findresult;
    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[18])printf("bdos18(search next)=%s\n", filename);

    if(filesearchingflag == 0)
    {
        // Not in searching (find first is not called before, return fail)
        *regh = 0x00;
        *regl = *rega = 0xff;
		if (bdosdbgflag[18])printf("bdos18() not in searching, REGA=0x%02x\n", *rega);
		ReturnZ80;
        return;
    }
    while ((findresult = _findnext( hFile, &c_file )) == 0)
    {
        // file found (attrib _A_SUBDIR is invalid for CP/M. ignore it and get next one)
        if (bdosdbgflag[18])printf("0x%02x <%s>\n", c_file.attrib, c_file.name);
        if ( c_file.attrib & _A_SUBDIR ) continue; // skip subdirectories
		// valid 8+3 filename -> leave while loop
		if (isvalidcpmfilename(c_file.name)) break;
	}
	if (findresult == 0)
	{
        // found next file
        fillfcb((char *)&ram[dmaaddr], c_file.name);
        *rega = 0x00;
    }
    else
    {
        // Can't find next file
        _findclose( hFile );
        // Searching complete
        filesearchingflag = 0;
        *rega = 0xff;
    }
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[18])printf("bdos18() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
#endif //Windows

#ifdef LINUX
    char filename[20];
    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[18])printf("bdos18(search next)=%s\n", filename);
    if(filesearchingflag == 0)
    {
        // Not in searching (find first is not called before, return fail)
        *regh = 0x00;
        *regl = *rega = 0xff;
		if (bdosdbgflag[18])printf("bdos18() not in searching, REGA=0x%02x\n", *rega);		
        ReturnZ80;
        return;
    }
    while ((entry = readdir(dp)) != NULL)
    {
        lstat(entry->d_name, &statbuf);
        if (bdosdbgflag[18])printf("0x%02x <%s>\n", statbuf.st_mode, entry->d_name);
        if (S_ISDIR(statbuf.st_mode)) continue;
        // invalid 8+3 filename -> skip
		if (!isvalidcpmfilename(entry->d_name)) continue;
        // need to implement '*' and '?' filename comparision here (linux only show *.*)
        if (compare_pattern(filename, entry->d_name)) break; // leave loop if file in directory match pattern
    }
    if (entry != NULL)
    {
        fillfcb((char *)&ram[dmaaddr], entry->d_name);
        *rega = 0x00;
    }
    else
    {
        closedir(dp);
        // Searching complete
        filesearchingflag = 0;
        *rega = 0xff; // end of directory
    }
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[18])printf("bdos18() Return REGA=0x%02x\n", *rega);

    ReturnZ80;
    return;
#endif // Linux
}
/*----------------------------------------------------------------------*/
void bdos19(void)		 /* delete file */
{
#ifdef WIN
    UINT8 fcbidx = 0;
    struct _finddata_t c_file;
    intptr_t hFile;
    UINT8 deleted = 0xff;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[19])printf("bdos19(delete file)=%s\n", filename); //#debug

	if ((strchr(filename, '*')==NULL) && (strchr(filename,'?')==NULL)) 
	{
		// filename do not contain '?' or '*'
		
		// Search fcb table and close if file is already opened
		fcbidx = SearchOpenedFCBTableIdx(filename);
		// already opened? close it before delete.
		if (bdosdbgflag[19])printf("bdos19() fcbidx=%d, fcbfilename=%s, fcbfilelen=%u\n", fcbidx, fcbused[fcbidx], fcbfilelen[fcbidx]); // debug
		if (fcbidx <= MAXFILE)
		{
			//printf("bdos19() delete an opened file. close it before delete!\n"); //#debug
			if (fclose(fcbfile[fcbidx]) != 0) printf("Oops! can't close file before delete!\n");
			fcbused[fcbidx][0] = 0;
			fcbfile[fcbidx] = NULL;
		}
		// delete file
		if (bdosdbgflag[19])printf("delete %s\n", filename); // #debug
		if (remove(filename) == 0) deleted = 0x00;
    }
	else if( (hFile = _findfirst( filename, &c_file )) != -1L )
    {
        // find 1st file at least
        do
        {
			// only delete valid cpm 8+3 files
			if (isvalidcpmfilename(c_file.name))
			{
				// Search fcb table and close if file is already opened
				fcbidx = SearchOpenedFCBTableIdx(c_file.name);
				// already opened? close it before delete.
				if (bdosdbgflag[19])printf("bdos19() fcbidx=%d, fcbfilename=%s, fcbfilelen=%u\n", fcbidx, fcbused[fcbidx], fcbfilelen[fcbidx]); // debug
				if (fcbidx <= MAXFILE)
				{
					//printf("bdos19() delete an opened file. close it before delete!\n"); //#debug
					if (fclose(fcbfile[fcbidx]) != 0) printf("Oops! can't close file before delete!\n");
					fcbused[fcbidx][0] = 0;
					fcbfile[fcbidx] = NULL;
				}
				// delete file
				if (bdosdbgflag[19])printf("delete %s\n", c_file.name); // #debug
				if (remove(c_file.name) == 0) deleted = 0x00;
			}
        }
        while( _findnext( hFile, &c_file ) == 0 );
        _findclose( hFile );
    }
    *rega = deleted;
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[19])printf("bdos19() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
#endif //Windows

#ifdef LINUX
    // need to implement search_first and search_next since filename may include '*' or '?'
    // This is not implemented in previous portings.
    UINT8 fcbidx = 0;
    UINT8 deleted = 0xff;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);
    if (bdosdbgflag[19])printf("bdos19(delete file)=%s\n", filename); //#debug

	if ((strchr(filename, '*')==NULL) && (strchr(filename,'?')==NULL)) 
	{
		// filename do not contain '?' or '*'
		
		// Search fcb table and close if file is already opened
		fcbidx = SearchOpenedFCBTableIdx(filename);
		// already opened? close it before delete.
		if (bdosdbgflag[19])printf("bdos19() fcbidx=%d, fcbfilename=%s, fcbfilelen=%u\n", fcbidx, fcbused[fcbidx], fcbfilelen[fcbidx]); // debug
		if (fcbidx <= MAXFILE)
		{
			//printf("bdos19() delete an opened file. close it before delete!\n"); //#debug
			if (fclose(fcbfile[fcbidx]) != 0) printf("Oops! can't close file before delete!\n");
			fcbused[fcbidx][0] = 0;
			fcbfile[fcbidx] = NULL;
		}
		// delete file
		if (bdosdbgflag[19])printf("delete %s\n", filename); // #debug
		if (remove(filename) == 0) deleted = 0x00;
    }
	else
	{
		// filename contains '?' or '*'
		if((dp = opendir(".")) == NULL)
		{
			// printf("cannot open directory: %s/n",dir);
			// can't open current directory, return with fail
			if (bdosdbgflag[19])printf("bdos19 opendir() fail\n"); //#debug
			*regl = *rega = 0xff;
			*regh = 0x00;
			return;
		}
		while((entry = readdir(dp)) != NULL)
		{
			lstat(entry->d_name, &statbuf);
			if (bdosdbgflag[19])printf("bdos19 <0x%x><%s>\n", statbuf.st_mode, entry->d_name); //#debug		
			if (S_ISDIR(statbuf.st_mode)) continue;
		
			// search file only, skip directory and non-cpm format files
			//printf("%*s%s\n",depth,"",entry->d_name);
			if (!compare_pattern(filename, entry->d_name)) continue; // loop next if file in directory not match pattern
		
			// invalid 8+3 filename -> skip
			if (!isvalidcpmfilename(entry->d_name)) continue;
		
			// check if file is already opened
			fcbidx = SearchOpenedFCBTableIdx(entry->d_name);
			if (bdosdbgflag[19])printf("bdos19() fcbidx=%d, fcbfilename=%s, fcbfilelen=%u\n", fcbidx, fcbused[fcbidx], fcbfilelen[fcbidx]); // debug
			// already opened? close it before delete.
			if (fcbidx <= MAXFILE)
			{
				//printf("bdos19() delete an opened file. close it before delete!\n");
				if (fclose(fcbfile[fcbidx]) != 0) printf("Oops! can't close file before delete!\n");
				fcbused[fcbidx][0] = 0;
				fcbfile[fcbidx] = NULL;
			}
			// delete file
			if (bdosdbgflag[19])printf("delete %s\n", entry->d_name); // #debug
			if (remove(entry->d_name) == 0) deleted = 0x00;
		}
	}
    *rega = deleted;
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)

    if (bdosdbgflag[19])printf("bdos19() Return REGA=0x%02x\n", *rega);

    ReturnZ80;
    return;
#endif //Linux
}
/*----------------------------------------------------------------------*/
void bdos20(void)				/* squential read */
{
    UINT8 i;
    UINT32 l, offset;
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);
    fcbidx = SearchOpenedFCBTableIdx(filename);
    offset = GetFCBSeqOffset(&ram[*regde]);
    if (bdosdbgflag[20])printf("bdos20(read squential)=%s OFFSET=%u, FCBFileLen=%u\n", filename, offset, fcbfilelen[fcbidx]); // debug
    if ( offset  >= fcbfilelen[fcbidx] ) *rega = 0x01;
    else if ( fcbidx > MAXFILE) *rega = 0xff;
    else if ( fseek(fcbfile[fcbidx], offset, SEEK_SET) != 0 ) *rega = 0xff;
    /* read fail */
    else
    {
        fread(&ram[dmaaddr], 1, 128, fcbfile[fcbidx]); // read into CP/M DMA buffer directly
        l =  fcbfilelen[fcbidx] - offset;
        if (l < 128L)
        {
            for (i = (UINT8)l; i < 128; i++)
            {
                ram[dmaaddr + i] = 0x1a;
            }
        }
        *rega = 0x00;
        SetFCBSeqOffset(&ram[*regde], offset + 128);
        if (bdosdbgflag[20])dumpram(dmaaddr, 128); //debug
    }
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[20])printf("bdos20() Return REGA=%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos21(void)		/* write sequential */
{
    UINT8 fcbidx = 0;
    UINT32 offset;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);
    fcbidx = SearchOpenedFCBTableIdx(filename);
    offset = GetFCBSeqOffset(&ram[*regde]);
    if (bdosdbgflag[21])printf("bdos21(write squential)=%s OFFSET=%u, FCBFileLen=%u\n", filename, offset, fcbfilelen[fcbidx]); // debug

    if ( fcbidx > MAXFILE)
    {
        *rega = 0xff;
        ReturnZ80;
    }
    else if ( fseek(fcbfile[fcbidx], offset, SEEK_SET)  != 0 )
    {
        *rega = 0xff; //ReturnZ80;  /* write fail */
    }
    else
    {
        SetFCBSeqOffset(&ram[*regde], offset + 128);
        if (offset >= fcbfilelen[fcbidx])
            fcbfilelen[fcbidx] = offset + 128;
        fwrite(&ram[dmaaddr], 1, 128, fcbfile[fcbidx]); // write from DMA buffer directly (w/o copy)
        *rega = 0x00;
        if (bdosdbgflag[21])dumpram(dmaaddr, 128); //debug
    }
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[21])printf("bdos21() Return REGA=%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos22(void)		/* make file by fcb filename */
{
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchUnusedFCBTableIdx(filename);
    if (bdosdbgflag[22])printf("bdos22(make file)=%s, fcbidx=%d\n", filename, fcbidx); // #debug

    if ( fcbidx > MAXFILE )  *rega = 0xFF;
    else if ( strcasecmp((char *)fcbused[fcbidx], filename) == 0 )
    {
        opensuccess(fcbidx, filename);
        *rega = 0x00;
    }
    else if ( (fcbfile[fcbidx] = fopen(filename, "w+b")) == NULL )
        * rega = 0XFF;
    else
    {
        *rega = 0x00;
        opensuccess(fcbidx, filename);
    }
    SetFCBSeqOffset(&ram[*regde], 0L);
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[22])printf("bdos22() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos23(void)		/*  rename file  */
{
    char oldname[15];
    char newname[15];

    if (bdosdbgflag[23])printf("bdos23(rename file)\n"); // #debug
    fcbtofilename((char *)&ram[*regde], oldname);
    fcbtofilename((char *)&ram[*regde + 16], newname);

    *rega = (char)rename(oldname, newname);
    if (*rega != 0x00) *rega = 0xff;
    *regh = 0x00;
    *regl = *rega;
    if (bdosdbgflag[23])printf("bdos23() Return REGA=0x%02x\n", *rega);
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos24(void)		/* return login vector ---not checked */
{
    if (bdosdbgflag[24])printf("bdos24(return login vector)\n"); // #debug
    *reghl = 0x0007;
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos25(void)		/* return currect disk -- not checked! */
{
#ifdef WIN
    TCHAR Buffer[BUFSIZE];
    DWORD dwRet;
    if (bdosdbgflag[25])printf("bdos25(get current disk)\n"); // #debug
    dwRet = GetCurrentDirectory(BUFSIZE, Buffer);

    if( dwRet == 0 )
    {
        printf("GetCurrentDirectory failed (%lu)\n", GetLastError());
        return;
    }
    *rega = Buffer[0] - 'A';
    *regl = *rega;
    *regh = 0x00;
    ReturnZ80;
    return;
#endif // Windows
#ifdef LINUX
    // No need to implement since there is no drive letter support in Linux FS.
    *regl = *rega = 0x02; //Always C:
    *regh = 0x00;
    ReturnZ80;
    return;
#endif // LINUX
}
/*----------------------------------------------------------------------*/
void bdos26(void)		/* set dma address */
{
    if (bdosdbgflag[26])printf("bdos26() set dma adddress:"); // #debug
    dmaaddr = *regde;
    if (bdosdbgflag[26])printf("%04X\n", dmaaddr); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos27(void)		/* get alloc address */
{
    if (bdosdbgflag[27])printf("bdos27(get alloc addr)\n"); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos28(void)		/* write protect disk */
{
    // all disks are R/W. So, no action for this call.
    if (bdosdbgflag[28])printf("bdos28(write protect disk)\n"); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos29(void)		/* get r/o vector */
{
    // all disks are R/W. So, return 0 for this vector
    if (bdosdbgflag[29])printf("bdos29(get r/o vector)\n"); // #debug
    *reghl = 0x0000; // all drives with no R/O bit set(all drives are R/W)
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos30(void)		/* set file attributes */
{
    if (bdosdbgflag[30])printf("bdos30(set file attrib)\n"); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos31(void)		/* get disk parms */
{
    if (bdosdbgflag[31])printf("bdos31(get disk parms)\n"); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos32(void)		/* get and set user code --not ready */
{
    if (bdosdbgflag[32])printf("bdos32(get/set usercode) E=0x%02x, user_code=0x%02x\n", *rege, user_code);
    if (*rege == 0xff)
    {
        *regl = *rega = user_code;
        *regh = 0x00;
    }
    else user_code = *rege & 0x1f;
    if (bdosdbgflag[32])printf("bdos32() user_code=0x%02x\n", user_code);
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos33(void)		/* read random */
{
    UINT8 i;
    UINT32 l, offset;
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchOpenedFCBTableIdx(filename);
    offset = GetFCBRandOffset(&ram[*regde]);
    if (bdosdbgflag[33])printf("bdos33(read random)=%s OFFSET=%u, FCBFileLen=%u\n", filename, offset, fcbfilelen[fcbidx]); // debug
    if (bdosdbgflag[33])dumpfcb(*regde); // #debug

    if ( (char) * (fcbln + *regde + 2) != 0x00) *rega = 0x06; // CPM2: overflow
    else if ( offset > fcbfilelen[fcbidx] ) *rega = 0x04;
    else if ( fcbidx > MAXFILE) *rega = 0x05;
    else if ( fseek(fcbfile[fcbidx], offset, SEEK_SET) != 0 ) *rega = 0x03;
    /* seek fail */
    else
    {
        fread(&ram[dmaaddr], 1, 128, fcbfile[fcbidx]);
        l =  fcbfilelen[fcbidx] - offset;
        if ( l < 128L )
        {
            for (i = (UINT8)l; i < 128; i++)
            {
                ram[dmaaddr + i] = 0x1a;
            }
        }
        if ( offset == fcbfilelen[fcbidx] ) *rega = 0x01;
        else *rega = 0x00;
        SetFCBSeqOffset(&ram[*regde], offset); // update sequential r/w position
        if (bdosdbgflag[33])dumpram(dmaaddr, 128); //debug
    }
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[33])printf("bdos33() Return REGA=%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos34(void)		/* write random */
{
    UINT8 fcbidx = 0;
    UINT32 offset;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchOpenedFCBTableIdx(filename);
    offset = GetFCBRandOffset(&ram[*regde]);
    if (bdosdbgflag[34])printf("bdos34(write random)=%s OFFSET=%u, FCBFileLen=%u\n", filename, offset, fcbfilelen[fcbidx]); // debug
    if (bdosdbgflag[34])dumpfcb(*regde); //#debug
    if ( (char) * (fcbln + *regde + 2) != 0x00)
    {
        *rega = 0x06;    // CPM2: overflow (S2!=0)
    }
    else if ( fcbidx > MAXFILE)
    {
        *rega = 0x05;
    }
    else if ( fseek(fcbfile[fcbidx], offset, SEEK_SET) != 0 )
    {
        *rega = 0x03;    /* seek fail */
    }
    else
    {
        *rega = 0x00;
        fwrite(&ram[dmaaddr], 1, 128, fcbfile[fcbidx]);
        if (offset >= fcbfilelen[fcbidx])
            fcbfilelen[fcbidx] = offset + 128;
        SetFCBSeqOffset(&ram[*regde], offset);
        if (bdosdbgflag[34])dumpram(dmaaddr, 128); //debug
    }
    *regh = 0x00;
    *regl = *rega; // HL also return A (refer to RUNCPM)
    if (bdosdbgflag[34])printf("bdos34() Return REGA=0x%02x\n", *rega); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos35(void)		/* compute file size -- not checked*/
{
    UINT8 fcbidx = 0;
    char filename[20];

    fcbtofilename((char *)&ram[*regde], filename);

    fcbidx = SearchUnusedFCBTableIdx(filename);
    if (bdosdbgflag[35])printf("bdos35(compute filesize)=%s, FCBFileLen=%u\n", filename, fcbfilelen[fcbidx]); // debug

    if ( (fcbfile[fcbidx] = fopen(filename, "r+b")) != NULL )
    {
        opensuccess(fcbidx, filename);
        SetFCBRandOffset(&ram[*regde], fcbfilelen[fcbidx]);
    }
    if (bdosdbgflag[35])printf("bdos35() Return\n"); //debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos36(void)		/* set random record --not checked--have error!*/
{
    if (bdosdbgflag[36])printf("bdos36(set random record)\n"); // #debug

    SetFCBRandOffset(&ram[*regde], GetFCBSeqOffset(&ram[*regde]));
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos37(void)		/* reset drive */
{
    if (bdosdbgflag[37])printf("bdos37(reset drive)\n"); // #debug
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
void bdos38(void)		/* write random (zero) */
{
    ReturnZ80;
    return;
}
/*----------------------------------------------------------------------*/
/* Process CPM BDOS Services API                                        */
/*----------------------------------------------------------------------*/
void cpmbdos(void)
{
    if (DebugFlag == 1 && lpt != NULL) PrintDebug();
    //if ((*regc != 2)&&(*regc != 6)) printf("bdos(%d) enter:", *regc); // debug
    switch ( *regc )
    {
    case 0:
        bdos00();
        break;       /* system reset */
    case 1:
        bdos01();
        break;       /* console input */
    case 2:
        bdos02();
        break;       /* console output */
    case 3:
        bdos03();
        break;       /* reader input */
    case 4:
        bdos04();
        break;       /* punch output */
    case 5:
        bdos05();
        break;       /* list output */
    case 6:
        bdos06();
        break;       /* direct console i/o */
    case 7:
        bdos07();
        break;       /* get iobyte */
    case 8:
        bdos08();
        break;       /* set iobyte */
    case 9:
        bdos09();
        break;       /* print string */
    case 10:
        bdos10();
        break;      /* read console buffer */
    case 11:
        bdos11();
        break;      /* get console status */
    case 12:
        bdos12();
        break;      /* get version number */
    case 13:
        bdos13();
        break;      /* reset disk system */
    case 14:
        bdos14();
        break;      /* select disk */
    case 15:
        bdos15();
        break;      /* open file */
    case 16:
        bdos16();
        break;      /* close file */
    case 17:
        bdos17();
        break;      /* search for first */
    case 18:
        bdos18();
        break;      /* search for next */
    case 19:
        bdos19();
        break;      /* delete file */
    case 20:
        bdos20();
        break;      /* read sequential */
    case 21:
        bdos21();
        break;      /* write sequential */
    case 22:
        bdos22();
        break;      /* make file */
    case 23:
        bdos23();
        break;      /* rename file */
    case 24:
        bdos24();
        break;      /* return login vector */
    case 25:
        bdos25();
        break;      /* return current disk */
    case 26:
        bdos26();
        break;      /* set dma address */
    case 27:
        bdos27();
        break;      /* get alloc address */
    case 28:
        bdos28();
        break;      /* write protect disk */
    case 29:
        bdos29();
        break;      /* get r/o vector */
    case 30:
        bdos30();
        break;      /* set file attributes */
    case 31:
        bdos31();
        break;      /* get disk parms */
    case 32:
        bdos32();
        break;      /* get & set user code */
    case 33:
        bdos33();
        break;      /* read random */
    case 34:
        bdos34();
        break;      /* write random */
    case 35:
        bdos35();
        break;      /* compute file size */
    case 36:
        bdos36();
        break;      /* set random record */
    case 37:
        bdos37();
        break;      /* reset drive */
    case 40:
        bdos34();
        break;      /* write random (zero) */ // According to RunCPM, 40=34 (for smallc21 ar.com)
    default:
        printf("unknown BDOS call %d\n", *regc);
        *reghl = 0;
        break;
    }
    //if ((*regc != 2)&&(*regc != 6)) printf("bdos(%d) leave.\n", *regc); // debug
}
