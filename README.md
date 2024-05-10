# David's CPMEMU CP/M 2.2 Emulator 

There are dozens of CP/M emulators or Z80 emulators that can run CP/M already.
It is not necessary to create one now.
However, back to 30+ years ago, we do have CP/M emulator for DOS but not fast.
That's why I start this project.
This is an old project, but not open to public till now.

## Version 1 (1988,1989) - CP/M emulator for my Z80 ISA CARD on IBM PC XT/AT (clone)

This program is start at 1988. In that time my PC is a IBM PC XT clone (4.77MHz/8MHz).
I decide to build an ISA CARD for Z80 and implement CP/M emulation in PC.
So, the CP/M program can be executed in my PC.

The design of that card is quite simple. It has a Z80 CPU (up to 20MHz), 64KB SRAM.
then some latches and glue logic for this card, that's all.
The RAM can be accessed by both Z80 and 8088. But one at a time. 8088 is master, Z80 
is slave. 8088 can write a special IO address to halt Z80 CPU and gain the RAM access,
and release it to Z80, and polling for Z80's state (in halt or not in halt).
Z80 can output a special IO port to half itself.

The emulation is simple. PC load .COM program and environment into RAM on this card (Z80 is halted now),
then release it. So, Z80 start to execute program. When Z80 call BDOS/BIOS, my
BDOS/BIOS emulation layer will store all registers in dedicated address, then halt ifself.
And, when PC see Z80 is in halt state by polling, CPMEMU will access RAM. find out what BDOS/BIOS
call is needed. Then, PC program will do these calls and put the result back to RAM, and
resume Z80's program execution till next BDOS/BIOS call. That's all. 

This program emulate almost all CP/M 2.2 BDOS calls and partial BIOS call. So, it can not run
some programs that need these unemulated calls. (like STAT, PUN support, LST support, DISK support in BIOS...)

This program is implemented by pure C code using Microsoft Quick C 2.0/2.5 or Microsoft C 6.0 ~ 8.0.
And, lots of BDOS call is implemented by MSDOS's INT21h.

The schematics file of this card is done by orcad 1.0 for DOS. And, I can't open these SCH files.
But I have paper copy on hand. So, I put scanned version as well.

After I make everything works, I management to get one 20MHz Z80 sample (I want to buy it from Zilog Taiwan,
but they just sent it to me!) So I'm happy that I have the fastest CP/M machine ever!

## Version 2 (2001) - CP/M emulator and Z80 simulator for DOS
As time go to 2001. My PC-XT is retired and I have a 386/SX machine. But my Z80 card can't work in 
that machine (reason is still unknown. Maybe from wait state generation, or memory mapping). I choose not to 
debug this card. (ISA bus is dying for PC at that age) I try to port Z80 simulator from YAZE-1.10. 
Then CPMEMU can run on 386/486 PC again. 

## Version 3 (2014) - CP/M emulator and Z80 simulator for LINUX
At 2014, I try to porting it to Linux, use standard C library to emulate these BDOS calls.
I have to rewrite all BDOS/BIOS call that use INT10h/INT21h. Using standard C library and console I/O instead.
Then my program can be executed in Linux. However, I do not spent enough time on testing and improve it's 
compatability, just make it able to run some program. 

## Version 4 (2023) - CP/M emulator and Z80 simulator for Win32/64
In 2023, after my retirement, I have time to do whatever I want. So, I restart this project and add these:
* Port it to Win32/64 (this version is Win32/64 only)
* XSUB
* Time measurement
* Z80CPU simulator is upgraded to YAZE-AG 2.51.3's engine

For Win32/64, I use msys2 as development environment. And, I also try Code::blocks with embedded GCC.
But this embedded GCC is lack of GNU Readline library. So, need to remove -DGNU_READLINE to disable 
command line history feature, or configure Code::blocks to use msys2's gcc compiler and libraries.

