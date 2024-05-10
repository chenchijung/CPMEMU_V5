/************************************************************************/
/*                                                                      */
/*             CP/M Hardware Emulator Card Support Program              */
/*                         CPMGLOB.C Ver 1.51                           */
/*                 Copyright (c) By C.J.Chen NTUEE 1988                 */
/*                         All Right Reserved                           */
/*                                                                      */
/************************************************************************/

#ifndef _CPMGLOB_H
#define _CPMGLOB_H

/* Z80 Registers backup for BDOS/BIOS Call */
extern UINT8 *bioscode;
extern UINT8 *bdoscall;
extern UINT8 *eop;

extern UINT8 *rega;         extern UINT8 *regb;
extern UINT8 *regc;         extern UINT8 *regd;       extern UINT8 *rege;
extern UINT8 *regh;         extern UINT8 *regl;

extern UINT16 *regaf;       extern UINT16 *regbc;     extern UINT16 *regde;
extern UINT16 *reghl;       extern UINT16 *regix;     extern UINT16 *regiy;
extern UINT16 *regsp;       extern UINT16 *regip;

/* FCB Pointers for BDOS call */
extern UINT8 *fcbdn;        extern UINT8 *fcbfn;      extern UINT8 *fcbft;
extern UINT8 *fcbrl;        extern UINT8 *fcbrc;      extern UINT8 *fcbcr;
extern UINT8 *fcbln;

extern UINT16 dmaaddr;


extern jmp_buf ctrl_c;

/* File handler array */
extern FILE *fcbfile[MAXFILE];
extern UINT32 fcbfilelen[MAXFILE];
extern UINT8 fcbused[MAXFILE][15];
extern FILE *lpt;
extern FILE *stream;
extern FILE *subfile;
extern UINT8 xsubflag;
extern UINT8 bdosdbgflag[BDOSDBGFLAGS]; // bdos debug print flag
extern int ctrlc_flag;

extern char DebugFlag;
extern UINT8 lastcall;
extern UINT16 repeats;
extern char debugmess1[];
extern char debugmess2[];
extern UINT8 in_bios;

extern char Z80DebugFlag;
extern FILE *Z80Trace;

extern const char cpmhex[];

extern UINT8 ram[];

#endif /* ifdef _CPMGLOB_H */
