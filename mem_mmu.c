/* yaze - yet another Z80 emulator.
   Copyright (C) 1995,1998  Frank D. Cringle.

   Module MEM_MMU.[hc] Copyright (C) 1998/1999 by Andreas Gerlich (agl)


This file is part of yaze - yet another Z80 emulator.

Yaze is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef Z80_YAZE110
  #include "mem_mmu_yaze110.h"	// yaze-110 Z80 Engine
#else
  #include "mem_mmu.h"			// yaze-ag Z80 Engine
#endif

//#ifdef DJGPP
BYTE ram[MEMSIZE*1024];         /* the whole memory space */
//#endif

//#ifdef MSC
//  BYTE ram[65530];
//#endif

#ifdef MMU /* <------------------------- only if MMU is selected ------------ */

#include "simz80.h"		/* for the definitions of the Z80 registers */

pagetab_struct MMUtable[MMUTABLES];	/* MMU page tables (default 8)        */
pagetab_struct *mmu;		       /* Pointer to selected MMU-pagetable  */
pagetab_struct *dmmu = &MMUtable[0];  /* Pointer to destination MMU-pagetbl */
pagetab_struct *mmuget,*mmuput;	     /* Pointer for get/put		   */
int mmutab;			    /* choosen MMU-pagetable              */

/*------------------------------------------- initMEM ------------------*/
void
initMEM()
{
    int p;

    for (p=0; p<RAMPAGES; p++) {	   /* initialize all Pages and	  */
	memset(PP(p), p, PAGESIZE*1024 ); /* fill the page with the No.	 */
					 /* of the page			*/
/*
	#ifdef MMUTEST
	printf("init page %3d,  ",p);
	#endif
*/
    }
/*
    #ifdef MMUTEST
    puts("");
    #endif
*/
} /* END of initMEM */

/*------------------------------------------- initMMU ------------------*/
void
initMMU()
{
    int c,m;

    /* initialice whole MMU-table */
    /* points all page points of any table to the first 64 KByte of RAM */
    for (m=0; m<MMUTABLES; m++)
	for (c=0; c<(MMUPAGEPOINTERS); ++c)
		MMUtable[m].page[c] = PP(c);

    #ifdef MMUTEST
	/* for testing choose bank 7 */
	if (MMUTABLES <7)
		ChooseMMUtab(1);
	else
		ChooseMMUtab(7);
	if (MEMSIZE >= 192) {
		mmu->page[11] = PP(16);	/* B000H point to 10000H */
	 	mmu->page[10] = PP(32);	/* A000H point to 20000H */
		mmu->page[ 9] = PP(19);	/* 9000H point to 13000H */
		mmu->page[ 8] = PP(18);	/* 8000H point to 12000H */
		mmu->page[ 7] = PP(0x1E); /* 7000H point to 1E000H */
		mmu->page[ 6] = PP(0x1D); /* 6000H point to 1D000H */
		mmu->page[ 5] = PP(47); /* 5000H point to 2F000H */
		mmu->page[ 4] = PP(44); /* 4000H point to 2C000H */
		mmu->page[ 3] = PP(1);	/* 3000H point to 2A000H */
		mmu->page[ 1] = PP(42);	/* 3000H point to 2A000H */
		/* mmu->page[ 0] = PP(41);	\* 3000H point to 29000H */
	}

	/***** extra test for the MMU-Tables
	for (m=0; m<MMUTABLES; m++) {
	   printf("MMUTEST: MMUtable[%d]\n\n",m);
	   for (c=0; c<64/4; ++c) {
	      printf("MMUtable[%d].page[%2d] = 0x%X",
                                m,        c,      MMUtable[m].page[c]-ram);
	      printf("\t(0x%lX)\n",(long unsigned int) MMUtable[m].page[c]);
	   }
	   printf("\nMMUTEST: END of MMUtable[%d]:\n\n",m);
	}
	*****/
    #else
	ChooseMMUtab(0);	/* choose mmutable 0 (default) */
    #endif

    #ifdef MMUTEST
      #ifndef SHOWMMU
	#define SHOWMMU
      #endif
    #endif

    #ifdef SHOWMMU
	printMMU();  /* */
    #else
	printf("RAM: %d KByte, %d KByte PAGESIZE, %d PAGES\n",
		MEMSIZE,   PAGESIZE,     RAMPAGES);
	printf("MMU: %d TABLES, %d PAGEPOINTERS per TABLE, ",
		MMUTABLES, MMUPAGEPOINTERS);
	printf("selected MMU-PAGETABLE: %d\n\n", mmutab);
    #endif

} /* END of initMMU() */


