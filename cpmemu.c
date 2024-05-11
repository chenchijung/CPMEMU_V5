/************************************************************************/
/*                                                                      */
/*             CP/M Hardware Emulator Card Support Program              */
/*                         CPMEMU.C Ver 1.51                            */
/*                 Copyright (c) By C.J.Chen NTUEE 1988                 */
/*                         All Right Reserved                           */
/*                                                                      */
/************************************************************************/
// # dchen 2014.5
// Porting to 32 bits and 64 bits Unix, and using Posix standard data types
// to avoid porting trouble.
//
// # dchen 2024.2
// Porting back to windows & fix bios console status/fopen for .com file/turbo pascal display
//
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <unistd.h>

#ifdef GNU_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef WIN
#include <windows.h>
#include <synchapi.h>  // for Windows Sleep() function
#include <conio.h> // Windows _kbhit()
#include <direct.h>
#include <sysinfoapi.h> // windows API timer GetTickCount()

#define KEY_ESCAPE  27
#define KEY_BACKSPACE 8
#define KEY_ENTER 13
#define KEY_F1 315
#define KEY_F2 316
#define KEY_F3 317
#define KEY_F4 318
#define KEY_F5 319
#define KEY_F6 320
#define KEY_F7 321
#define KEY_F8 322
#define KEY_F9 323
#define KEY_F10 324
#define KEY_F11 389
#define KEY_F12 390
#define KEY_UP	 328
#define KEY_DOWN 336
#define KEY_LEFT 331
#define KEY_RIGHT 333
#define KEY_PAGEUP 329
#define KEY_PAGEDOWN 337
#define KEY_HOME 327
#define KEY_END 335
#define KEY_INSERT 338
#define KEY_DELETE 339

/*---------------------------------------------
    GetKey() -- Get key press & translate WS4/TP3 editor related keys

    Thanks John Wagner
*---------------------------------------------*/
char pending_flag = 0;
int GetKey(void)
{
    static int pending_char;
    int c;

    if (pending_flag)
    {
        pending_flag = 0;
        return pending_char;
    }

    c = _getch();
    if((c == 0) || (c == 224))
    {
        c = 256 + _getch(); /* If extended key (like F10), add 256. */
    }
    if (c < 256) return c;
    // special key -> key convert to Wordstar/Turbo Pascal 3 for CP/M text editors
    switch (c)
    {
    case KEY_UP:
        c = 0x05;
        break;
    case KEY_DOWN:
        c = 0x18;
        break;
    case KEY_LEFT:
        c = 0x13;
        break;
    case KEY_RIGHT:
        c = 0x04;
        break;
    case KEY_PAGEUP:
        c = 0x12;
        break;
    case KEY_PAGEDOWN:
        c = 0x03;
        break;
    case KEY_INSERT:
        c = 0x16;
        break;
    case KEY_DELETE:
        c = 0x07;
        break;
    case KEY_HOME:
        pending_flag = 1;
        pending_char = 'R';
        return 0x11;	// ^Q R
    case KEY_END:
        pending_flag = 1;
        pending_char = 'C';
        return 0x11;	// ^Q C
    default:
        c = 0x00;
        break; // unsupported chars
    }
    return c;
}
// empty fuctions -- in order to reduce #ifdef LINUX
void _console_init(void)
{
}
void _console_set(void)
{
}
void _console_reset(void)
{
}
#endif //Windows

#ifdef LINUX
#include <unistd.h>
inline int32_t max(int32_t a, int32_t b)
{
    return((a) > (b) ? a : b);
}
inline int32_t min(int32_t a, int32_t b)
{
    return((a) < (b) ? a : b);
}
#define Sleep(x) usleep(1000*x)
#define DWORD unsigned long
extern void handle_ctrl_c(int signo); // for ctrl-c

/* Console abstraction functions */
/*===============================================================================*/
#include <termios.h>
#include <unistd.h>
#include <poll.h>
static struct termios _old_term, _new_term;

void _console_init(void)
{
    tcgetattr(0, &_old_term);

    _new_term = _old_term;

    _new_term.c_lflag &= ~ICANON; /* Input available immediately (no EOL needed) */
    _new_term.c_lflag &= ~ECHO; /* Do not echo input characters */
    //_new_term.c_lflag &= ~ISIG; /* ^C and ^Z do not generate signals */
    _new_term.c_iflag &= INLCR; /* Translate NL to CR on input */

    tcsetattr(0, TCSANOW, &_new_term); /* Apply changes immediately */

    setvbuf(stdout, (char *)NULL, _IONBF, 0); /* Disable stdout buffering */
}
// console setting for program execution
void _console_set(void)
{
    //printf("_console_set() -- for program execution\n"); // debug
    tcsetattr(0, TCSANOW, &_new_term);
}
// console setting for command line input
void _console_reset(void)
{
    //printf("_console_reset() -- for cmd input\n");
    tcsetattr(0, TCSANOW, &_old_term);
}

int _kbhit(void)
{
#if 0
    struct pollfd pfds[1];

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN | POLLPRI | POLLRDBAND | POLLRDNORM;

    return (poll(pfds, 1, 0) == 1) && (pfds[0].revents & (POLLIN | POLLPRI | POLLRDBAND | POLLRDNORM));
#else
    int pending_input(void);
    return pending_input();
#endif
}

int _getch(void)
{
#if 0
    return getchar();
#else
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
#endif
}

void _putch(const char ch)
{
    putchar(ch);
}

int _getche(void)
{
    int ch = _getch();

    _putch(ch);

    return ch;
}

