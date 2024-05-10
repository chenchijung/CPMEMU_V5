/************************************************************************/
/*                                                                      */
/*             CP/M Hardware Emulator Card Support Program              */
/*                         CPMEMU.C Ver 1.60                            */
/*                 Copyright (c) By C.J.Chen NTUEE 1988                 */
/*                         All Right Reserved                           */
/*                                                                      */
/************************************************************************/
#include <sys/types.h>
#include <stdint.h>
#include <signal.h>

#ifndef _CPMEMU_H

#define _CPMEMU_H

#define   interrupt
#define   cdecl

#define   PROGRAM_NAME   "\nDavid's CP/M 2.2 Emulator and Z80 CPU Simulator Ver 5.0\n"
#define   COPYRIGHT      "Z80 CPU Simulator by Frank D. Cringle.\n"
#define   COPYRIGHT1     "CP/M 2.2 Emulator by David Chen, 1988,1989,2001.\n"
#define   COPYRIGHT2     "Porting to Linux by David Chen, 2014.\nPorting to Win32/64 by David Chen, 2024.\n"

#define   MAXFILE         250
#define   NULLSIZE        0x40
#define	  BDOSDBGFLAGS    45
#define   CPMCMDBUF       (254)
#define   kMaxArgs        45

#define   GotoPC
#define   ResetZ80      resetz80()
#define   ReturnZ80

#define   REGA            (ram+0XFF83)
#define   REGB            (ram+0XFF85)
#define   REGC            (ram+0XFF84)
#define   REGD            (ram+0XFF87)
#define   REGE            (ram+0XFF86)
#define   REGH            (ram+0XFF89)
#define   REGL            (ram+0XFF88)

#define   REGAF           (ram+0XFF82)
#define   REGBC           (ram+0XFF84)
#define   REGDE           (ram+0XFF86)
#define   REGHL           (ram+0XFF88)
#define   REGIX           (ram+0XFF8A)
#define   REGIY           (ram+0XFF8C)
#define   REGSP           (ram+0XFF8E)
#define   REGIP           (ram+0XFF90)
#define   BIOSCODE        (ram+0XFF92)
#define   BDOSCALL        (ram+0XFF81)
#define   EOP             (ram+0XFF80)
/*   CP/M SYSTEM FILE CONTROL BLOCK  */
#define   FCBDN           (ram+0X0000)
#define   FCBFN           (ram+0X0001)
#define   FCBFT           (ram+0X0009)
#define   FCBRL           (ram+0X000C)
#define   FCBRC           (ram+0X000F)
#define   FCBCR           (ram+0X0020)
#define   FCBLN           (ram+0X0021)


typedef uint8_t        UINT8;
typedef uint16_t       UINT16;
typedef uint32_t       UINT32;
typedef int8_t         SINT8;
typedef int16_t        SINT16;
typedef int32_t        SINT32;


/* CPMEMU.C */
void checknullptr(void);
void quit(void);
void resetz80(void);
void waitz80(void);
void fillfcb(char *,char *);
void store_stack_segment(void);
char *gear_fgets(char *, int, FILE *, int);
void getstring(char *, int);
void chkclosesubfile(void);

/* CPMBDOS.C */
void initialbdos(void);
void cpmbdos(void);
void dumpmem(UINT8 *, UINT16);
void dumpram(UINT16, UINT16);

/* CPMBIOS.C */
void initialbios(void);
void cpmbios(void);

/* CPMEMUIN.C */
//void handle_ctrl_c(sig_t s);

/* SIMZ80.C */
//UINT32 simz80(UINT16);
//UINT16 GetPC(void);
//void SetPC(UINT16);

#endif /* #ifdef _CPMEMU_H */
