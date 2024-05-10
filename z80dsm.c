/***
  * Z80 Disassembler
  *
  * This little disassembler for Z80 code was created in an afternoon.
  * There is no user interface! Size of the ROM to be disassembled and
  * Any further jumps must be changed directly in the program code!!!
  *
  * It can be translated under Think C 5.0 on the Macintosh. Who doesn't
  * Macintosh has, on the one hand, may convert the umlauts in the source code, and
  * — if you don't have a C++ compiler — change the comments from \\.
  *
  * An ANSI library (file functions) is also required. But also that
  * can be easily changed (see main()). The “EPROM” file is always stored here
  * loaded. Of course you can change...
  *
  * The program consists of two parts:
  * 1. Analysis of the program. The program starts from the various
  * Walked through hardware vectors of the Z80 (RST commands, NMI) and all jumps
  * executed by a recursive subprogram (ParseOpcodes). Become there
  * found opcodes marked in an array (OpcodesFlags). Also addresses that
  * used as jump targets are marked there. The disassembler
  * can later see exactly whether he has data or program code in front of him!
  * Of course there are exceptions that he cannot recognize:
  * a) self-modifying code. Something like that should normally be the case in a ROM
  * does not occur.
  * b) calculated jumps with JP (IY), JP (IX) or JP (HL). Here too you can
  * the parser does not recognize the jumps. You end up in the MacsBug if
  * such a jump was found. If you set the DEBUGGER symbol to 0,
  * has peace...
  * c) Jump tables. Unfortunately, these occur quite often. Only solution:
  * Disassemble and view the program. If you look at the jump tables
  * has found, you can - like with my Futura aquarium computer ROMs
  * happen — add more ParseOpcodes() calls. How and where that
  * goes, is in main()
  * d) Unuser code. Code that is never started will of course fail
  * the analysis was not found. As a rule, it is not about such code
  * it's a shame :-) However, the "unused" code is often stored via a jump table
  * jumped! So be careful!
  * 2. Disassembling the program. With the help of those generated during parsing
  * A listing is now created for the OpcodesFlags table. The Disassemble subroutine
  * is unfortunately quite "elongated". It disassembles an opcode starting at an address
  * in ROM into a buffer. I wrote it down in one piece (by hand
  * an opcode list). In particular, the management of IX and IY can be done safely
  * shorten significantly...
  *
  * The OpcodeLen() subprogram determines the length of an opcode in bytes. It
  * is required during parsing and during disassembly.
  *
  * By the way, the disassembler knows _no_ hidden opcodes of the Z80. I had
  * no table about it. In my case they weren't even necessary... whoever?
  * If you have a list, you can add it to the disassembler.
  *
  * By the way, if a subprogram expects an "address" in the Z80 code, so is
  * This means an _offset_ on the array with the code! They are NOT pointers!
  * Longs are unnecessary, by the way, because a Z80 only has 64K...
  *
  * In main(), instead of disassembling with labels, you can also do one with
  * Set address and hexdump before opcode. Very practical to avoid possible errors
  * found in the disassembler or when creating a variable list.
  *
  *
  * The program is freeware. It may _not_ be used as the basis for a commercial one
  * Product to be taken! I assume no liability for any damage caused directly
  * or arise indirectly through the use of this program!
  *
  * If you want to reach me, the best way to do so is in our company mailbox:
  *
  * Sigma Soft Mailbox
  * ©1992 ∑-Soft, Markus Fritze
 
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
//#include <portab.h>

#define CODESIZE        65024L          // 64K Code Space - 512bytes(FE00-FFFF)
#define FUTURA_189      0               // Jump table jumps for Futura aquarium computer ROM V1.89
#define DEBUGGER        0               // if 1, then you end up with calculated
                                        // Jumps in the debugger. See also above.
#include <sys/types.h>
#include <stdint.h>										
typedef uint8_t        UBYTE;
typedef uint16_t       UWORD;
typedef uint32_t       ULONG;
typedef int8_t         CHAR;
typedef int8_t         BYTE;
typedef char*		   STR;
typedef int16_t        WORD;
typedef void		   VOID;
typedef int32_t        LONG;
typedef bool			Boolean;
#define DebugStr		printf


// Memory for the program code (point to SIMZ80 ram space)
UBYTE       Opcodes[CODESIZE];

// Flag per memory location, whether opcode, operand, data
// Bit 4 = 1, i.e. jump to here via JR or similar.
enum {
    Opcode,
    Operand,
    Data
} DataType;

UBYTE       OpcodesFlags[CODESIZE];

// Determine the length of an opcode in bytes
UBYTE       OpcodeLen(UBYTE *Opcodes, ULONG p)
{
UBYTE   len = 1;

    switch(Opcodes[p]) {// Opcode
    case 0x06:          // LD B,n
    case 0x0E:          // LD C,n
    case 0x10:          // DJNZ e
    case 0x16:          // LD D,n
    case 0x18:          // JR e
    case 0x1E:          // LD E,n
    case 0x20:          // JR NZ,e
    case 0x26:          // LD H,n
    case 0x28:          // JR Z,e
    case 0x2E:          // LD L,n
    case 0x30:          // JR NC,e
    case 0x36:          // LD (HL),n
    case 0x38:          // JR C,e
    case 0x3E:          // LD A,n
    case 0xC6:          // ADD A,n
    case 0xCE:          // ADC A,n
    case 0xD3:          // OUT (n),A
    case 0xD6:          // SUB n
    case 0xDB:          // IN A,(n)
    case 0xDE:          // SBC A,n
    case 0xE6:          // AND n
    case 0xEE:          // XOR n
    case 0xF6:          // OR n
    case 0xFE:          // CP n

    case 0xCB:          // Shift, rotate, bit commands
                len = 2;
                break;
    case 0x01:          // LD BC,nn'
    case 0x11:          // LD DE,nn'
    case 0x21:          // LD HL,nn'
    case 0x22:          // LD (nn'),HL
    case 0x2A:          // LD HL,(nn')
    case 0x31:          // LD SP,(nn')
    case 0x32:          // LD (nn'),A
    case 0x3A:          // LD A,(nn')
    case 0xC2:          // JP NZ,nn'
    case 0xC3:          // JP nn'
    case 0xC4:          // CALL NZ,nn'
    case 0xCA:          // JP Z,nn'
    case 0xCC:          // CALL Z,nn'
    case 0xCD:          // CALL nn'
    case 0xD2:          // JP NC,nn'
    case 0xD4:          // CALL NC,nn'
    case 0xDA:          // JP C,nn'
    case 0xDC:          // CALL C,nn'
    case 0xE2:          // JP PO,nn'
    case 0xE4:          // CALL PO,nn'
    case 0xEA:          // JP PE,nn'
    case 0xEC:          // CALL PE,nn'
    case 0xF2:          // JP P,nn'
    case 0xF4:          // CALL P,nn'
    case 0xFA:          // JP M,nn'
    case 0xFC:          // CALL M,nn'
                len = 3;
                break;
    case 0xDD:  len = 2;
                switch(Opcodes[p+1]) {// 2.Part of the opcode
                case 0x34:          // INC (IX+d)
                case 0x35:          // DEC (IX+d)
                case 0x46:          // LD B,(IX+d)
                case 0x4E:          // LD C,(IX+d)
                case 0x56:          // LD D,(IX+d)
                case 0x5E:          // LD E,(IX+d)
                case 0x66:          // LD H,(IX+d)
                case 0x6E:          // LD L,(IX+d)
                case 0x70:          // LD (IX+d),B
                case 0x71:          // LD (IX+d),C
                case 0x72:          // LD (IX+d),D
                case 0x73:          // LD (IX+d),E
                case 0x74:          // LD (IX+d),H
                case 0x75:          // LD (IX+d),L
                case 0x77:          // LD (IX+d),A
                case 0x7E:          // LD A,(IX+d)
                case 0x86:          // ADD A,(IX+d)
                case 0x8E:          // ADC A,(IX+d)
                case 0x96:          // SUB A,(IX+d)
                case 0x9E:          // SBC A,(IX+d)
                case 0xA6:          // AND (IX+d)
                case 0xAE:          // XOR (IX+d)
                case 0xB6:          // OR (IX+d)
                case 0xBE:          // CP (IX+d)
                            len = 3;
                            break;
                case 0x21:          // LD IX,nn'
                case 0x22:          // LD (nn'),IX
                case 0x2A:          // LD IX,(nn')
                case 0x36:          // LD (IX+d),n
                case 0xCB:          // Rotation (IX+d)
                            len = 4;
                            break;
                }
                break;
    case 0xED:  len = 2;
                switch(Opcodes[p+1]) {// 2.Part of the opcode
                case 0x43:          // LD (nn'),BC
                case 0x4B:          // LD BC,(nn')
                case 0x53:          // LD (nn'),DE
                case 0x5B:          // LD DE,(nn')
                case 0x73:          // LD (nn'),SP
                case 0x7B:          // LD SP,(nn')
                            len = 4;
                            break;
                }
                break;
    case 0xFD:  len = 2;
                switch(Opcodes[p+1]) {// 2.Part of the opcode
                case 0x34:          // INC (IY+d)
                case 0x35:          // DEC (IY+d)
                case 0x46:          // LD B,(IY+d)
                case 0x4E:          // LD C,(IY+d)
                case 0x56:          // LD D,(IY+d)
                case 0x5E:          // LD E,(IY+d)
                case 0x66:          // LD H,(IY+d)
                case 0x6E:          // LD L,(IY+d)
                case 0x70:          // LD (IY+d),B
                case 0x71:          // LD (IY+d),C
                case 0x72:          // LD (IY+d),D
                case 0x73:          // LD (IY+d),E
                case 0x74:          // LD (IY+d),H
                case 0x75:          // LD (IY+d),L
                case 0x77:          // LD (IY+d),A
                case 0x7E:          // LD A,(IY+d)
                case 0x86:          // ADD A,(IY+d)
                case 0x8E:          // ADC A,(IY+d)
                case 0x96:          // SUB A,(IY+d)
                case 0x9E:          // SBC A,(IY+d)
                case 0xA6:          // AND (IY+d)
                case 0xAE:          // XOR (IY+d)
                case 0xB6:          // OR (IY+d)
                case 0xBE:          // CP (IY+d)
                            len = 3;
                            break;
                case 0x21:          // LD IY,nn'
                case 0x22:          // LD (nn'),IY
                case 0x2A:          // LD IY,(nn')
                case 0x36:          // LD (IY+d),n
                case 0xCB:          // Rotation,Bitop (IY+d)
                            len = 4;
                            break;
                }
                break;
    }
    return(len);
}

void        ParseOpcodes(ULONG adr)
{
WORD    i,len;
ULONG   next;
Boolean label = true;

    do {
        if(label)                       // put a label?
            OpcodesFlags[adr] |= 0x10;  // Set label
        if((OpcodesFlags[adr] & 0x0F) == Opcode) break; // Loop detected!
        if((OpcodesFlags[adr] & 0x0F) == Operand) {
            DebugStr("\nIllegal Jump?!?");
            return;
        }
        len = OpcodeLen(Opcodes, adr);           // Determine the length of the opcode
        for(i=0;i<len;i++)
            OpcodesFlags[adr+i] = Operand;  // Enter opcode
        OpcodesFlags[adr] = Opcode;     // Mark start of opcode
        if(label) {                     // put a label?
            OpcodesFlags[adr] |= 0x10;  // Set label
            label = false;              // Reset label flag
        }

        next = adr + len;               // Ptr to the subsequent opcode
        switch(Opcodes[adr]) {          // Opcode lairs
        case 0xCA:      // JP c,????
        case 0xC2:
        case 0xDA:
        case 0xD2:
        case 0xEA:
        case 0xE2:
        case 0xFA:
        case 0xF2:
                ParseOpcodes((Opcodes[adr+2]<<8) + Opcodes[adr+1]);
                break;
        case 0x28:      // JR c,??
        case 0x20:
        case 0x38:
        case 0x30:
                ParseOpcodes(adr + 2 + (BYTE)Opcodes[adr+1]);
                break;
        case 0xCC:      // CALL c,????
        case 0xC4:
        case 0xDC:
        case 0xD4:
        case 0xEC:
        case 0xE4:
        case 0xFC:
        case 0xF4:
                ParseOpcodes((Opcodes[adr+2]<<8) + Opcodes[adr+1]);
                break;
        case 0xC8:      // RET c
        case 0xC0:
        case 0xD8:
        case 0xD0:
        case 0xE8:
        case 0xE0:
        case 0xF8:
        case 0xF0:
                break;
        case 0xC7:      // RST 0
        case 0xCF:      // RST 8
        case 0xD7:      // RST 10
        case 0xDF:      // RST 18
        case 0xE7:      // RST 20
        case 0xEF:      // RST 28
        case 0xF7:      // RST 30
        case 0xFF:      // RST 38
                ParseOpcodes(Opcodes[adr] & 0x38);
                break;
        case 0x10:      // DJNZ ??
                ParseOpcodes(adr + 2 + (BYTE)Opcodes[adr+1]);
                break;
        case 0xC3:      // JP ????
                next = (Opcodes[adr+2]<<8) + Opcodes[adr+1];
                label = true;
                break;
        case 0x18:      // JR ??
                next = adr + 2 + (BYTE)Opcodes[adr+1];
                label = true;
                break;
        case 0xCD:      // CALL ????
                ParseOpcodes((Opcodes[adr+2]<<8) + Opcodes[adr+1]);
                break;
        case 0xC9:      // RET
                return;
        case 0xE9:
#if DEBUGGER
                DebugStr("\pJP (HL) gefunden"); // JP (HL)
#endif
                break;
        case 0xDD:
#if DEBUGGER
                if(Opcodes[adr+1] == 0xE9) {    // JP (IX)
                    DebugStr("\pJP (IX) gefunden");
                }
#endif
                break;
        case 0xFD:
#if DEBUGGER
                if(Opcodes[adr+1] == 0xE9) {    // JP (IY)
                    DebugStr("\pJP (IY) gefunden");
                }
#endif
                break;
        case 0xED:
                if(Opcodes[adr+1] == 0x4D) {    // RTI
                    return;
                } else if(Opcodes[adr+1] == 0x45) { // RETN
                    return;
                }
                break;
        }
        adr = next;
    } while(1);
}

// Disassemble
VOID        Disassemble(UBYTE *Opcodes, UWORD adr,STR s)
{
UBYTE           a = Opcodes[adr];
UBYTE           d = (a >> 3) & 7;
UBYTE           e = a & 7;
static STR      reg[8] = {"B","C","D","E","H","L","(HL)","A"};
static STR      dreg[4] = {"BC","DE","HL","SP"};
static STR      cond[8] = {"NZ","Z","NC","C","PO","PE","P","M"};
static STR      arith[8] = {"ADD  A,","ADC  A,","SUB  ","SBC  A,","AND  ","XOR  ","OR   ","CP   "};
CHAR            stemp[80];      // temp.String for sprintf()
CHAR            ireg[3];        // temp.Index register

    switch(a & 0xC0) {
    case 0x00:
        switch(e) {
        case 0x00:
            switch(d) {
            case 0x00:
                strcpy(s,"NOP  ");
                break;
            case 0x01:
                strcpy(s,"EX   AF,AF'");
                break;
            case 0x02:
                strcpy(s,"DJNZ ");
                sprintf(stemp,"%4.4Xh",adr+2+(BYTE)Opcodes[adr+1]);strcat(s,stemp);
                break;
            case 0x03:
                strcpy(s,"JR   ");
                sprintf(stemp,"%4.4Xh",adr+2+(BYTE)Opcodes[adr+1]);strcat(s,stemp);
                break;
            default:
                strcpy(s,"JR   ");
                strcat(s,cond[d & 3]);
                strcat(s,",");
                sprintf(stemp,"%4.4Xh",adr+2+(BYTE)Opcodes[adr+1]);strcat(s,stemp);
                break;
            }
            break;
        case 0x01:
            if(a & 0x08) {
                strcpy(s,"ADD  HL,");
                strcat(s,dreg[d >> 1]);
            } else {
                strcpy(s,"LD   ");
                strcat(s,dreg[d >> 1]);
                strcat(s,",");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
            }
            break;
        case 0x02:
            switch(d) {
            case 0x00:
                strcpy(s,"LD   (BC),A");
                break;
            case 0x01:
                strcpy(s,"LD   A,(BC)");
                break;
            case 0x02:
                strcpy(s,"LD   (DE),A");
                break;
            case 0x03:
                strcpy(s,"LD   A,(DE)");
                break;
            case 0x04:
                strcpy(s,"LD   (");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                strcat(s,"),HL");
                break;
            case 0x05:
                strcpy(s,"LD   HL,(");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                strcat(s,")");
                break;
            case 0x06:
                strcpy(s,"LD   (");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                strcat(s,"),A");
                break;
            case 0x07:
                strcpy(s,"LD   A,(");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                strcat(s,")");
                break;
            }
            break;
        case 0x03:
            if(a & 0x08)
                strcpy(s,"DEC  ");
            else
                strcpy(s,"INC  ");
            strcat(s,dreg[d >> 1]);
            break;
        case 0x04:
            strcpy(s,"INC  ");
            strcat(s,reg[d]);
            break;
        case 0x05:
            strcpy(s,"DEC  ");
            strcat(s,reg[d]);
            break;
        case 0x06:              // LD   d,n
            strcpy(s,"LD   ");
            strcat(s,reg[d]);
            strcat(s,",");
            sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
            break;
        case 0x07:
            {
            static STR str[8] = {"RLCA","RRCA","RLA","RRA","DAA","CPL","SCF","CCF"};
            strcpy(s,str[d]);
            }
            break;
        }
        break;
    case 0x40:                          // LD   d,s
        if(d == e) {
            strcpy(s,"HALT");
        } else {
            strcpy(s,"LD   ");
            strcat(s,reg[d]);
            strcat(s,",");
            strcat(s,reg[e]);
        }
        break;
    case 0x80:
        strcpy(s,arith[d]);
        strcat(s,reg[e]);
        break;
    case 0xC0:
        switch(e) {
        case 0x00:
            strcpy(s,"RET  ");
            strcat(s,cond[d]);
            break;
        case 0x01:
            if(d & 1) {
                switch(d >> 1) {
                case 0x00:
                    strcpy(s,"RET");
                    break;
                case 0x01:
                    strcpy(s,"EXX");
                    break;
                case 0x02:
                    strcpy(s,"JP   (HL)");
                    break;
                case 0x03:
                    strcpy(s,"LD   SP,HL");
                    break;
                }
            } else {
                strcpy(s,"POP  ");
                if((d >> 1)==3)
                    strcat(s,"AF");
                else
                    strcat(s,dreg[d >> 1]);
            }
            break;
        case 0x02:
            strcpy(s,"JP   ");
            strcat(s,cond[d]);
            strcat(s,",");
            sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
            break;
        case 0x03:
            switch(d) {
            case 0x00:
                strcpy(s,"JP   ");
                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                break;
            case 0x01:                  // 0xCB
                a = Opcodes[++adr];     // Get expansion opcode
                d = (a >> 3) & 7;
                e = a & 7;
                stemp[1] = 0;           // temp.String = 1 character
                switch(a & 0xC0) {
                case 0x00:
                    {
                    static STR str[8] = {"RLC","RRC","RL","RR","SLA","SRA","???","SRL"};
                    strcpy(s,str[d]);
                    }
                    strcat(s,"\t\t");
                    strcat(s,reg[e]);
                    break;
                case 0x40:
                    strcpy(s,"BIT  ");
                    stemp[0] = d+'0';strcat(s,stemp);
                    strcat(s,",");
                    strcat(s,reg[e]);
                    break;
                case 0x80:
                    strcpy(s,"RES  ");
                    stemp[0] = d+'0';strcat(s,stemp);
                    strcat(s,",");
                    strcat(s,reg[e]);
                    break;
                case 0xC0:
                    strcpy(s,"SET  ");
                    stemp[0] = d+'0';strcat(s,stemp);
                    strcat(s,",");
                    strcat(s,reg[e]);
                    break;
                }
                break;
            case 0x02:
                strcpy(s,"OUT  (");
                sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                strcat(s,"),A");
                break;
            case 0x03:
                strcpy(s,"IN   A,(");
                sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                strcat(s,")");
                break;
            case 0x04:
                strcpy(s,"EX   (SP),HL");
                break;
            case 0x05:
                strcpy(s,"EX   DE,HL");
                break;
            case 0x06:
                strcpy(s,"DI");
                break;
            case 0x07:
                strcpy(s,"EI");
                break;
            }
            break;
        case 0x04:
            strcpy(s,"CALL ");
            strcat(s,cond[d]);
            strcat(s,",");
            sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
            break;
        case 0x05:
            if(d & 1) {
                switch(d >> 1) {
                case 0x00:
                    strcpy(s,"CALL ");
                    sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                    break;
                case 0x02:              // 0xED
                    a = Opcodes[++adr]; // Get expansion opcode
                    d = (a >> 3) & 7;
                    e = a & 7;
                    switch(a & 0xC0) {
                    case 0x40:
                        switch(e) {
                        case 0x00:
                            strcpy(s,"IN   ");
                            strcat(s,reg[d]);
                            strcat(s,",(C)");
                            break;
                        case 0x01:
                            strcpy(s,"OUT  (C),");
                            strcat(s,reg[d]);
                            break;
                        case 0x02:
                            if(d & 1)
                                strcpy(s,"ADC");
                            else
                                strcpy(s,"SBC");
                            strcat(s,"  HL,");
                            strcat(s,dreg[d >> 1]);
                            break;
                        case 0x03:
                            if(d & 1) {
                                strcpy(s,"LD   ");
                                strcat(s,dreg[d >> 1]);
                                strcat(s,",(");
                                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                                strcat(s,")");
                            } else {
                                strcpy(s,"LD   (");
                                sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                                strcat(s,"),");
                                strcat(s,dreg[d >> 1]);
                            }
                            break;
                        case 0x04:
                            {
                            static STR str[8] = {"NEG","???","???","???","???","???","???","???"};
                            strcpy(s,str[d]);
                            }
                            break;
                        case 0x05:
                            {
                            static STR str[8] = {"RETN","RETI","???","???","???","???","???","???"};
                            strcpy(s,str[d]);
                            }
                            break;
                        case 0x06:
                            strcpy(s,"IM   ");
                            stemp[0] = d + '0' - 1; stemp[1] = 0;
                            strcat(s,stemp);
                            break;
                        case 0x07:
                            {
                            static STR str[8] = {"LD   I,A","???","LD   A,I","???","RRD","RLD","???","???"};
                            strcpy(s,str[d]);
                            }
                            break;
                        }
                        break;
                    case 0x80:
                        {
                        static STR str[32] = {"LDI","CPI","INI","OUTI","???","???","???","???",
                                              "LDD","CPD","IND","OUTD","???","???","???","???",
                                              "LDIR","CPIR","INIR","OTIR","???","???","???","???",
                                              "LDDR","CPDR","INDR","OTDR","???","???","???","???"};
                        strcpy(s,str[a & 0x1F]);
                        }
                        break;
                    }
                    break;
                default:                // 0x01 (0xDD) = IX, 0x03 (0xFD) = IY
                    strcpy(ireg,(a & 0x20)?"IY":"IX");
                    a = Opcodes[++adr]; // Get expansion opcode
                    switch(a) {
                    case 0x09:
                        strcpy(s,"ADD  ");
                        strcat(s,ireg);
                        strcat(s,",BC");
                        break;
                    case 0x19:
                        strcpy(s,"ADD  ");
                        strcat(s,ireg);
                        strcat(s,",DE");
                        break;
                    case 0x21:
                        strcpy(s,"LD   ");
                        strcat(s,ireg);
                        strcat(s,",");
                        sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                        break;
                    case 0x22:
                        strcpy(s,"LD   (");
                        sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                        strcat(s,"),");
                        strcat(s,ireg);
                        break;
                    case 0x23:
                        strcpy(s,"INC  ");
                        strcat(s,ireg);
                        break;
                    case 0x29:
                        strcpy(s,"ADD  ");
                        strcat(s,ireg);
                        strcat(s,",");
                        strcat(s,ireg);
                        break;
                    case 0x2A:
                        strcpy(s,"LD   ");
                        strcat(s,ireg);
                        strcat(s,",(");
                        sprintf(stemp,"%4.4Xh",Opcodes[adr+1]+(Opcodes[adr+2]<<8));strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x2B:
                        strcpy(s,"DEC  ");
                        strcat(s,ireg);
                        break;
                    case 0x34:
                        strcpy(s,"INC  (");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x35:
                        strcpy(s,"DEC  (");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x36:
                        strcpy(s,"LD   (");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,"),");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+2]);strcat(s,stemp);
                        break;
                    case 0x39:
                        strcpy(s,"ADD  ");
                        strcat(s,ireg);
                        strcat(s,",SP");
                        break;
                    case 0x46:
                    case 0x4E:
                    case 0x56:
                    case 0x5E:
                    case 0x66:
                    case 0x6E:
                        strcpy(s,"LD   ");
                        strcat(s,reg[(a>>3)&7]);
                        strcat(s,",(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x70:
                    case 0x71:
                    case 0x72:
                    case 0x73:
                    case 0x74:
                    case 0x75:
                    case 0x77:
                        strcpy(s,"LD   (");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,"),");
                        strcat(s,reg[a & 7]);
                        break;
                    case 0x7E:
                        strcpy(s,"LD   A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x86:
                        strcpy(s,"ADD  A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x8E:
                        strcpy(s,"ADC  A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x96:
                        strcpy(s,"SUB  (");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0x9E:
                        strcpy(s,"SBC  A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0xA6:
                        strcpy(s,"AND  A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0xAE:
                        strcpy(s,"XOR  A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0xB6:
                        strcpy(s,"OR   A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0xBE:
                        strcpy(s,"CP   A,(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    case 0xE1:
                        strcpy(s,"POP  ");
                        strcat(s,ireg);
                        break;
                    case 0xE3:
                        strcpy(s,"EX   (SP),");
                        strcat(s,ireg);
                        break;
                    case 0xE5:
                        strcpy(s,"PUSH ");
                        strcat(s,ireg);
                        break;
                    case 0xE9:
                        strcpy(s,"JP   (");
                        strcat(s,ireg);
                        strcat(s,")");
                        break;
                    case 0xF9:
                        strcpy(s,"LD   SP,");
                        strcat(s,ireg);
                        break;
                    case 0xCB:
                        a = Opcodes[adr+2]; // another sub-opcode
                        d = (a >> 3) & 7;
                        stemp[1] = 0;
                        switch(a & 0xC0) {
                        case 0x00:
                            {
                            static STR str[8] = {"RLC","RRC","RL","RR","SLA","SRA","???","SRL"};
                            strcpy(s,str[d]);
                            }
                            strcat(s,"  ");
                            break;
                        case 0x40:
                            strcpy(s,"BIT  ");
                            stemp[0] = d + '0';
                            strcat(s,stemp);
                            strcat(s,",");
                            break;
                        case 0x80:
                            strcpy(s,"RES  ");
                            stemp[0] = d + '0';
                            strcat(s,stemp);
                            strcat(s,",");
                            break;
                        case 0xC0:
                            strcpy(s,"SET  ");
                            stemp[0] = d + '0';
                            strcat(s,stemp);
                            strcat(s,",");
                            break;
                        }
                        strcat(s,"(");
                        strcat(s,ireg);
                        strcat(s,"+");
                        sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
                        strcat(s,")");
                        break;
                    }
                    break;
                }
            } else {
                strcpy(s,"PUSH ");
                if((d >> 1)==3)
                    strcat(s,"AF");
                else
                    strcat(s,dreg[d >> 1]);
            }
            break;
        case 0x06:
            strcpy(s,arith[d]);
            sprintf(stemp,"%2.2Xh",Opcodes[adr+1]);strcat(s,stemp);
            break;
        case 0x07:
            strcpy(s,"RST  ");
            sprintf(stemp,"%2.2Xh",a & 0x38);strcat(s,stemp);
            break;
        }
        break;
    }
}

// Read, parse, disassemble and output
void z80dsm(int argc, char *argv[])
{
LONG    i;
FILE    *f;
LONG   adr = 0, endaddr, nread;
CHAR    s[80];          // Output string

	switch (argc)
	{
		case 1: // no argument, print help
			printf("usage: z80dsm comfile [prnfile]\n");
			return;
	}
	// Load Code and Flag initial...
	printf("Loading %s...\n", argv[1]);	
    f = fopen(argv[1],"rb"); // 1st argument as input file (expect .COM)
    if(!f) {
		printf("File not found!\n");
		return;
	}
    nread = fread(&Opcodes[0x100],1, CODESIZE-0x100,f);    // .COM file (start to load from 0x100)
    fclose(f);	
	endaddr = nread + 0x100;
	printf("startaddr = 0x100, endaddr=%d\n", endaddr);
	Opcodes[0] = Opcodes[5] = 0xC9; // put address 0(codestart) and address 5(bdos) in 0xC9(RET) to terminate ParseCode()
    for(i=0;i<CODESIZE;i++)         // all Data...
        OpcodesFlags[i] = Data;
		
	printf("Parsing...\n");
	// Start to parse code from 0x100
	ParseOpcodes(0x100);	
	printf("Default Entry Addr:0x0100\n");
	// arguments with additional entry point addr
	if (argc > 3)
	{
		int i, addr;
		for (i = 3; i < argc; i++)
		{
			addr = strtoul(argv[i], NULL, 16);
			printf("Additional Entry Addr:0x%04x\n", addr);
			if (addr) ParseOpcodes(addr);
		}
	}
#if 0	// no need to parse interrupt and NMI	
    for(i=0;i<0x40;i+=0x08)
        if((OpcodesFlags[i] & 0x0F) == Data)
            ParseOpcodes(i);        // Parse RST vectors (if necessary)
    if((OpcodesFlags[i] & 0x0F) == Data)
        ParseOpcodes(0x66);         // Parse NMI vector too (if necessary)
#endif

#if FUTURA_189
    ParseOpcodes(0xA41);
    ParseOpcodes(0xDB6);        // Meßwerte darstellen
    ParseOpcodes(0xF5D);
    ParseOpcodes(0xE83);

    ParseOpcodes(0x0978);
    ParseOpcodes(0x0933);
    ParseOpcodes(0x11D3);
    ParseOpcodes(0x1292);
    ParseOpcodes(0x0AF8);
    ParseOpcodes(0x098F);
    ParseOpcodes(0x0B99);
    ParseOpcodes(0x0BB3);
    ParseOpcodes(0x0B4A);       // Tastenfeld
    ParseOpcodes(0x0B12);
    ParseOpcodes(0x08FF);
    ParseOpcodes(0x08F0);
    ParseOpcodes(0x0BDA);
    ParseOpcodes(0x0BCD);
    ParseOpcodes(0x0A7E);
    ParseOpcodes(0x0C2D);
    ParseOpcodes(0x0AA6);
    ParseOpcodes(0x0848);

    ParseOpcodes(0x1660);
    ParseOpcodes(0x166E);
    ParseOpcodes(0x167C);       // Spezielle Tastenkombinationen
    ParseOpcodes(0x168A);
    ParseOpcodes(0x1698);
    ParseOpcodes(0x16A6);
    ParseOpcodes(0x16CF);
#endif
	// Output Z80 Assembly code
	
    //f = stdout;
	printf("Outputing...\n");
	switch (argc)
	{
		case 1: // no argument: should not touched.
			return;
		case 2:
			f = fopen("OUTPUT.PRN","w");
			break;
		case 3: // argument with output filename
		default: // more than 2 arguments
			f = fopen(argv[2], "w");
			break;			
	}
    if(!f) return;
	adr = 0x100;	// List begin at 0x100
    while(adr < endaddr) {
        WORD    len,i;

        if((OpcodesFlags[adr] & 0x0F) == Data) {
            fprintf(f,"L%4.4X:\tDEFB",(UWORD)adr);
            for(i=0;i<16;i++) {
                if((OpcodesFlags[adr+i] & 0x0F) != Data) break;
                //fprintf(f,"%c%2.2Xh",(i)?',':' ',Opcodes[adr+i]);
				fprintf(f,"%c",(i)?',':' ');
				if ((Opcodes[adr+i]>=0x20) & (Opcodes[adr+i]<=0x7F))
					fprintf(f,"'%c'",Opcodes[adr+i]);
				else
					fprintf(f,"%2.2Xh",Opcodes[adr+i]);
            }
            fprintf(f,"\n");
            adr += i;
        } else {
            len = OpcodeLen(Opcodes, adr);           // Determine the length of the opcode
#if 1
            if(OpcodesFlags[adr] & 0x10)
                fprintf(f,"L%4.4X:\t",adr);
            else
                fprintf(f,"\t\t");
#else
            fprintf(f,"%4.4X: ",(UWORD)adr);
            for(i=0;i<len;i++)
                fprintf(f,"%2.2X ",Opcodes[adr+i]);
            for(i=4;i>len;i--)
                fprintf(f,"   ");
            fprintf(f," ");
#endif			
            Disassemble(Opcodes, adr,s);
            fprintf(f,"%s\n",s);
            adr += len;
        }
    }
    fclose(f);
}