#include <errno.h>
int pending_input(void)
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}
char pending_flag = 0;
// Get Key press and translate WS4/TP3 related editor keys
int GetKey()
{
    int nread, i;
    char c;
    static int pending_char;

    if (pending_flag)
    {
        pending_flag = 0;
        return pending_char;
    }

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN) printf("read error!");
    }
    // non ESC: reply read results
    if (c != '\x1b') return c;

    //ESC : check ESC sequences
    char seq[7];
    seq[0] = seq[1] = seq[2] = seq[3] = seq[4] = seq[5] = seq[6] = 0x00;

    // Read till there is no keycode inside (read entire ESC code sequence)
    for (i = 0; i < 7; i++)
    {
        if (!pending_input()) break;
        nread =  read(STDIN_FILENO, &seq[i], 1);
    }
    //printf("\n{<ESC>%c%c%c%c%c%c%c}", seq[0], seq[1], seq[2], seq[3], seq[4], seq[5], seq[6]);

    // Parse ESC sequence for KEY
    if (seq[0] == '[')
    {
        if (seq[1] >= '0' && seq[1] <= '9')
        {
            if (seq[2] == '~')
            {
                switch (seq[1])
                {
                case '1':
                    //printf("<HOME>");
                    pending_flag = 1;
                    pending_char = 'R';
                    return 0x11;	// ^Q R
                case '2':
                    //printf("<INS>");
                    return 0x16;
                //return INS_KEY;
                case '3':
                    //printf("<DEL>");
                    return 0x07;
                //return DEL_KEY;
                case '4':
                    //printf("<END>");
                    pending_flag = 1;
                    pending_char = 'C';
                    return 0x11;	// ^Q C
                //return END_KEY;
                case '5':
                    //printf("<PGUP>");
                    return 0x12;
                //return PAGE_UP;
                case '6':
                    //printf("<PGDN>");
                    return 0x03;
                //return PAGE_DOWN;
                case '7':
                    //printf("<HOME>");
                    pending_flag = 1;
                    pending_char = 'R';
                    return 0x11;	// ^Q R
                //return HOME_KEY;
                case '8':
                    //printf("<END>");
                    pending_flag = 1;
                    pending_char = 'C';
                    return 0x11;	// ^Q C
                    //return END_KEY;
                }
            }
            else
            {
                if (seq[3] == '~')
                {
                    if (seq[1] == '1')
                    {
                        switch (seq[2])
                        {
                        case '5':
                            //printf("<F5>");
                            return '\x1b';
                        case '7':
                            //printf("<F6>");
                            return '\x1b';
                        case '8':
                            //printf("<F7>");
                            return '\x1b';
                        case '9':
                            //printf("<F8>");
                            return '\x1b';
                        }
                    }
                    else if (seq[1] == '2')
                    {
                        switch (seq[2])
                        {
                        case '0':
                            //printf("<F9>");
                            return '\x1b';
                        case '1':
                            //printf("<F10>");
                            return '\x1b';
                        case '3':
                            //printf("<F11>");
                            return '\x1b';
                        case '4':
                            //printf("<F12>");
                            return '\x1b';
                        }
                    }
                }
            }
        }
        else
        {
            switch (seq[1])
            {
            case 'A':
                //printf("<UP>");
                return 0x05;
            //return ARROW_UP;
            case 'B':
                //printf("<DOWN>");
                return 0x18;
            //return ARROW_DOWN;
            case 'C':
                //printf("<RIGHT>");
                return 0x04;
            //return ARROW_RIGHT;
            case 'D':
                //printf("<LEFT>");
                return 0x13;
            //return ARROW_LEFT;
            case 'H':
                //printf("<HOME>");
                pending_flag = 1;
                pending_char = 'R';
                return 0x11;	// ^Q R
            //return HOME_KEY;
            case 'F':
                //printf("<END>");
                pending_flag = 1;
                pending_char = 'C';
                return 0x11;	// ^Q C
                //return END_KEY;
            }
        }
    }
    else if (seq[0] == 'O')
    {
        switch (seq[1])
        {
        case 'H':
            //printf("<HOME>");
            pending_flag = 1;
            pending_char = 'R';
            return 0x11;	// ^Q R
        //return HOME_KEY;
        case 'F':
            //printf("<END>");
            pending_flag = 1;
            pending_char = 'C';
            return 0x11;	// ^Q C
        //return END_KEY;
        case 'P':
            //printf("<F1>");
            return '\x1b';
        case 'Q':
            //printf("<F2>");
            return '\x1b';
        case 'R':
            //printf("<F3>");
            return '\x1b';
        case 'S':
            //printf("<F4>");
            return '\x1b';
        }
    }
    return '\x1b';
}
/*----------------------------------------------------------------------*/
// try to implement WINDOW's GetTickCount()
unsigned long GetTickCount(void)
{
    struct timespec ts;
    unsigned long TickCount = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
        TickCount = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }
    return (TickCount);
}
#endif //Linux
/*----------------------------------------------------------------------*/
#include "cpmemu.h"
#include "cpmglob.h"

#ifdef Z80_YAZE110
#include "simz80_yaze110.h"	// yaze-110 z80 engine
#else
#include "mem_mmu.h"		// yaze-ag z80 engine
#include "simz80.h"		// yaze-ag z80 engine
#include "ytypes.h"		// yaze-ag z80 engine
#endif

static char *hexptr;
UINT8 halt = 0;
static UINT8 doscommand = 1;

FILE *subfile = NULL;
UINT8 xsubflag = 0; // support for xsub

static UINT8 checknum;
static jmp_buf cold_start;
static UINT32 timer_start, timer_stop;
static UINT8 timer_flag = 0;

extern WORD pc;

void printtitle(void)
{
    printf(PROGRAM_NAME);
    //printf(COPYRIGHT);
    printf(COPYRIGHT1);
    printf(COPYRIGHT2);
#ifdef Z80_YAZE110
    printf("Z80 CPU Simulator by Frank D. Cringle from YAZE 1.10 https://www.mathematik.uni-ulm.de/users/ag/yaze-ag/\n");
#else
    printf("Z80 CPU Simulator by Frank D. Cringle from YAZE-AG 2.51.3 https://www.mathematik.uni-ulm.de/users/ag/yaze-ag/\n");
#endif
    printf("Z80 Disassembler module by Markus Fritze from https://github.com/sarnau/Z80DisAssembler\n");
#ifdef Z80DDT
	printf("Z80 Assembler module by Petr Kulhavy/Achim Flammenkamp from Z80-ASM 2.4.1 https://wwwhomes.uni-bielefeld.de/achim/z80-asm.html\n");
	printf("Z80DDT module is based on https://github.com/algodesigner/z80 with lots of modifications\n");
#endif
    printf("TPA Area: 0100H - FDFFH\n\n");
    return;
}

int getc_cpmcmd(void)
{
    int ch;
    //printf("getc_cpmcmd()");
    while (1)          // wait key stroke, pause to reduce CPU utilization
    {
        Sleep(4);     // sleep 4ms

        if (ctrlc_flag)
        {
            printf("^C\n");
            ctrlc_flag = 0;    // clear ctrl-c flag
            longjmp(ctrl_c, 0); // process control-c event
        }
        if (_kbhit()) break;    // exit while when key pressed
    }
    ch = _getch(); // fgetc(fp); // read char
    return (ch);
}