## Version 5 (2024) - CP/M emulator and Z80 simulator for LINUX and Win32/64
These feature are addded:
* Both Win32/64 and Linux are maintained
* Key translation (map arrow key, home/end/pageup/pagedn/ins/del to wordstar ctrl key sequence)
* Z80 disassembler (porting from Markus Fritze's Z80Disassembler project https://github.com/sarnau/Z80DisAssembler). 
* HELP is also added for usage of all internal commands.
* Embedded debugger (Z80DDT) into this program and capable to debug Z80 program.

This Z80DDT is very similar to DRI's DDT, but use Z80 Style.
(Original DDT still can be used. I do not spent time to test these CP/M debuggers since these SW
based debugger has it's limitation. )
This DDT feature is refer to https://github.com/algodesigner/z80. I use it's idea and implemenation 
method but rewrite user interface to DDT Like style.

I also porting the feature to input Z80 assembly. This is from Z80-SIM project (by Petr Kulhavy/
Achim Flammenkamp from Z80-ASM 2.4.1 https://wwwhomes.uni-bielefeld.de/achim/z80-asm.html )
and call the generated asm.a directly.

To build this asm.a, you need to download this project and build it, then copy the generated asm.a,
rename it to asm_linux.a or asm_win.a according to platform, copy asm_win.a or asm_linux.a into CPMEMU 
build directory. In windows platform, msys2 can not fully compile z80-asm project (console I/O libary issue)
but still able to generate asm.a (z80-asm project claim to use DJGPP for windows build, not msys2))
In Linux platform, z80-asm can be built successfully (with some warning messages only). 

BTW, My linux build and test platform is WSL2 Ubuntu 22.04.3 under Win10.

## Performance

To make CPMEMU fast, you need to disable Z80DDT and Z80DEBUG feature by remove -DZ80DDT and -DZ80DEBUG
in makefile. This can reduce 2 if checks per Z80 instruction execution (one for each feature).

Time for ZEXALL.COM

PC: Ryzen 5600 (un-overclocked), DDR4-3200 32GB. Windows 10.
```
w/o -DZ80DDT and -DZ80DEBUG: 	8.2 secs
with -DZ80DDT and -DZ80DEBUG:	14.4 secs
```
According to YAZE-AG web site, 4MHz Z80 needs 11569 secs to complete ZEXALL.COM. 8.2 secs means the simulated 
Z80 speed is more than 4GHz (more than 1000x faster!)

Because the time difference of these 2 versions is large (debug version is 75% slower), I suggest to build 2 versions. 
One with both -DZ80DDT and -DZ80DEBUG, for special debugging purpose.
The other is w/o these 2 defines, for normal use.

For gcc compiling optimization, you can try either -O3 or -Ofast, choose faster one.
(In my test environment, msys2's gcc use -O3 and WSL2 Ubuntu 22.04.3's gcc use -Ofast)

## License

This program is original written for myself for fun. So that' why you can see "all right reserved" in source code.
But after 30+ years, it is meaningless to keep reserving these rights.
And, I include lots of open source modules.

So, I, David Chen (chenchijung) declare:
```
The David's CPMEMU program (all versions include original PC ISA Z80 Card Schmeatics) 
now become GPLv2 license. I do not have time to change the source code to remove all 
"all right reserved" description. Just claim here now everything become GPLv2 open source.
2024.5.9
```
I'll be happy if somebody try it and happy with it.
If you think it is not good or you want to improve it, you can clone it and modify yourselves freely.

## Terminal Emulation
CPMEMU do not implement any terminal emulation. So, it is rely on the LINUX shell or Windows CMD.EXE To
do that. In my environment, LINUX shell has ANSI terminal emulation turn on by default, and Windows command
prompt default turn off. Please check this web site about how to turn it on:
Please check https://stackoverflow.com/questions/16755142/how-to-make-win32-console-recognize-ansi-vt100-escape-sequences-in-c


So, for full screen application like turbo pascal or word star, please set the thermal to ANSI. 

## Multiple drive support and user code
For multiple drive, only Windows porting support it. And, the drive in CPM map to real drive in Windows. (C: in CP/M use C: in windows. you can use CD to change working directory for each drive).
Linux porting do not support multiple drive. CPMEMU do not accept change drive command in Linux. And BDOS call ignore this change.


For user code, both Windows and Linux build do not support. There is only one user for each drive.

## CPMEMU Internal Commands


### HELP
You can enter 'HELP' or '? to show this usage lines
```
Z80 /home/dchen/cpmemu5>help
Supported Internal Commands:
 HELP: show this help screen
 DIR or LS <arguments>: list files
 ERA or DEL or RM <arguments>: remove/delete files
 MV or MOVE <arguments>: move files
 REN or RENAME <arguments>: rename files
 TYPE or CAT <arguments>: show contents of file
 PWD: show current working directory
 CD <arguments>: change working directory
 CP or COPY <arguments>: copy files
 SUBMIT <sub-file>: CP/M like submit feature. (incompatible with DRI's SUBMIT.COM)
 XSUB: CP/M like xsub (incompible with DRI's XSUB.COM)
 BDOSDBG <on|off> <BDOS call no. list>: enable/disable/show BDOS call debug prints
 COLD!: cold boot CP/M
 VER: show program version info
 DEBUG <on|off>: enable/disable debug log files (btrace.out & stream.out)
 TIME <program> <arguments>: run program and calculate it's elapsed time
 TIMER <reset|show>: reset/show Timer (for calculate time inside .sub file)
 Z80DDT <program><arguments>: use embedded Z80DDT debugger to run program
 Z80DSM <comfile> <prnfile> [entry addr list]: Disassemble .COM file
 !<command>: run host command
 EXIT or QUIT: quit program
```
 
### SUBMIT and XSUB
 These 2 feature is similar to DRI CP/M's. But not fully compatible.
 That is, CPMEMU can not accept the $$$.SUB generted by DRI's SUBMIT.COM.
 CPMEMU's internal SUBMIT　generate $$$$.SUB instead and with different format.
 So, DRI's SUBMIT.COM and 3rd party SUBMIT.COM enhancement apps are also not work, either.
 DRI's XSUB.COM is also incompatible with CPMEMU. In CPMEMU program, you can only use internal SUBMIT/XSUB.
 
 The .SUB file is very similar, but not fully compatible.
 So, you may need to modify .SUB file to make it work under CPMEMU. It is not difficult.
 Just need some trial and error.
 This is 1st example. Head ';',':','#' or '=' means comment. $1 means 1st argument.
 'timer reset' and 'timer show' command are used to measure the execution time between then.
 will be exeplained later.
 
``` 
; Submit file to compile and link a single Small-C/Plus
; file from the current drive, with the result going to
; drive B:
;
; The compilation takes place on drive M:
;
; Usage:
; 
;   cc file                     : no optimisation
;   cc file opt                 : optimise code
;   cc file opt -c              : compact optimisation
;
timer reset
cc0 $1
zopt -c $1
zmac $1=$1
zres c: clib $1
zlink $1=$1,IOLIB,CLIB
timer show
```
 
 
 This is 2nd example .SUB file. XSUB is called because libr needs extra parameters from .SUB file.
 (This is for HITECH C compiler package, for generating LIBR.LIB.)
```
  xsub
  C:libr 
  r libc3.lib start1.obj open.obj read.obj write.obj chmod.obj seek.obj\
  fcbname.obj rename.obj creat.obj time.obj convtime.obj timezone.obj \
  stat.obj isatty.obj cleanup.obj close.obj unlink.obj dup.obj getfcb.obj \
  srand1.obj getch.obj signal.obj getuid.obj <abort.obj execl.obj bdos.obj \
  bios.obj _exit.obj exit.obj fakeclean.obj fakecpcln.obj sys_err.obj
``` 

### TIMER and TIME
TIME is the internal app to measure the execution time of an app. Like this:\
```
Z80 /home/dchen/cpmemu5>TIME HELLO.COM

Timer Start, base:663085311 ms
hello, world

Timer Stop, base:663085312 ms, Elapsed Time:0.001 Seconds
Z80 /home/dchen/cpmemu5>
```

TIMER is used inside .SUB file, to measure the execution time of several commands,
like this:
  In A.SUB:
```
  TIMER RESET
  program a
  program b
  TIMER SHOW
  program c
  TIMER SHOW
```
1st "TIMER SHOW" shows the ellapse time of program a and b. 2nd "TIMER SHOW" shows the total ellapse time 
for program a, b and c.
 
### DEBUG
CPMEMU can generate the log for BDOS call. Just use this command
```
  DEBUG on
```
to turn on this debug log. Then run your program. Then use 
```
  DEBUG off
```
to stop this log. stream.out and btrace.out will be generated.
stream.out: all input/output chars will be logged in this file
btrace.out: all bdos call will be logged in this file.

### BDOSDBG
This is an independent feature than DEBUG. Each BDOS call has a debug flag to turn on/off debug print on screen.
For example:
```
  BDOSDBG on 15 16 17 18 19
```
will turn on the debug print flag of BDOS CALL 15,16,17,18 and 19. 
Then run your program, the debug print will be shown on screen when these BDOS call is triggered.
You can check CPMBDOS.C to see what debug print will be print out.
```
  BDOSDBG on
```
will turn on all flags.

And, 
```
  BDOSDBG 
```
can show how many flags are opened.
```
  BDOSDBG off 
```
can turn off all flags.

BTW. BDOSDBG flag 0 will also print out some system debug print.

### Z80DSM

This program is a disassembler. It will search from 0x100 for the possible execution path.
(However, self-modifying code, jump tables and other assembly tricks (like push something
into stack and RET) can't be recognized.
```
  Z80DSM HELLO.COM HELLO.PRN
```  
Will disassemble HELLO.COM into HELLO.PRN

If you know the entry address other than 0x100, you can also type these addresses to make disassembly 
more complete. For example:
```
  Z80DSM HELLO.COM HELLO.PRN 200 300 400
```   
The disassembler will start searching valid instructions from 0x100 (default) plus 0x200, 0x300 and 0x400 
(your input). 

### Z80DDT

This is an optional program. Build CPMEMU with "-DZ80DDT" will include this feature.
However, include this feature will make CPMEMU runs slower.

```
Z80 /home/dchen/cpmemu5>Z80DDT HELLO

Z80DDT Embedded Debugger

BC:0000 DE:123C HL:FFFF AF:0154 [.Z.H.P..] IX:0000 IY:0000 SP:FFFD PC:0100 : JP   1A46h
- l
0100: C3 46 1A    JP   1A46h
0103: 21 0E 01    LD   HL,010Eh
0106: E5          PUSH HL
0107: 3E 01       LD   A,01h
0109: CD 40 12    CALL 1240h
010C: C1          POP  BC
010D: C9          RET
010E: 68          LD   L,B
010F: 65          LD   H,L
0110: 6C          LD   L,H
0111: 6C          LD   L,H
0112: 6F          LD   L,A
0113: 2C          INC  L

BC:0000 DE:123C HL:FFFF AF:0154 [.Z.H.P..] IX:0000 IY:0000 SP:FFFD PC:0100 : JP   1A46h
- ?

Embedded Z80DDT Debug Commands:
  t<n>                 - Trace into next n instructions
  u<n>                 - Untrace into next n instructions (no middle reg print)
  s                    - Step over a call
  l<start><end>        - Disassembly
  aaddr                - Assemble from address
  g<start>,<bp1>,<bp2> - Go from start with breakpoint bp1 and bp2
  d<start>,<end>       - Dumps memory from start to end
  waddr,v1,<v2>...     - Write memory contents from addr
  x<reg>,value         - eXamine CPU Flag/Register
  c<0|1|2|3|4>         - Clear breakpoint
  b<0|1|2|3|4>,<addr>  - set/display Breakpoint setting
    (bp0 - watch, bp1,2 - program, bp3,4 - data)
  fstart,end,value     - Fill memory from start till end with value
  mstart,end,dest      - Move memory from start till end to dest
  q                    - Quit z80ddt

BC:0000 DE:123C HL:FFFF AF:0154 [.Z.H.P..] IX:0000 IY:0000 SP:FFFD PC:0100 : JP   1A46h
-
```
The commands are very similar to DRI's DDT. However, there are 5 Breakpoints.
```
BP0: watchpoint (data will be shown with registers on every step, won't break program)
BP1: program breakpoint 1
BP2: program breakpoint 2
BP3: data breakpoint 1 (will be triggered when the value of breakpoint address is changed)
BP4: data breakpoint 2 (will be triggered when the value of breakpoint address is changed)
```
'c' command and 'b' command are used to set/show/clear these breakpoints.

's' command means step over. If next instruction is a "CALL" instruction (conditional or non-conditional),
the break point will be set to the next instruction of this CALL. For other instructions, 
's' command will be the same as 't' or 't1'. 

For usage of rest commands, please referred to DRI's DDT document. They're quite similar.

### Z80DBG

This feature is also optional (make with -DZ80DEBUG will include this feature. Which also 
make Z80 program execution slower).

Usage Example:
```
  Z80DBG on
  HELLO.COM
  Z80DBG off
```
A file "z80trace.out" will be generated. It shows the trace of all executed Z80 instructions 
and register values, one instruction per line. If you run big program, you will have a very 
big z80trace.out file. Please be careful!

### Change Drive command
This feature only supported in Windows build. CPMEMU use host filesystem.
So, A: is mapping to drive A of PC, C: is mapping to drive C of PC....
LINUX file system do not have this feature. So, in LINUX, CPMEMU runs like a single disk system.
Change disk command is missing and related BDOS call will be ignored and all drives map to  
current working directory. (like Windows/Linux, you can use CD to new working directory)

### DIR, ERA, TYPE,...
These command only convert the command ifself. The arguments remain unchanged.
Then host command executes.

So, LINUX and WINDOWS have different behavior. Here are how I convert the command.

| CPMEMU  |  WINDOWS  | LINUX
| --------|-----------|------
| DIR     |	dir		  |	ls -l --group-directories-first
| LS  	  |	dir       |	ls
| CP  	  |	copy      |	cp
| COPY	  |	copy      |	cp
| REN     | rename    |	mv
| RENAME  | rename    |	mv
| MV 	  | move	  |	mv
| MOVE	  | move	  |	mv
| DEL 	  |	del		  |	rm
| RM 	  |	del		  |	rm
| ERA 	  |	del		  |	rm
| TYPE 	  |	type	  |	cat
| CAT 	  |	type	  |	cat
| PWD 	  |	cd		  |	pwd