/*------------------------------------------- loadMMU ------------------
  load a MMU-Table with the PP's which are given by a Table, which are
  adressed by HL. The structure of the table is :

		first-Byte: adr of MMUtable
		2..16:	    16 bytes which will translated in pointers
			    and put in to the MMUTab.

	   return-codes (reg A & HL):
	   	A = 0	  All is OK.
		A = 0xFE: PagePointer is wrong (out of Memory). HL points
			  to the wrong PP.
		A = 0xFF: MMUtable (first Byte) numbers an MMUtable which
			  does not exist.
*/

/* Z80 registers */
#define AF	af[af_sel]
#define HL	regs[regs_sel].hl

void
loadMMU()
{
    static pagetab_struct * p_mmu;
    static int i,h_mmut,page;

    if ( (h_mmut = GetBYTE_pp(HL)) < MMUTABLES ) {
	Sethreg(AF, 0x00);	/* 0x00 default OK */
	p_mmu = &MMUtable[ h_mmut ];	/* get pointer to Table */
	for (i=0; i<MMUPAGEPOINTERS; i++)
	    if ( (page = GetBYTE_pp(HL)) < RAMPAGES ) {
		p_mmu->page[i] = PP( page );
	    } else {
		Sethreg(AF, 0xfe);	/* 0xfe not OK */
		HL--;		/* Points HL-Register to the wrong byte */
		#ifdef MMUTEST
		 printf("\r\nYAZE: Load-MMU-Table %d : Page No. %d for "
			"the %d. pagepointer is wrong!\r\n",h_mmut,page,i);
		 printf("                         posible max. number "
			"is %d\n",RAMPAGES-1);
		#endif
		break; /* for */
	    }
    } else {
	Sethreg(AF, 0xff);	/* 0xff not OK */
	HL--;		/* Points HL-Register to the wrong byte */
	#ifdef MMUTEST
	printf("\r\nYAZE: Load-MMU-Table %d <-- Number for the selected "
		"table is out of range!\r\n",h_mmut);
	#endif
    }
} /* END of loadMMU() */


/*------------------------------------------- printMMU -----------------*/
void
printMMU()
{
    int c,m,i;
     printf("RAM: %d KByte, %d KByte PAGESIZE, %d PAGES\r\n",
		MEMSIZE,   PAGESIZE,     RAMPAGES);
     printf("MMU: %d TABLES, %d PAGEPOINTERS per TABLE, ",
		MMUTABLES, MMUPAGEPOINTERS);
     printf("selected MMU-PAGETABLE: %d\r\n\n", mmutab);

     puts("Z80-\\      MT0      MT1      MT2      MT3      MT4      MT5      "
	  "MT6      MT7\r\n"
	#if MMUTABLES>8
          "      MT8      MT9      MT10     MT11     MT12     MT13     "
	  "MT14     MT15\r\n"
	#endif
          "ADDR \\-----------------------------------------------------------"
	  "---------------\r");
     for (c=0; c<64/4; ++c) {
	printf("%04X :",c*(PAGESIZE*1024));
	for (m=0; m<MMUTABLES; m++) {
		if (m == mmutab) printf("->");
		else if (m != mmutab+1) printf("  ");
		if ((i=MMUtable[m].page[c]-ram) <= 0xFFFF ) {
			printf("  %04X ",i);
		} else {
			printf(" %5X ",i);
		}
		if (m == mmutab) printf("<-");
	}
	puts("\r");
     }
     puts("-----------------------------------------------------------------"
	  "---------------\r");
} /* END of printMMU() */

#endif /* MMU <------------------------- only if MMU is selected ------------ */

/*--------------------------- END of Module mem_mmu.c -----------------------*/