int fgets_cpmcmd(char *buf, int num, FILE *fp)
{
    int i;
    int ch;

    //printf("fgets_cpmcmd()\n");
    for (i = 0; i < num - 1;)
    {
        ch = getc_cpmcmd();
        if (ch == 0x0d) // RETURN
        {
            break;          // exit loop
        }
        else if (ch == 0x08) // backspace
        {
            if (i != 0) i--;
            _putch(ch);
        }
        else if (ch == 0x03) // CTRL-C
        {
            longjmp(ctrl_c, 0); // process ctrl-c
        }
        else
        {
            buf[i++] = ch;
            _putch(ch);
        }
        //printf("ch=0x%x i=%d\n", ch, i);
    }
    buf[i] = 0x00; // add or replace '\n' for end of string char
    //printf("<%s>\n",buf);
    return 1;
}

/*----------------------------------------------------------------------*/
/* use fgets to replace gets, and fix additional '\n' */
/* Since gets is not recommands for current C programming */
/*----------------------------------------------------------------------*/
char *gear_fgets(char *buf, int num, FILE *fp, int ignore)
{
    char *find = 0;

    //printf("gear_fgets()\n");
    //if (!fgets(buf, num, fp))
    if (!fgets_cpmcmd(buf, num, fp))
    {
        return NULL;
    }
    if ((find = strrchr(buf, '\n')))
    {
        *find = '\0';
    }
    //else if(ignore)
    //{
    //    char ch;
    //    while (((ch=fgetc(fp)!=EOF)&&( ch!='\n')));
    //}
    return buf;
}
/*----------------------------------------------------------------------*/
/* This part is related to decode HEX and load to simulated Z80's memory space */
/*----------------------------------------------------------------------*/
char hexfile[1400];
char getchfromcpmhex(void)
{
    char ch;
    ch = *hexptr++;
    //putchar(ch); //#debug
    return(ch);
}
/*----------------------------------------------------------------------*/
char readnibble(void)
{
    char tmp;

    tmp = getchfromcpmhex();
    if (tmp > '9') tmp -= 55;
    else tmp -= 48;
    return(tmp);
}
/*----------------------------------------------------------------------*/
unsigned char readbyte(void)
{
    UINT8 result;

    result = (UINT8)readnibble() * 16;
    result += (UINT8)readnibble();
    checknum += result;
    return(result);
}
/*----------------------------------------------------------------------*/
int hex_readline(void)
{
    char ch;
    UINT16 addr = 0;
    unsigned char data_type;
    unsigned char data_number;
    unsigned char i, tmp;
    unsigned char *ptr;

    checknum = 0;
    while (1)
    {
        if ((ch = getchfromcpmhex()) == EOF)
        {
            printf("\ninvalid EOF \n");
            quit();
            return(EOF);
        }
        if (ch == ':') break;
        //{
        //	printf("\nInvalid HEX file!\n");
        //	quit();
        //}
    }
    if (( data_number = readbyte() ) == 0) return(EOF);
    addr = readbyte();
    tmp = readbyte() ;
    addr = addr * 256 + tmp;
    if (( data_type = readbyte() ) == 1) return(EOF);
    else if (data_type != 0)
    {
        printf("\nInvalid data type char.\n");
        quit();
    }
    ptr = (unsigned char *)(&ram[addr]);
    for (i = 1 ; i <= data_number ; i++) *ptr++ = readbyte();
    readbyte();             /* read checknumber */
    getchfromcpmhex();      /* read Line Feed char */
    if (checknum != 0)
    {
        printf("\nCheck Sum error!\n");
        quit();
    }
    return(0);
}
/*----------------------------------------------------------------------*/
void loadcpmhex(void)
{
    int result;
    static char hexfile_loaded = 0;
    static char *hexptr_backup;
    FILE *hexfp = NULL;

    if (!hexfile_loaded) // first time call
    {
        hexfile_loaded = 1;
        if ((hexfp = fopen ("CPMEMUZ8.HEX", "r")) != NULL)
        {
            // only read file once. (otherwise it can not load when CD to other directories!)
            printf("Loading CPMEMUZ8.HEX\n\n");

            fread(hexfile, 1400, 1, hexfp);
            fclose(hexfp);
            hexptr_backup = hexptr = hexfile;
        }
        else
        {
            printf("Loading internal HEX\n\n");
            hexptr_backup = hexptr = (char *)cpmhex;
        }
    }
    else // 2nd, 3rd,... call
    {
        //printf("Loading HEX\n");
        hexptr = hexptr_backup;
    }
    // Start loading
    do
    {
        result = hex_readline();
    }
    while (result != EOF);
}
/*----------------------------------------------------------------------*/
UINT8 loadcom(char *filename)
{
    char *addr = (char *)(&ram[0x0100]);
    unsigned filelen = 0;
    FILE *fp;

    if (strstr(filename, ".COM") == NULL)
    {
        printf("%s is not a Z80 Execution File\n", filename);
        return(1);
    }
    if ((fp = fopen(filename, "rb")) == NULL)
    {
        printf("Z80 Execution File %s not found\n", filename);
        return(1);
    }
    fseek(fp, 0L, SEEK_END);
    filelen = (unsigned)ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    fread(addr, (size_t)1, (size_t)filelen, fp);
    fclose(fp);
    return(0);
}
/*----------------------------------------------------------------------*/
/* Clear all simulated Z80's memory space to 0 */
/*----------------------------------------------------------------------*/
void clearmem(void)
{
    GotoPC;
    memset(ram, 0, 65536);
    return;
}
/*----------------------------------------------------------------------*/
//extern void dumpmem(UINT8 *ram, UINT16 len);
// New implementation (easier to understand, hopefully)
void fillfcb(char *fcb, char *filename)
{
    char *ptr;
    char i;

    //printf("fillfcb(0x%p, 0x%p)\n", fcb, filename);
    //dumpmem(filename, strlen(filename)); // #debug
    // init fcb's filename
    fcb[0] = 0x00;
    memset(&fcb[1], ' ', 11);

    ptr = filename;
    // process head "x:" of drive (fcb[0])
    if (ptr[1] == ':')
    {
        if ((toupper(ptr[0]) >= 'A') && (toupper(ptr[0]) <= 'Z'))
            fcb[0] = toupper(ptr[0]) - 'A' + 1;
        ptr += 2; // shift ptr to head of filename
    }

    // fill file name (w/o ext). (fcb[1]..fcb[8])
    for (i = 0; i < 8; i++)
    {
        if (*ptr == '.' || *ptr == 0x00) break;
        fcb[i + 1] = *ptr++;
    }
    if (i == 8) while(*ptr != '.' && *ptr != 0x00) ptr++; // filename too long, skip till next 0x00 or '.'

    // fill file ext (fcb[9]..fcb[11])
    switch (*ptr)
    {
    case 0x00:
        break; // no ext
    case '.':
        ptr++;	// skip '.'
        for (i = 0; i < 3; i++)
            if (*ptr != 0x00) fcb[i + 9] = *ptr++;
        break;
    }
    //dumpmem(fcb,0x40); //#debug
    return;
}
/*----------------------------------------------------------------------*/
#define SUBMIT_MAX_ARGS 30 // must > 10
void submit(char *ptr1)
{
    char *ptr2;
    UINT8 ch, ch2, ch3;
    char filename[20];
    UINT8 submitcount = 1;
    char *submitptr[SUBMIT_MAX_ARGS];
    char submitstring[CPMCMDBUF];
    int tmp;
    void upcase(char *);
    FILE *subfileout = NULL;

    // if submit cmd inside submit file, follow CPM 2.2 style, this submit cmd
    // can only be the last cmd in order to chain .SUB files. remaining cmds
    // after submit will be discarded. So, Here we delete old $$$$.SUB if exist.
    if (subfile != NULL)
    {
        fclose(subfile);
        remove("$$$$.SUB");
        subfile = NULL;
    }

    for (tmp = 1 ; tmp < SUBMIT_MAX_ARGS ; submitptr[tmp++] = NULL);
    *submitstring = 0x00;
    upcase(ptr1);	// cmd string to uppercase
    // fill filename xxxx.sub
    ptr1 += 6; // skip 'submit'
    while (*ptr1 == ' ') ptr1++; // skip spaces
    ptr2 = filename;
    while (*ptr1 != ' ' && *ptr1 != '.' && *ptr1 != 0x00) *ptr2++ = *ptr1++; // copy file name(till '.', ' ' or 0x00)
    switch (*ptr1)
    {
    case '.':
        while (*ptr1 != ' ' && *ptr1 != 0x00) // copy file extension
            *ptr2++ = *ptr1++;
        *ptr2 = 0x00;
        break;
    case ' ':
        strcpy(ptr2, ".SUB");
        break;			// no extension, add ".SUB"
    case  0 :
        strcpy(ptr2, ".SUB");
        break;			// no extension, add ".SUB"
    }
    // Open xxxx.sub and fill submit $1...$9
    if ((subfile = fopen(filename, "r")) == NULL)
    {
        printf("Submit File %s not found\n", filename);
        return;
    }
    else if (*ptr1 != 0x00)
    {
        ptr1++;
        strncpy(submitstring, ptr1, sizeof(submitstring) - 1);
        ptr1 = submitstring;
        while (submitcount < SUBMIT_MAX_ARGS)
        {
            if (*ptr1 == 0x00) break;
            submitptr[submitcount++] = ptr1;
            while (*ptr1 != ' ' && *ptr1 != 0x00) ptr1++;
            if (*ptr1 == ' ') *ptr1++ = 0x00;
        }
    }
    // debug
    //for (tmp = 1; tmp < SUBMIT_MAX_ARGS; tmp++)
    //{
    //	printf("$%d = '%s'\n", tmp, submitptr[tmp]);
    //}

    // open $$$$.SUB and convert xxxx.sub to $$$$.sub
    //printf("creating $$$$.SUB\n"); //debug
    if ((subfileout = fopen("$$$$.SUB", "w")) == NULL) printf("Can't create $$$$.SUB\n");
    else
    {
        //printf("subfile=%p, subfileout=%p\n", subfile, subfileout); // #debug
        while (!feof(subfile))
        {
            //ch = toupper(getc(subfile));
            ch = getc(subfile);
            //printf("%c", ch); // debug
            if (ch == '$')
            {
                ch2 = getc(subfile);
                if (!isdigit(ch2))
                {
                    putc(ch, subfileout);
                    putc(ch2, subfileout);
                    //putchar(ch); putchar(ch2); // #debug
                }
                else
                {
                    ch3 = getc(subfile);
                    if (!isdigit(ch3))
                    {
                        ptr2 = submitptr[ch2 - '0'];
                        if (ptr2 != NULL)
                        {
                            fputs(ptr2, subfileout);
                            //printf("%s",ptr2); // #debug
                        }
                        putc(ch3, subfileout);
                    }
                    else
                    {
                        int argidx = 10 * (ch2 - '0') + (ch3 - '0');
                        if (argidx < SUBMIT_MAX_ARGS)
                        {
                            ptr2 = submitptr[argidx];
                            if (ptr2 != NULL)
                            {
                                fputs(ptr2, subfileout);
                                //printf("%s",ptr2); // #debug
                            }
                        }
                    }
                }
            }
            else if (ch != 0xFF)
            {
                putc(ch, subfileout);
                //putchar(ch); // #debug
            }
        }
    }
    //putc('\r',subfileout);
    //putc('\n',subfileout); // add additional CRLF to $$$$.SUB
    //printf("---- end of $$$$.SUB ---\n"); //#debug
    fclose(subfile);
    fclose(subfileout);
    subfileout = NULL;
    // Open $$$$.SUB for submit
    if ((subfile = fopen("$$$$.SUB", "r")) == NULL) printf("Can't open $$$$.SUB\n");
    return;
}
void chkclosesubfile(void)
{
    if(subfile == NULL) return;
    if (feof(subfile) != 0)
    {
        fclose(subfile);
        subfile = NULL;
        remove("$$$$.SUB");
        xsubflag = 0; // xsub support
    }
    return;
}
/*----------------------------------------------------------------------*/
/* BDOS and BIOS call Trace                                             */
/*----------------------------------------------------------------------*/
void debug(char *ptr1)
{
    if (strcasecmp(ptr1, "DEBUG ON") == 0)
    {
        printf("Debug is set on\n");
        DebugFlag = 1;
        lastcall = 0xff;
        if (lpt == NULL)
        {
            lpt = fopen("btrace.out", "w");
            //lpt = fopen("stdout","w"); // debug print
            fputs("Times  Z80PC    Z80DE    Z80DMA   FUNCTION\n"
                  , lpt);
            fputs("-----  -----    -----    ------   --------\n"
                  , lpt);
        }
        // add screen output stream debug file
        if (stream == NULL)
        {
            stream = fopen("stream.out", "wb");
        }
    }
    else if (strcasecmp(ptr1, "DEBUG OFF") == 0)
    {
        printf("Debug is set off\n");
        DebugFlag = 0;
        if (lpt != NULL)
        {
            if (lastcall != 0xFF)
            {
                fprintf(lpt, "% 5d  %s%s\n", repeats, debugmess1
                        , debugmess2);
            }
            fputs("--------- END OF BDOS TRACE TABLE -------- "
                  , lpt);
            fclose(lpt);
            lpt = NULL;
        }
        // add screen output stream debug file
        if (stream != NULL)
        {
            fclose(stream);
            stream = NULL;
        }
    }
    else
    {
        if (DebugFlag) printf("Debug is on\n");
        else printf("Debug is off\n");
    }
    return;
}
/*----------------------------------------------------------------------*/
#ifdef Z80DEBUG
void z80debug(char *ptr1)
{
    if (strcasecmp(ptr1, "Z80DBG ON") == 0)
    {
        printf("Z80 Debug is set on\n");
        Z80DebugFlag = 1;
        if (Z80Trace == NULL)
        {
            Z80Trace = fopen("z80trace.out", "w");
            fputs("--------- Start OF Z80 TRACE TABLE --------\n", Z80Trace);

        }
    }
    else if (strcasecmp(ptr1, "Z80DBG OFF") == 0)
    {
        printf("Z80 Debug is set off\n");
        Z80DebugFlag = 0;
        if (Z80Trace != NULL)
        {
            fputs("--------- END OF Z80 TRACE TABLE --------\n", Z80Trace);
            fclose(Z80Trace);
            Z80Trace = NULL;
        }
    }
    else
    {
        if (Z80DebugFlag) printf("Z80 Debug is on\n");
        else printf("Z80 Debug is off\n");
    }
    return;
}
#endif
void printhelp(void)
{
    printf("Supported Internal Commands:\n");
    printf(" HELP: show this help screen\n");
    printf(" DIR or LS <arguments>: list files\n");
    printf(" ERA or DEL or RM <arguments>: remove/delete files\n");
    printf(" MV or MOVE <arguments>: move files\n");
    printf(" REN or RENAME <arguments>: rename files\n");
    printf(" TYPE or CAT <arguments>: show contents of file\n");
    printf(" PWD: show current working directory\n");
    printf(" CD <arguments>: change working directory\n");
    printf(" CP or COPY <arguments>: copy files\n");
    printf(" SUBMIT <sub-file>: CP/M like submit feature. (incompatible with DRI's SUBMIT.COM)\n");
    printf(" XSUB: CP/M like xsub (incompible with DRI's XSUB.COM)\n");
    printf(" BDOSDBG <on|off> <BDOS call no. list>: enable/disable/show BDOS call debug prints\n");
    printf(" COLD!: cold boot CP/M\n");
    printf(" VER: show program version info\n");
    printf(" DEBUG <on|off>: enable/disable debug log files (btrace.out & stream.out)\n");
    printf(" TIME <program> <arguments>: run program and calculate it's elapsed time\n");
    printf(" TIMER <reset|show>: reset/show Timer (for calculate time inside .sub file)\n");
#ifdef Z80DEBUG
    printf(" Z80DBG <on|off> enable/disable Z80 execution trace file dump (z80trace.out)\n");
#endif
#ifdef Z80DDT
    printf(" Z80DDT <program><arguments>: use embedded Z80DDT debugger to run program\n");
#endif
    printf(" Z80DSM <comfile> <prnfile> [entry addr list]: Disassemble .COM file\n");
    printf(" !<command>: run host command\n");
    printf(" EXIT or QUIT: quit program\n");
    return;
}
/*----------------------------------------------------------------------*/
/* Check input string from prompt is a command or a Z80 program         */
/* Command will use system() to run                                     */
/*----------------------------------------------------------------------*/

static DWORD base_time;

void CheckDosCommand(char *ptr)
{
    int cnt = 0, i;
    int argc = 0;
    char *argv[kMaxArgs];
    char output[CPMCMDBUF], buffer[CPMCMDBUF], *p2, *cmd, *strend;
    char *ptr1;

    strncpy(buffer, ptr, sizeof(buffer) - 1);
    ptr1 = buffer;

    // cmd string into argc and argv[]
    strend = ptr1 + strlen(ptr1);
    p2 = strtok(ptr1, " ");
    while (cnt < kMaxArgs)
    {
        if (p2 != NULL)
        {
            argv[cnt] = p2;
            argc++;
        }
        else
        {
            argv[cnt] = strend;
        }
        //     printf("argv[%d] = %s\n", cnt, argv[cnt]);
        cnt++;
        p2 = strtok(0, " ");
    }
    //   printf("argc = %d\n",argc);

    doscommand = 1;
    //printf("CheckDosCommand[%s,len=%d]\n",ptr,strlen(ptr)); // debug
    //for (i = 0; i < strlen(ptr); i++) printf("<0x%02x>",ptr[i]);
    //printf("\n");
    if (strcasecmp(argv[0], "EXIT") == 0)         quit();
    else if (strcasecmp(argv[0], "QUIT") == 0)    quit();
    else if (ptr[0] == '!')
    {
        system(ptr + 1);
        return;
    }
    else if ((ptr[0] == ';') || (ptr[0] == ':') || (ptr[0] == '=')|| (ptr[0] == '#'))
    {
        return;    //comment, no action
    }
    else if (strcasecmp(argv[0], "COLD!") == 0)
    {
        clearmem();
        loadcpmhex();
        longjmp(cold_start, 0);
        return;
    }
    else if (strcasecmp(argv[0], "VER") == 0 )
    {
        printtitle();
        return;
    }
    else if (ptr[0] == '?')
    {
        printhelp();
        return;
    }
    else if (strcasecmp(argv[0], "HELP") == 0)
    {
        printhelp();
        return;
    }
    else if (strcasecmp(argv[0], "DEBUG") == 0)
    {
        debug(ptr);
        return;
    }
    else if (strcasecmp(argv[0], "SUBMIT") == 0)
    {
        submit(ptr);
        return;
    }
    else if (strcasecmp(argv[0], "XSUB") == 0)
    {
        if (subfile != NULL)
        {
            xsubflag = 1;    // XSUB support
            printf("XSUB enabled\n");
            return;
        }
    }
    else if (strcasecmp(argv[0], "Z80DSM") == 0)
    {
        extern void z80dsm(int argc, char *argv[]);
        z80dsm(argc, argv);
        return;
    }
#ifdef Z80DEBUG
    else if (strcasecmp(argv[0], "Z80DBG") == 0)
    {
        z80debug(ptr);
        return;
    }
#endif
    else if (strlen(ptr) == 0) return;
#ifdef WIN
    else if ((strlen(ptr) == 2) && (isalpha(ptr[0])) && (ptr[1] == ':'))
    {
        int drive;
        // change drive (This is work on windows CMD shell, not work on msys2 command shell!
        drive = toupper(ptr[0]) - 'A' + 1;
        if(_chdrive( drive ))
        {
            printf("Drive %s is not available!\n", ptr1);
        }
        return;
    }
#endif //Windows
#ifdef LINUX
    // no need to support this (only one drive in Linux)
#endif
    else if (ptr[0] == 0xFF) return; // workaround for $$$$.SUB end with 0xFF
#ifdef GNU_READLINE
    else if (strcasecmp(argv[0], "LIST") == 0) // list readline buffer
    {
        HIST_ENTRY **list;
        int i;

        list = history_list ();
        if (list)
        {
            for (i = 0; list[i]; i++)
                fprintf (stderr, "%d: %s\r\n", i, list[i]->line);
        }
        return;
    }
#endif
#if 0 // Linux key trans test code
    else if (strcasecmp(argv[0], "KEYTEST") == 0) // list readline buffer
    {
        _console_set(); // console setting for program execution
        printf("Press Q to quit...\n");
        for (;;)
        {
            int key = GetKey();
            printf("Found: %d<0x%02x><%c>\n", key, key, key);
            if (key != -1)
            {
                if ((key == 113) || (key == 81))
                {
                    printf("\nNormal exit\n");
                    break;
                }
            }
        }
        _console_reset(); // console setting for command line
        return;
    }
#endif
    if (strcasecmp(argv[0], "bdosdbg") == 0) // set/clear bdos debug flag
    {
        // format: bdosdbg [on/off] [n1 n2 n3 n4....]
        UINT8 flag;
        switch (argc)
        {
        case 1: // no other arguments, show bdosdbg flags (print current flags)
            printf("BDOS DBG Flag ON:");
            for (i = 0; i < BDOSDBGFLAGS; i++)
                if (bdosdbgflag[i] != 0) printf("%d ", i);
            printf("\n");
            break;
        case 2: // all on or all off
            if (strcasecmp(argv[1], "on") == 0)
                for (i = 0; i < BDOSDBGFLAGS; i++) bdosdbgflag[i] = 1;
            else if (strcasecmp(argv[1], "off") == 0)
                for (i = 0; i < BDOSDBGFLAGS; i++) bdosdbgflag[i] = 0;
            break;
        default:
            if (strcasecmp(argv[1], "on") == 0)
                flag = 1;
            else if (strcasecmp(argv[1], "off") == 0)
                flag = 0;
            else
                break;
            for (i = 2; i < argc; i++)
                if (atoi(argv[i]) < BDOSDBGFLAGS)
                    bdosdbgflag[atoi(argv[i])] = flag;
            break;
        }
        return;
    }
    DWORD curr_time, timer_ms;
    if (strcasecmp(argv[0], "TIMER") == 0)
    {
        if (strcasecmp(argv[1], "RESET") == 0)
        {
            base_time = GetTickCount();
            printf("Timer Reset, Base Time:%ld ms\n", base_time);
        }
        if (strcasecmp(argv[1], "SHOW") == 0)
        {
            curr_time = GetTickCount();
            timer_ms = curr_time - base_time;
            printf("Current Time:%ld ms, Duration %.3f seconds\n", curr_time, (float)timer_ms / 1000.0f);
        }
        return;
    }
#ifdef WIN
    if (strcasecmp(argv[0], "DIR") == 0) cmd = "dir";
    else if (strcasecmp(argv[0], "LS") == 0) cmd = "dir";
    else if (strcasecmp(argv[0], "CP") == 0)   cmd = "copy";
    else if (strcasecmp(argv[0], "COPY") == 0)   cmd = "copy";
    else if (strcasecmp(argv[0], "REN") == 0)    cmd = "rename";
    else if (strcasecmp(argv[0], "RENAME") == 0) cmd = "rename";
    else if (strcasecmp(argv[0], "MV") == 0)     cmd = "move";
    else if (strcasecmp(argv[0], "MOVE") == 0)    cmd = "move";
    else if (strcasecmp(argv[0], "DEL") == 0)  cmd = "del";
    else if (strcasecmp(argv[0], "RM") == 0)   cmd = "del";
    else if (strcasecmp(argv[0], "ERA") == 0)  cmd = "del";
    else if (strcasecmp(argv[0], "TYPE") == 0) cmd = "type";
    else if (strcasecmp(argv[0], "CAT") == 0)  cmd = "type";
    else if (strcasecmp(argv[0], "PWD") == 0)  cmd = "cd";
#endif //Windows
#ifdef LINUX
    if (strcasecmp(argv[0], "DIR") == 0) cmd = "ls -l --group-directories-first";
    else if (strcasecmp(argv[0], "LS") == 0) cmd = "ls";
    else if (strcasecmp(argv[0], "CP") == 0)   cmd = "cp";
    else if (strcasecmp(argv[0], "COPY") == 0)   cmd = "cp";
    else if (strcasecmp(argv[0], "REN") == 0)    cmd = "mv";
    else if (strcasecmp(argv[0], "RENAME") == 0) cmd = "mv";
    else if (strcasecmp(argv[0], "MV") == 0)     cmd = "mv";
    else if (strcasecmp(argv[0], "MOVE") == 0)    cmd = "mv";
    else if (strcasecmp(argv[0], "DEL") == 0)  cmd = "rm";
    else if (strcasecmp(argv[0], "RM") == 0)   cmd = "rm";
    else if (strcasecmp(argv[0], "ERA") == 0)  cmd = "rm";
    else if (strcasecmp(argv[0], "TYPE") == 0) cmd = "cat";
    else if (strcasecmp(argv[0], "CAT") == 0)  cmd = "cat";
    else if (strcasecmp(argv[0], "PWD") == 0)  cmd = "pwd";
#endif // Linux
    else if (strcasecmp(argv[0], "CD") == 0)
    {
        if (argc == 1) cmd = "cd";
        else
        {
            if(chdir(argv[1]) != 0) printf("Path %s not exist!\n", argv[1]);
            return;
        }
    }
    else
    {
        doscommand = 0;
        return;
    }

    strncpy(output, cmd, sizeof(output) - 1);
    for (i = 1; i < argc; i++)
    {
        strncat(output, " ", sizeof(output) - 1);
        strncat(output, argv[i], sizeof(output) - 1);
    }
    system(output);
    printf("\n");
    return;
}
/*----------------------------------------------------------------------*/
void upcase(char *ptr1)
{
    while (*ptr1 != 0x00)
    {
        *ptr1 = toupper(*ptr1);
        ptr1++;
    }
    return;
}
/*----------------------------------------------------------------------*/
void getstring(char *filename, int size)
{
    char tempname[CPMCMDBUF];
    int name_len;

    //printf("getstring()\n"); //debug
    filename[0] = 0x00;	// clear filename first
    name_len = min(size - 1, CPMCMDBUF - 1);
    if (subfile != NULL)
    {
        /* Submit Batch command File Processing */
        fgets(filename, name_len, subfile);
        printf("%s\n", filename);
        chkclosesubfile();
    }
    else
    {
        /* Normal Command Input */
        /* gets(filename); */
        tempname[0] = 0x00;
        if (gear_fgets(tempname, name_len, stdin, 1) != NULL)
        {
            strncpy(filename, tempname, name_len);
        }
        printf("\n");
    }
    return;
}
/*----------------------------------------------------------------------*/
void getcommand(void)
{
    char filename[CPMCMDBUF];
    char first_file[20];
    char *ptr1, *ptr2;
    UINT8 i;
    char dir_now[256];

    int cnt;
    int argc;
    char *argv[kMaxArgs];
    char *p2, *strend;

    if (bdosdbgflag[0])printf("getcommand()\n"); //#debug
#ifdef LINUX
    system("tput cvvis"); // show cursor
#endif
    do
    {
        do
        {
            if (getcwd(dir_now, 250) == NULL) strcpy(dir_now, "Unknown");

#ifdef GNU_READLINE
            if (subfile == NULL)
            {
                /* Use GNU_Readline Library, support command line history */
                char prompt[256];
                filename[0] = 0x00;
                rl_getc_function = (Function *)getc_cpmcmd; // change getc function to handle ctrl-c;
                //rl_getc_function = getc_cpmcmd;
                rl_catch_signals = 0;
                rl_catch_sigwinch = 0;
                sprintf(prompt, "Z80 %s>", dir_now);
                ptr1 = readline(prompt);
                if (ptr1 != NULL)
                {
                    if (*ptr1) add_history(ptr1);
                    strncpy(filename, ptr1, sizeof(filename) - 1);
                    free(ptr1);
                }
                //else filename[0]=0x00;
            }
            else
            {
                printf("Z80 %s>", dir_now);
                getstring(filename, sizeof(filename));
            }
#else
            /* Do not use GNU_Readline Library. No history support */
            printf("Z80 %s>", dir_now);
            getstring(filename, sizeof(filename));
#endif
            if (bdosdbgflag[0]) dumpmem((UINT8 *)filename, strlen(filename)); //printf("getcommand(%s)\n",filename); //#debug
            if (stream != NULL)
            {
                fprintf(stream, "Z80 %s>", dir_now);
                fputs(filename, stream);   // log command to stream.out
                fputs("\n", stream);
            }
            ptr1 = filename;
            //while (*ptr1 == ' ') ptr1++; // skip head spaces

            // remove 0x0d/0x0a
            for (i = 0; ptr1[i] != 0x00; i++)
            {
                if ((ptr1[i] == 0x0d) || (ptr1[i] == 0x0a)) ptr1[i] = 0x00;
            }
            CheckDosCommand(ptr1);
            upcase(ptr1);
        }
        while (doscommand);

        // measure single application elapsed time feature
        if (strncasecmp(ptr1, "TIME ", 5) == 0)
        {
            // Time xxxx .... (to measure the elapsed time of this application)
            ptr1 += 5; // skip "TIME "
            timer_start = GetTickCount();
            timer_flag = 1;
            printf("\nTimer Start, base:%u ms\n", timer_start);
        }

#ifdef Z80DDT
        // Z80DDT internal debugger
        extern void z80_ddt_init(void);
        extern char z80ddt_flag;
        if (strncasecmp(ptr1, "Z80DDT ", 7) == 0)
        {
            ptr1 += 7;
            z80ddt_flag = 1;
            printf("\nZ80DDT Embedded Debugger\n");
            z80_ddt_init();
        }
#endif //Z80DDT
        // #dchen 20240407
        // Rewrite this part by strtok(). I think these original hard-coded ptr code is not bug free
        // use strchr/strtok can make program much simpler and bug free.

        // fill ram[0x81] ~ ram[0xFF] with rest arguments and ram[0x80] with length
        memset(&ram[0x80], 0x00, 128); 	// Clear RAM 0x80-0xFF
        if ((ptr2 = strchr(ptr1, ' ')) != NULL)
        {
            // with space in argument --> argc >= 1 -> copy remaining arguments to ram[0x80]
            //while (*ptr2 == ' ') ptr2++; // skip these spaces (may not be one space char)
            // #dchen20240410 need to keep at least one space char to make small-c-floats runs correctly.
            strncpy((char *)&ram[0x81], ptr2, 126); // max length 126 bytes (0x80-0x7E), 0x7F for end string 00.
            ram[0x80] = (UINT8)strlen((char *)&ram[0x81]);
        }
        if (bdosdbgflag[0]) dumpram(0x80, 128); // #debug

        // fill argc and argvs
        cnt = 0;
        argc = 0;
        strend = ptr1 + strlen(ptr1);
        p2 = strtok(ptr1, " ");
        while (cnt < kMaxArgs)
        {
            if (p2 != NULL)
            {
                argv[cnt] = p2;
                argc++;
            }
            else
            {
                argv[cnt] = strend;
            }
            if (bdosdbgflag[0])printf("argv[%d] = %s\n", cnt, argv[cnt]); // #debug
            cnt++;
            p2 = strtok(0, " ");
        }
        if (bdosdbgflag[0])printf("argc = %d\n", argc); // #debug

        strcpy(first_file, argv[0]);
        if(strchr(first_file, '.') == NULL)
        {
            strcat(first_file, ".COM");
        }
    }
    while (loadcom(first_file) != 0);   //load 'first_file'.COM file
    fillfcb((char *)(&ram[0x005c]), argv[1]);
    fillfcb((char *)(&ram[0x006c]), argv[2]);
    if (bdosdbgflag[0])dumpram(0x5c, 0x30); //#debug

    ResetZ80;        /* pull reset line to low */
    ReturnZ80;        /* set z80 to raad RAM */
    ResetZ80;   /* pull reset line to high ,z80 begin running from PC=0x0000 */
    simz80(pc); //waitz80(); skip 1st fake bdos/bios call in WARMBOOT->CALLPC (CPMEMUZ80.Z80)
    ReturnZ80;
    simz80(pc); //waitz80(); skip 2nd fake bdos/bios call in MAIN (CPMEMUZ80.Z80)
    halt = *eop = 0; // Now, program will really start (enter 0x0100) in next simz80() call
    return;
}
/*----------------------------------------------------------------------*/
/* Program Initialize Part                                              */
/*----------------------------------------------------------------------*/
void initpointer(void)               /* initial pointers value */
{
    rega = (UINT8 *)REGA;  /* initial 8 bits register pointers */
    regb = (UINT8 *)REGB;
    regc = (UINT8 *)REGC;
    regd = (UINT8 *)REGD;
    rege = (UINT8 *)REGE;
    regh = (UINT8 *)REGH;
    regl = (UINT8 *)REGL;

    regaf = (UINT16 *)REGAF; /* initial 16 bits register pointers */
    regbc = (UINT16 *)REGBC;
    regde = (UINT16 *)REGDE;
    reghl = (UINT16 *)REGHL;
    regix = (UINT16 *)REGIX;
    regiy = (UINT16 *)REGIY;
    regip = (UINT16 *)REGIP;
    regsp = (UINT16 *)REGSP;

    eop = (UINT8 *)EOP;
    bioscode = (UINT8 *)BIOSCODE;
    bdoscall = (UINT8 *)BDOSCALL;

    fcbdn = (UINT8 *)FCBDN; /* initial FCB block pointers */
    fcbfn = (UINT8 *)FCBFN;
    fcbft = (UINT8 *)FCBFT;
    fcbrl = (UINT8 *)FCBRL;
    fcbrc = (UINT8 *)FCBRC;
    fcbcr = (UINT8 *)FCBCR;
    fcbln = (UINT8 *)FCBLN;
    return;
}
/*----------------------------------------------------------------------*/
void initial(void)
{
    initpointer();
    initialbdos();
    initialbios();
    printtitle();

    //subfile = fopen("$$$$.SUB", "r");
    base_time = GetTickCount();

    _console_init();

    // for Z80DSM module Init: point DSM Opcode space to simz80's ram.
    extern UINT8 *Opcodes;
    Opcodes = ram;
    return;
}

/*----------------------------------------------------------------------*/
/* Reset Simulated Z80 CPU (move PC to 0x0000)                          */
/*----------------------------------------------------------------------*/
void resetz80(void)
{
    SetPC(0x0000);	/* initialize pc */
    /* There should be some other registers to be initialized */
    return;
}
/*----------------------------------------------------------------------*/

void quit(void)
{
    if (DebugFlag ==  1 && lpt !=  NULL)
    {
        if (lastcall  !=  0xFF)
        {
            fprintf(lpt, "% 5d  %s%s\n", repeats, debugmess1
                    , debugmess2);
        }
        fputs("----- END OF BDOS TRACE TABLE ----- ", lpt);
        fclose(lpt);
    }

    _console_reset();

    exit(0);
    return;
}
/*----------------------------------------------------------------------*/
void closeall(void)
{
    UINT8 fcbptr;

    for (fcbptr = 0 ; fcbptr < MAXFILE ; fcbptr++)
    {
        if (fcbfile[fcbptr] != NULL) fclose(fcbfile[fcbptr]);
        fcbfile[fcbptr] = NULL;
        fcbused[fcbptr][0] = 0x00;
        fcbfilelen[fcbptr] = 0x0000;
    }
    return;
}
/*----------------------------------------------------------------------*/
/* Main Loop                                                            */
/*----------------------------------------------------------------------*/
extern UINT8 filesearchingflag;// flag for bdos17(search for 1st) and bdos18(search for next)
extern char z80ddt_flag;
int main(void)
{
    initial();
    setjmp(cold_start);
    ctrlc_flag = 0;
#ifdef LINUX
    signal (SIGINT, handle_ctrl_c);
#endif // LINUX

#ifdef WIN    // Windows
    BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);

    if (SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        //printf("\nThe Ctrl-C Control Handler is installed.\n");
        ctrlc_flag = 0; // Clear Ctrl-C flag
    }
#endif  // Windows

    while(1)
    {
        clearmem();
        setjmp(ctrl_c);
        ctrlc_flag = 0;
        z80ddt_flag = 0;
        chkclosesubfile();
        while (1)
        {
            GotoPC;
            loadcpmhex();       /* Load .HEX to RAM everytime befor running a program */
            closeall();         /* Clear opened files in last Z80 program (for turbo pascal!) */
            _console_reset(); // console setting for command line
            getcommand();       /* Read a Command and Loading to RAM. */
            _console_set(); // console setting for program execution
            dmaaddr = 0x0080;
            filesearchingflag = 0; // clear filesearchingflag (for BDOS findfirst/findnext)
            while(1)
            {
                simz80(pc); //waitz80();
                if (halt) break;
                if (*bdoscall) cpmbdos();
                else cpmbios();
                if (*eop) break;
                // ctrl-c break (per bdos/bios API call)
                if (ctrlc_flag)
                {
                    ctrlc_flag = 0;
                    printf("^C\n");
                    longjmp(ctrl_c, 0);
                    break;
                }
            }
            halt = 0;
            printf("\n");

            // Time xxxx .... (to measure the elapsed time of this application)
            if (timer_flag)
            {
                timer_stop = GetTickCount();

                timer_flag = 0;
                printf("\nTimer Stop, base:%u ms, Elapsed Time:%.3f Seconds\n", timer_stop, (timer_stop - timer_start) / 1000.0f);
            }
#ifdef Z80DDT
            // Z80DDT execution ends
            extern char z80ddt_flag;
            if (z80ddt_flag)
            {
                z80ddt_flag = 0;
            }
#endif //Z80DDT			
        }
    }
    return 0;
}
