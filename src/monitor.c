/*
 * monitor.c - Implements a builtin system monitor for debugging
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "antic.h"
#include "pia.h"
#include "gtia.h"
#include "pokey.h"
#include "prompts.h"
#include "rt-config.h"
#include "monitor.h"

#ifdef HAVE_FFLUSH
#define FFLUSH_STDOUT fflush(stdout)
#else
#define FFLUSH_STDOUT
#endif

#ifdef TRACE
int tron = FALSE;
#endif

unsigned int disassemble(UWORD addr1, UWORD addr2);
UWORD show_instruction(UWORD inad, int wid);

static UWORD addr = 0;

#ifdef MONITOR_ASSEMBLER
static UWORD assembler(UWORD addr);
#endif

#ifdef MONITOR_HINTS
typedef struct {
	char name[9];  /* max. 8 characters */
	UWORD addr;
} symtable_rec;
 /* SYMBOL NAMES TAKEN FROM atari.equ - part of disassembler by Erich BACHER
    and from GTIA.H, POKEY.H, PIA.H & ANTIC.H */
 /* Note: all symbols must be sorted by address (binary search is used).
    Maximal length of symbol name is 8 characters (can be changed above).
    If the adress has different names when reading/writting to it (GTIA ports),
    put the read name first. */

static const symtable_rec symtable[] = {
    {"NGFLAG",  0x0001}, {"CASINI",  0x0002}, {"CASINI+1",0x0003}, {"RAMLO",   0x0004},
    {"RAMLO+1", 0x0005}, {"TRAMSZ",  0x0006}, {"CMCMD",   0x0007}, {"WARMST",  0x0008},
    {"BOOT",    0x0009}, {"DOSVEC",  0x000a}, {"DOSVEC+1",0x000b}, {"DOSINI",  0x000c},
    {"DOSINI+1",0x000d}, {"APPMHI",  0x000e}, {"APPMHI+1",0x000f}, {"POKMSK",  0x0010},
    {"BRKKEY",  0x0011}, {"RTCLOK",  0x0012}, {"RTCLOK+1",0x0013}, {"RTCLOK+2",0x0014},
    {"BUFADR",  0x0015}, {"BUFADR+1",0x0016}, {"ICCOMT",  0x0017}, {"DSKFMS",  0x0018},
    {"DSKFMS+1",0x0019}, {"DSKUTL",  0x001a}, {"DSKUTL+1",0x001b}, {"ABUFPT",  0x001c},
    {"ABUFPT+1",0x001d}, {"ABUFPT+2",0x001e}, {"ABUFPT+3",0x001f},
    {"ICHIDZ",  0x0020}, {"ICDNOZ",  0x0021}, {"ICCOMZ",  0x0022}, {"ICSTAZ",  0x0023},
    {"ICBALZ",  0x0024}, {"ICBAHZ",  0x0025}, {"ICPTLZ",  0x0026}, {"ICPTHZ",  0x0027},
    {"ICBLLZ",  0x0028}, {"ICBLHZ",  0x0029}, {"ICAX1Z",  0x002A}, {"ICAX2Z",  0x002B},
    {"ICAX3Z",  0x002C}, {"ICAX4Z",  0x002D}, {"ICAX5Z",  0x002E}, {"ICAX6Z",  0x002F},
    {"STATUS",  0x0030}, {"CHKSUM",  0x0031}, {"BUFRLO",  0x0032}, {"BUFRHI",  0x0033},
    {"BFENLO",  0x0034}, {"BFENHI",  0x0035}, {"LTEMP" ,  0x0036}, {"LTEMP+1", 0x0037},
    {"BUFRFL",  0x0038}, {"RECVDN",  0x0039}, {"XMTDON",  0x003A}, {"CHKSNT",  0x003B},
    {"NOCKSM",  0x003C}, {"BPTR"  ,  0x003D}, {"FTYPE" ,  0x003E}, {"FEOF"  ,  0x003F},
    {"FREQ"  ,  0x0040}, {"SOUNDR",  0x0041}, {"CRITIC",  0x0042}, {"FMSZPG",  0x0043},
    {"FMSZPG+1",0x0044}, {"FMSZPG+2",0x0045}, {"FMSZPG+3",0x0046}, {"FMSZPG+4",0x0047},
    {"FMSZPG+5",0x0048}, {"FMSZPG+6",0x0049}, {"ZCHAIN",  0x004A}, {"ZCHAIN+1",0x004B},
    {"DSTAT" ,  0x004C}, {"ATRACT",  0x004D}, {"DRKMSK",  0x004E}, {"COLRSH",  0x004F},
    {"TEMP"  ,  0x0050}, {"HOLD1" ,  0x0051}, {"LMARGN",  0x0052}, {"RMARGN",  0x0053},
    {"ROWCRS",  0x0054}, {"COLCRS",  0x0055}, {"COLCRS+1",0x0056}, {"DINDEX",  0x0057},
    {"SAVMSC",  0x0058}, {"SAVMSC+1",0x0059}, {"OLDROW",  0x005A}, {"OLDCOL",  0x005B},
    {"OLDCOL+1",0x005C}, {"OLDCHR",  0x005D}, {"OLDADR",  0x005E}, {"OLDADR+1",0x005F},
    {"FKDEF" ,  0x0060}, {"FKDEF+1", 0x0061}, {"PALNTS",  0x0062}, {"LOGCOL",  0x0063},
    {"ADRESS",  0x0064}, {"ADRESS+1",0x0065}, {"MLTTMP",  0x0066}, {"MLTTMP+1",0x0067},
    {"SAVADR",  0x0068}, {"SAVADR+1",0x0069}, {"RAMTOP",  0x006A}, {"BUFCNT",  0x006B},
    {"BUFSTR",  0x006C}, {"BUFSTR+1",0x006D}, {"BITMSK",  0x006E}, {"SHFAMT",  0x006F},
    {"ROWAC" ,  0x0070}, {"ROWAC+1", 0x0071}, {"COLAC" ,  0x0072}, {"COLAC+1", 0x0073},
    {"ENDPT" ,  0x0074}, {"ENDPT+1", 0x0075}, {"DELTAR",  0x0076}, {"DELTAC",  0x0077},
    {"DELTAC+1",0x0078}, {"KEYDEF",  0x0079}, {"KEYDEF+1",0x007A}, {"SWPFLG",  0x007B},
    {"HOLDCH",  0x007C}, {"INSDAT",  0x007D}, {"COUNTR",  0x007E}, {"COUNTR+1",0x007F},
    {"LOMEM" ,  0x0080}, {"LOMEM+1", 0x0081}, {"VNTP"  ,  0x0082}, {"VNTP+1",  0x0083},
    {"VNTD"  ,  0x0084}, {"VNTD+1",  0x0085}, {"VVTP"  ,  0x0086}, {"VVTP+1",  0x0087},
    {"STMTAB",  0x0088}, {"STMTAB+1",0x0089}, {"STMCUR",  0x008A}, {"STMCUR+1",0x008B},
    {"STARP" ,  0x008C}, {"STARP+1", 0x008D}, {"RUNSTK",  0x008E}, {"RUNSTK+1",0x008F},
    {"TOPSTK",  0x0090}, {"TOPSTK+1",0x0091}, {"MEOLFLG", 0x0092}, {"POKADR",  0x0095},
    {"POKADR+1",0x0096}, {"DATAD" ,  0x00B6}, {"DATALN",  0x00B7}, {"DATALN+1",0x00B8},
    {"STOPLN",  0x00BA}, {"STOPLN+1",0x00BB}, {"SAVCUR",  0x00BE}, {"IOCMD" ,  0x00C0},
    {"IODVC" ,  0x00C1}, {"PROMPT",  0x00C2}, {"ERRSAVE", 0x00C3}, {"COLOUR",  0x00C8},
    {"PTABW" ,  0x00C9}, {"LOADFLG", 0x00CA}, {"FR0"   ,  0x00D4}, {"FR0+1" ,  0x00D5},
    {"FR0+2" ,  0x00D6}, {"FR0+3" ,  0x00D7}, {"FR0+4" ,  0x00D8}, {"FR0+5" ,  0x00D9},
    {"FRE"   ,  0x00DA}, {"FRE+1" ,  0x00DB}, {"FRE+2" ,  0x00DC}, {"FRE+3" ,  0x00DD},
    {"FRE+4" ,  0x00DE}, {"FRE+5" ,  0x00DF}, {"FR1"   ,  0x00E0}, {"FR1+1" ,  0x00E1},
    {"FR1+2" ,  0x00E2}, {"FR1+3" ,  0x00E3}, {"FR1+4" ,  0x00E4}, {"FR1+5" ,  0x00E5},
    {"FR2"   ,  0x00E6}, {"FR2+1" ,  0x00E7}, {"FR2+2" ,  0x00E8}, {"FR2+3" ,  0x00E9},
    {"FR2+4" ,  0x00EA}, {"FR2+5" ,  0x00EB}, {"FRX"   ,  0x00EC}, {"EEXP"  ,  0x00ED},
    {"NSIGN" ,  0x00EE}, {"ESIGN" ,  0x00EF}, {"FCHRFLG", 0x00F0}, {"DIGRT" ,  0x00F1},
    {"CIX"   ,  0x00F2}, {"INBUFF",  0x00F3}, {"INBUFF+1",0x00F4}, {"ZTEMP1",  0x00F5},
    {"ZTEMP1+1",0x00F6}, {"ZTEMP4",  0x00F7}, {"ZTEMP4+1",0x00F8}, {"ZTEMP3",  0x00F9},
    {"ZTEMP3+1",0x00FA}, {"RADFLG",  0x00FB}, {"FLPTR" ,  0x00FC}, {"FLPTR+1", 0x00FD},
    {"FPTR2" ,  0x00FE}, {"FPTR2+1", 0x00FF},

    {"VDSLST",  0x0200}, {"VDSLST+1",0x0201}, {"VPRCED",  0x0202}, {"VPRCED+1",0x0203},
    {"VINTER",  0x0204}, {"VINTER+1",0x0205}, {"VBREAK",  0x0206}, {"VBREAK+1",0x0207},
    {"VKEYBD",  0x0208}, {"VKEYBD+1",0x0209}, {"VSERIN",  0x020A}, {"VSERIN+1",0x020B},
    {"VSEROR",  0x020C}, {"VSEROR+1",0x020D}, {"VSEROC",  0x020E}, {"VSEROC+1",0x020F},
    {"VTIMR1",  0x0210}, {"VTIMR1+1",0x0211}, {"VTIMR2",  0x0212}, {"VTIMR2+1",0x0213},
    {"VTIMR4",  0x0214}, {"VTIMR4+1",0x0215}, {"VIMIRQ",  0x0216}, {"VIMIRQ+1",0x0217},
    {"CDTMV1",  0x0218}, {"CDTMV1+1",0x0219}, {"CDTMV2",  0x021A}, {"CDTMV2+1",0x021B},
    {"CDTMV3",  0x021C}, {"CDTMV3+1",0x021D}, {"CDTMV4",  0x021E}, {"CDTMV4+1",0x021F},
    {"CDTMV5",  0x0220}, {"CDTMV5+1",0x0221}, {"VVBLKI",  0x0222}, {"VVBLKI+1",0x0223},
    {"VVBLKD",  0x0224}, {"VVBLKD+1",0x0225}, {"CDTMA1",  0x0226}, {"CDTMA1+1",0x0227},
    {"CDTMA2",  0x0228}, {"CDTMA2+1",0x0229}, {"CDTMF3",  0x022A}, {"SRTIMR",  0x022B},
    {"CDTMF4",  0x022C}, {"INTEMP",  0x022D}, {"CDTMF5",  0x022E}, {"SDMCTL",  0x022F},
    {"SDLSTL",  0x0230}, {"SDLSTH",  0x0231}, {"SSKCTL",  0x0232}, {"SPARE" ,  0x0233},
    {"LPENH" ,  0x0234}, {"LPENV" ,  0x0235}, {"BRKKY" ,  0x0236}, {"BRKKY+1", 0x0237},
    {"VPIRQ" ,  0x0238}, {"VPIRQ+1", 0x0239}, {"CDEVIC",  0x023A}, {"CCOMND",  0x023B},
    {"CAUX1" ,  0x023C}, {"CAUX2" ,  0x023D}, {"TMPSIO",  0x023E}, {"ERRFLG",  0x023F},
    {"DFLAGS",  0x0240}, {"DBSECT",  0x0241}, {"BOOTAD",  0x0242}, {"BOOTAD+1",0x0243},
    {"COLDST",  0x0244}, {"RECLEN",  0x0245}, {"DSKTIM",  0x0246}, {"PDVMSK",  0x0247},
    {"SHPDVS",  0x0248}, {"PDMSK" ,  0x0249}, {"RELADR",  0x024A}, {"RELADR+1",0x024B},
    {"PPTMPA",  0x024C}, {"PPTMPX",  0x024D}, {"CHSALT",  0x026B}, {"VSFLAG",  0x026C},
    {"KEYDIS",  0x026D}, {"FINE"  ,  0x026E}, {"GPRIOR",  0x026F}, {"PADDL0",  0x0270},
    {"PADDL1",  0x0271}, {"PADDL2",  0x0272}, {"PADDL3",  0x0273}, {"PADDL4",  0x0274},
    {"PADDL5",  0x0275}, {"PADDL6",  0x0276}, {"PADDL7",  0x0277}, {"STICK0",  0x0278},
    {"STICK1",  0x0279}, {"STICK2",  0x027A}, {"STICK3",  0x027B}, {"PTRIG0",  0x027C},
    {"PTRIG1",  0x027D}, {"PTRIG2",  0x027E}, {"PTRIG3",  0x027F}, {"PTRIG4",  0x0280},
    {"PTRIG5",  0x0281}, {"PTRIG6",  0x0282}, {"PTRIG7",  0x0283}, {"STRIG0",  0x0284},
    {"STRIG1",  0x0285}, {"STRIG2",  0x0286}, {"STRIG3",  0x0287}, {"HIBYTE",  0x0288},
    {"WMODE" ,  0x0289}, {"BLIM"  ,  0x028A}, {"IMASK" ,  0x028B}, {"JVECK" ,  0x028C},
    {"NEWADR",  0x028E}, {"TXTROW",  0x0290}, {"TXTCOL",  0x0291}, {"TXTCOL+1",0x0292},
    {"TINDEX",  0x0293}, {"TXTMSC",  0x0294}, {"TXTMSC+1",0x0295}, {"TXTOLD",  0x0296},
    {"TXTOLD+1",0x0297}, {"TXTOLD+2",0x0298}, {"TXTOLD+3",0x0299}, {"TXTOLD+4",0x029A},
    {"TXTOLD+5",0x029B}, {"CRETRY",  0x029C}, {"HOLD3" ,  0x029D}, {"SUBTMP",  0x029E},
    {"HOLD2" ,  0x029F}, {"DMASK" ,  0x02A0}, {"TMPLBT",  0x02A1}, {"ESCFLG",  0x02A2},
    {"TABMAP",  0x02A3}, {"TABMAP+1",0x02A4}, {"TABMAP+2",0x02A5}, {"TABMAP+3",0x02A6},
    {"TABMAP+4",0x02A7}, {"TABMAP+5",0x02A8}, {"TABMAP+6",0x02A9}, {"TABMAP+7",0x02AA},
    {"TABMAP+8",0x02AB}, {"TABMAP+9",0x02AC}, {"TABMAP+A",0x02AD}, {"TABMAP+B",0x02AE},
    {"TABMAP+C",0x02AF}, {"TABMAP+D",0x02B0}, {"TABMAP+E",0x02B1}, {"LOGMAP",  0x02B2},
    {"LOGMAP+1",0x02B3}, {"LOGMAP+2",0x02B4}, {"LOGMAP+3",0x02B5}, {"INVFLG",  0x02B6},
    {"FILFLG",  0x02B7}, {"TMPROW",  0x02B8}, {"TMPCOL",  0x02B9}, {"TMPCOL+1",0x02BA},
    {"SCRFLG",  0x02BB}, {"HOLD4" ,  0x02BC}, {"DRETRY",  0x02BD}, {"SHFLOC",  0x02BE},
    {"BOTSCR",  0x02BF}, {"PCOLR0",  0x02C0}, {"PCOLR1",  0x02C1}, {"PCOLR2",  0x02C2},
    {"PCOLR3",  0x02C3}, {"COLOR0",  0x02C4}, {"COLOR1",  0x02C5}, {"COLOR2",  0x02C6},
    {"COLOR3",  0x02C7}, {"COLOR4",  0x02C8}, {"RUNADR",  0x02C9}, {"RUNADR+1",0x02CA},
    {"HIUSED",  0x02CB}, {"HIUSED+1",0x02CC}, {"ZHIUSE",  0x02CD}, {"ZHIUSE+1",0x02CE},
    {"GBYTEA",  0x02CF}, {"GBYTEA+1",0x02D0}, {"LOADAD",  0x02D1}, {"LOADAD+1",0x02D2},
    {"ZLOADA",  0x02D3}, {"ZLOADA+1",0x02D4}, {"DSCTLN",  0x02D5}, {"DSCTLN+1",0x02D6},
    {"ACMISR",  0x02D7}, {"ACMISR+1",0x02D8}, {"KRPDER",  0x02D9}, {"KEYREP",  0x02DA},
    {"NOCLIK",  0x02DB}, {"HELPFG",  0x02DC}, {"DMASAV",  0x02DD}, {"PBPNT" ,  0x02DE},
    {"PBUFSZ",  0x02DF}, {"RUNAD" ,  0x02E0}, {"RUNAD+1", 0x02E1}, {"INITAD",  0x02E2},
    {"INITAD+1",0x02E3}, {"RAMSIZ",  0x02E4}, {"MEMTOP",  0x02E5}, {"MEMTOP+1",0x02E6},
    {"MEMLO" ,  0x02E7}, {"MEMLO+1", 0x02E8}, {"HNDLOD",  0x02E9}, {"DVSTAT",  0x02EA},
    {"DVSTAT+1",0x02EB}, {"DVSTAT+2",0x02EC}, {"DVSTAT+3",0x02ED}, {"CBAUDL",  0x02EE},
    {"CBAUDH",  0x02EF}, {"CRSINH",  0x02F0}, {"KEYDEL",  0x02F1}, {"CH1"   ,  0x02F2},
    {"CHACT" ,  0x02F3}, {"CHBAS" ,  0x02F4}, {"NEWROW",  0x02F5}, {"NEWCOL",  0x02F6},
    {"NEWCOL+1",0x02F7}, {"ROWINC",  0x02F8}, {"COLINC",  0x02F9}, {"CHAR"  ,  0x02FA},
    {"ATACHR",  0x02FB}, {"CH"    ,  0x02FC}, {"FILDAT",  0x02FD}, {"DSPFLG",  0x02FE},
    {"SSFLAG",  0x02FF},


    {"DDEVIC",  0x0300}, {"DUNIT"   ,0x0301}, {"DCOMND"  ,0x0302}, {"DSTATS"  ,0x0303},
    {"DBUFLO"  ,0x0304}, {"DBUFHI"  ,0x0305}, {"DTIMLO"  ,0x0306}, {"DUNUSE"  ,0x0307},
    {"DBYTLO"  ,0x0308}, {"DBYTHI"  ,0x0309}, {"DAUX1"   ,0x030A}, {"DAUX2"   ,0x030B},
    {"TIMER1"  ,0x030C}, {"TIMER1+1",0x030D}, {"ADDCOR"  ,0x030E}, {"CASFLG"  ,0x030F},
    {"TIMER2"  ,0x0310}, {"TIMER2+1",0x0311}, {"TEMP1"   ,0x0312}, {"TEMP1+1", 0x0313},
    {"TEMP2"   ,0x0314}, {"TEMP3"   ,0x0315}, {"SAVIO"   ,0x0316}, {"TIMFLG",  0x0317},
    {"STACKP",  0x0318}, {"TSTAT"   ,0x0319},
    {"HATABS",  0x031a},  /*HATABS 1-34*/
    {"PUTBT1",  0x033d}, {"PUTBT2",  0x033e}, {"PUTBT3",  0x033f},
    {"B0-ICHID",0x0340}, {"B0-ICDNO",0x0341}, {"B0-ICCOM",0x0342}, {"B0-ICSTA",0x0343},
    {"B0-ICBAL",0x0344}, {"B0-ICBAH",0x0345}, {"B0-ICPTL",0x0346}, {"B0-ICPTH",0x0347},
    {"B0-ICBLL",0x0348}, {"B0-ICBLH",0x0349}, {"B0-ICAX1",0x034a}, {"B0-ICAX2",0x034b},
    {"B0-ICAX3",0x034c}, {"B0-ICAX4",0x034d}, {"B0-ICAX5",0x034e}, {"B0-ICAX6",0x034f},
    {"B1-ICHID",0x0350}, {"B1-ICDNO",0x0351}, {"B1-ICCOM",0x0352}, {"B1-ICSTA",0x0353},
    {"B1-ICBAL",0x0354}, {"B1-ICBAH",0x0355}, {"B1-ICPTL",0x0356}, {"B1-ICPTH",0x0357},
    {"B1-ICBLL",0x0358}, {"B1-ICBLH",0x0359}, {"B1-ICAX1",0x035a}, {"B1-ICAX2",0x035b},
    {"B1-ICAX3",0x035c}, {"B1-ICAX4",0x035d}, {"B1-ICAX5",0x035e}, {"B1-ICAX6",0x035f},
    {"B2-ICHID",0x0360}, {"B2-ICDNO",0x0361}, {"B2-ICCOM",0x0362}, {"B2-ICSTA",0x0363},
    {"B2-ICBAL",0x0364}, {"B2-ICBAH",0x0365}, {"B2-ICPTL",0x0366}, {"B2-ICPTH",0x0367},
    {"B2-ICBLL",0x0368}, {"B2-ICBLH",0x0369}, {"B2-ICAX1",0x036a}, {"B2-ICAX2",0x036b},
    {"B2-ICAX3",0x036c}, {"B2-ICAX4",0x036d}, {"B2-ICAX5",0x036e}, {"B2-ICAX6",0x036f},
    {"B3-ICHID",0x0370}, {"B3-ICDNO",0x0371}, {"B3-ICCOM",0x0372}, {"B3-ICSTA",0x0373},
    {"B3-ICBAL",0x0374}, {"B3-ICBAH",0x0375}, {"B3-ICPTL",0x0376}, {"B3-ICPTH",0x0377},
    {"B3-ICBLL",0x0378}, {"B3-ICBLH",0x0379}, {"B3-ICAX1",0x037a}, {"B3-ICAX2",0x037b},
    {"B3-ICAX3",0x037c}, {"B3-ICAX4",0x037d}, {"B3-ICAX5",0x037e}, {"B3-ICAX6",0x037f},
    {"B4-ICHID",0x0380}, {"B4-ICDNO",0x0381}, {"B4-ICCOM",0x0382}, {"B4-ICSTA",0x0383},
    {"B4-ICBAL",0x0384}, {"B4-ICBAH",0x0385}, {"B4-ICPTL",0x0386}, {"B4-ICPTH",0x0387},
    {"B4-ICBLL",0x0388}, {"B4-ICBLH",0x0389}, {"B4-ICAX1",0x038a}, {"B4-ICAX2",0x038b},
    {"B4-ICAX3",0x038c}, {"B4-ICAX4",0x038d}, {"B4-ICAX5",0x038e}, {"B4-ICAX6",0x038f},
    {"B5-ICHID",0x0390}, {"B5-ICDNO",0x0391}, {"B5-ICCOM",0x0392}, {"B5-ICSTA",0x0393},
    {"B5-ICBAL",0x0394}, {"B5-ICBAH",0x0395}, {"B5-ICPTL",0x0396}, {"B5-ICPTH",0x0397},
    {"B5-ICBLL",0x0398}, {"B5-ICBLH",0x0399}, {"B5-ICAX1",0x039a}, {"B5-ICAX2",0x039b},
    {"B5-ICAX3",0x039c}, {"B5-ICAX4",0x039d}, {"B5-ICAX5",0x039e}, {"B5-ICAX6",0x039f},
    {"B6-ICHID",0x03a0}, {"B6-ICDNO",0x03a1}, {"B6-ICCOM",0x03a2}, {"B6-ICSTA",0x03a3},
    {"B6-ICBAL",0x03a4}, {"B6-ICBAH",0x03a5}, {"B6-ICPTL",0x03a6}, {"B6-ICPTH",0x03a7},
    {"B6-ICBLL",0x03a8}, {"B6-ICBLH",0x03a9}, {"B6-ICAX1",0x03aa}, {"B6-ICAX2",0x03ab},
    {"B6-ICAX3",0x03ac}, {"B6-ICAX4",0x03ad}, {"B6-ICAX5",0x03ae}, {"B6-ICAX6",0x03af},
    {"B7-ICHID",0x03b0}, {"B7-ICDNO",0x03b1}, {"B7-ICCOM",0x03b2}, {"B7-ICSTA",0x03b3},
    {"B7-ICBAL",0x03b4}, {"B7-ICBAH",0x03b5}, {"B7-ICPTL",0x03b6}, {"B7-ICPTH",0x03b7},
    {"B7-ICBLL",0x03b8}, {"B7-ICBLH",0x03b9}, {"B7-ICAX1",0x03ba}, {"B7-ICAX2",0x03bb},
    {"B7-ICAX3",0x03bc}, {"B7-ICAX4",0x03bd}, {"B7-ICAX5",0x03be}, {"B7-ICAX6",0x03bf},
    {"PRNBUF",  0x03c0},  /* PRNBUF 1-39 */
    {"SUPERF",  0x03e8}, {"CKEY",    0x03e9}, {"CASSBT",  0x03ea}, {"CARTCK",  0x03eb},
    {"DERRF",   0x03ec}, {"ACMVAR",  0x03ed}, /* ACMVAR 1-10 */
    {"BASICF",  0x03f8}, {"MINTLK",  0x03f9}, {"GINTLK",  0x03fa}, {"CHLINK",  0x03fb},
    {"CHLINK+1",0x03fc}, {"CASBUF",  0x03fd},

    {"M0PF"  ,0xd000}, {"HPOSP0",0xd000}, {"M1PF"  ,0xd001}, {"HPOSP1",0xd001},
    {"M2PF"  ,0xd002}, {"HPOSP2",0xd002}, {"M3PF"  ,0xd003}, {"HPOSP3",0xd003},
    {"P0PF"  ,0xd004}, {"HPOSM0",0xd004}, {"P1PF"  ,0xd005}, {"HPOSM1",0xd005},
    {"P2PF"  ,0xd006}, {"HPOSM2",0xd006}, {"P3PF"  ,0xd007}, {"HPOSM3",0xd007},
    {"M0PL"  ,0xd008}, {"SIZEP0",0xd008}, {"M1PL"  ,0xd009}, {"SIZEP1",0xd009},
    {"M2PL"  ,0xd00a}, {"SIZEP2",0xd00a}, {"M3PL"  ,0xd00b}, {"SIZEP3",0xd00b},
    {"P0PL"  ,0xd00c}, {"SIZEM", 0xd00c}, {"P1PL"  ,0xd00d}, {"GRAFP0",0xd00d},
    {"P2PL"  ,0xd00e}, {"GRAFP1",0xd00e}, {"P3PL"  ,0xd00f}, {"GRAFP2",0xd00f},
    {"TRIG0" ,0xd010}, {"GRAFP3",0xd010}, {"TRIG1" ,0xd011}, {"GRAFM", 0xd011},
    {"TRIG2" ,0xd012}, {"COLPM0",0xd012}, {"TRIG3" ,0xd013}, {"COLPM1",0xd013},
    {"PAL"   ,0xd014}, {"COLPM2",0xd014}, {"COLPM3",0xd015}, {"COLPF0",0xd016},
    {"COLPF1",0xd017},
    {"COLPF2",0xd018}, {"COLPF3",0xd019}, {"COLBK", 0xd01a}, {"PRIOR", 0xd01b},
    {"VDELAY",0xd01c}, {"GRACTL",0xd01d}, {"HITCLR",0xd01e}, {"CONSOL",0xd01f},

    {"POT0"  ,0xd200}, {"AUDF1", 0xd200}, {"POT1"  ,0xd201}, {"AUDC1", 0xd201},
    {"POT2"  ,0xd202}, {"AUDF2", 0xd202}, {"POT3"  ,0xd203}, {"AUDC2", 0xd203},
    {"POT4"  ,0xd204}, {"AUDF3", 0xd204}, {"POT5"  ,0xd205}, {"AUDC3", 0xd205},
    {"POT6"  ,0xd206}, {"AUDF4", 0xd206}, {"POT7"  ,0xd207}, {"AUDC4", 0xd207},
    {"ALLPOT",0xd208}, {"AUDCTL",0xd208}, {"KBCODE",0xd209}, {"STIMER",0xd209},
    {"RANDOM",0xd20a}, {"SKREST",0xd20a}, {"POTGO", 0xd20b},
    {"SERIN", 0xd20d}, {"SEROUT",0xd20d}, {"IRQST", 0xd20e}, {"IRQEN", 0xd20e},
    {"SKSTAT",0xd20f}, {"SKCTL", 0xd20f},

    {"PORTA", 0xd300}, {"PORTB", 0xd301}, {"PACTL", 0xd302}, {"PBCTL", 0xd303},

    {"DMACTL",0xd400}, {"CHACTL",0xd401}, {"DLISTL",0xd402}, {"DLISTH",0xd403},
    {"HSCROL",0xd404}, {"VSCROL",0xd405}, {"PMBASE",0xd407}, {"CHBASE",0xd409},
    {"WSYNC", 0xd40a}, {"VCOUNT",0xd40b}, {"PENH",  0xd40c}, {"PENV",  0xd40d},
    {"NMIEN", 0xd40e}, {"NMIST" ,0xd40f}, {"NMIRES",0xd40f},

    {"AFP",   0xd800}, {"FASC",  0xd8e6}, {"IFP",   0xd9aa}, {"FPI",   0xd9d2},
    {"ZPRO",  0xda44}, {"ZF1",   0xda46}, {"FSUB",  0xda60}, {"FADD",  0xda66},
    {"FMUL",  0xdadb}, {"FDIV",  0xdb28}, {"PLYEVL",0xdd40}, {"FLD0R", 0xdd89},
    {"FLD0R", 0xdd8d}, {"FLD1R", 0xdd98}, {"FLD1P", 0xdd9c}, {"FST0R", 0xdda7},
    {"FST0P", 0xddab}, {"FMOVE", 0xddb6}, {"EXP",   0xddc0}, {"EXP10", 0xddcc},
    {"LOG",   0xdecd}, {"LOG10", 0xded1},

    {"DSKINV",0xe453}, {"CIOV",  0xe456}, {"SIOV",  0xe459}, {"SETVBV",0xe45c},
    {"SYSVBV",0xe45f}, {"XITVBV",0xe462}, {"SIOINV",0xe465}, {"SENDEV",0xe468},
    {"INTINV",0xe46b}, {"CIOINV",0xe46e}, {"SELFSV",0xe471}, {"WARMSV",0xe474},
    {"COLDSV",0xe477}, {"RBLOKV",0xe47a}, {"CSOPIV",0xe47d}, {"PUPDIV",0xe480},
    {"SELFTSV",0xe483},{"PENTV", 0xe486}, {"PHUNLV",0xe489}, {"PHINIV",0xe48c},
    {"GPDVV", 0xe48f}
    };
static const int symtable_size = sizeof(symtable) / sizeof(symtable_rec);
#endif

static const char instr6502[256][11] =
{
	"BRK", "ORA ($1,X)", "CIM", "ASO ($1,X)", "NOP $1", "ORA $1", "ASL $1", "ASO $1",
	"PHP", "ORA #$1", "ASL", "ANC #$1", "NOP $2", "ORA $2", "ASL $2", "ASO $2",

	"BPL $0", "ORA ($1),Y", "CIM", "ASO ($1),Y", "NOP $1,X", "ORA $1,X", "ASL $1,X", "ASO $1,X",
	"CLC", "ORA $2,Y", "NOP", "ASO $2,Y", "NOP $2,X", "ORA $2,X", "ASL $2,X", "ASO $2,X",

	"JSR $2", "AND ($1,X)", "CIM", "RLA ($1,X)", "BIT $1", "AND $1", "ROL $1", "RLA $1",
	"PLP", "AND #$1", "ROL", "ANC #$1", "BIT $2", "AND $2", "ROL $2", "RLA $2",

	"BMI $0", "AND ($1),Y", "CIM", "RLA ($1),Y", "NOP $1,X", "AND $1,X", "ROL $1,X", "RLA $1,X",
	"SEC", "AND $2,Y", "NOP", "RLA $2,Y", "NOP $2,X", "AND $2,X", "ROL $2,X", "RLA $2,X",


	"RTI", "EOR ($1,X)", "CIM", "LSE ($1,X)", "NOP $1", "EOR $1", "LSR $1", "LSE $1",
	"PHA", "EOR #$1", "LSR", "ALR #$1", "JMP $2", "EOR $2", "LSR $2", "LSE $2",

	"BVC $0", "EOR ($1),Y", "CIM", "LSE ($1),Y", "NOP $1,X", "EOR $1,X", "LSR $1,X", "LSE $1,X",
	"CLI", "EOR $2,Y", "NOP", "LSE $2,Y", "NOP $2,X", "EOR $2,X", "LSR $2,X", "LSE $2,X",

	"RTS", "ADC ($1,X)", "CIM", "RRA ($1,X)", "NOP $1", "ADC $1", "ROR $1", "RRA $1",
	"PLA", "ADC #$1", "ROR", "ARR #$1", "JMP ($2)", "ADC $2", "ROR $2", "RRA $2",

	"BVS $0", "ADC ($1),Y", "CIM", "RRA ($1),Y", "NOP $1,X", "ADC $1,X", "ROR $1,X", "RRA $1,X",
	"SEI", "ADC $2,Y", "NOP", "RRA $2,Y", "NOP $2,X", "ADC $2,X", "ROR $2,X", "RRA $2,X",


	"NOP #$1", "STA ($1,X)", "NOP #$1", "SAX ($1,X)", "STY $1", "STA $1", "STX $1", "SAX $1",
	"DEY", "NOP #$1", "TXA", "ANE #$1", "STY $2", "STA $2", "STX $2", "SAX $2",

	"BCC $0", "STA ($1),Y", "CIM", "SHA ($1),Y", "STY $1,X", "STA $1,X", "STX $1,Y", "SAX $1,Y",
	"TYA", "STA $2,Y", "TXS", "SHS $2,Y", "SHY $2,X", "STA $2,X", "SHX $2,Y", "SHA $2,Y",

	"LDY #$1", "LDA ($1,X)", "LDX #$1", "LAX ($1,X)", "LDY $1", "LDA $1", "LDX $1", "LAX $1",
	"TAY", "LDA #$1", "TAX", "ANX #$1", "LDY $2", "LDA $2", "LDX $2", "LAX $2",

	"BCS $0", "LDA ($1),Y", "CIM", "LAX ($1),Y", "LDY $1,X", "LDA $1,X", "LDX $1,Y", "LAX $1,X",
	"CLV", "LDA $2,Y", "TSX", "LAS $2,Y", "LDY $2,X", "LDA $2,X", "LDX $2,Y", "LAX $2,Y",


	"CPY #$1", "CMP ($1,X)", "NOP #$1", "DCM ($1,X)", "CPY $1", "CMP $1", "DEC $1", "DCM $1",
	"INY", "CMP #$1", "DEX", "SBX #$1", "CPY $2", "CMP $2", "DEC $2", "DCM $2",

	"BNE $0", "CMP ($1),Y", "[ESCRTS]", "DCM ($1),Y", "NOP $1,X", "CMP $1,X", "DEC $1,X", "DCM $1,X",
	"CLD", "CMP $2,Y", "NOP", "DCM $2,Y", "NOP $2,X", "CMP $2,X", "DEC $2,X", "DCM $2,X",


	"CPX #$1", "SBC ($1,X)", "NOP #$1", "INS ($1,X)", "CPX $1", "SBC $1", "INC $1", "INS $1",
	"INX", "SBC #$1", "NOP", "SBC #$1", "CPX $2", "SBC $2", "INC $2", "INS $2",

	"BEQ $0", "SBC ($1),Y", "[ESC]", "INS ($1),Y", "NOP $1,X", "SBC $1,X", "INC $1,X", "INS $1,X",
	"SED", "SBC $2,Y", "NOP", "INS $2,Y", "NOP $2,X", "SBC $2,X", "INC $2,X", "INS $2,X"
};

/* Opcode type:
   bits 1-0 = instruction length
   bit 3    = instruction writes to memory (without stack-manipulating instructions)
   bits 7-4 = adressing type:
     0 = NONE (implicit)
     1 = ABSOLUTE
     2 = ZPAGE
     3 = ABSOLUTE_X
     4 = ABSOLUTE_Y
     5 = INDIRECT_X
     6 = INDIRECT_Y
     7 = ZPAGE_X
     8 = ZPAGE_Y
     9 = RELATIVE
     A = IMMEDIATE
     B = STACK 2 (RTS)
     C = STACK 3 (RTI)
     D = INDIRECT (JMP () )
     E = ESC RTS
     F = ESC
*/

static const UBYTE optype6502[256] = {
  0x01, 0x52, 0x01, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0x01, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b,
  0x13, 0x52, 0x01, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0x01, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b,
  0xc1, 0x52, 0x01, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0x01, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b,
  0xb1, 0x52, 0x01, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0xd3, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0x01, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b,
  0xa2, 0x5a, 0x01, 0x5a, 0x2a, 0x2a, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x1b, 0x1b, 0x1b, 0x1b,
  0x92, 0x6a, 0x01, 0x6a, 0x7a, 0x7a, 0x8a, 0x8a, 0x01, 0x4b, 0x01, 0x4b, 0x33, 0x3b, 0x43, 0x43,
  0xa2, 0x52, 0xa2, 0x52, 0x22, 0x22, 0x22, 0x22, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x13, 0x13,
  0x92, 0x62, 0x01, 0x62, 0x72, 0x72, 0x82, 0x82, 0x01, 0x43, 0x01, 0x43, 0x33, 0x33, 0x43, 0x43,
  0xa2, 0x52, 0xa2, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0xe2, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b,
  0xa2, 0x52, 0xa2, 0x5a, 0x22, 0x22, 0x2a, 0x2a, 0x01, 0xa2, 0x01, 0xa2, 0x13, 0x13, 0x1b, 0x1b,
  0x92, 0x62, 0xf2, 0x6a, 0x72, 0x72, 0x7a, 0x7a, 0x01, 0x43, 0x01, 0x4b, 0x33, 0x33, 0x3b, 0x3b };


static char *get_token(char *string)
{
	static char *s;
	char *t;

	if (string)
		s = string;				/* New String */

	while (*s == ' ')
		s++;					/* Skip Leading Spaces */

	if (*s) {
		t = s;					/* Start of String */
		while (*s != ' ' && *s) {	/* Locate End of String */
			s++;
		}

		if (*s == ' ') {		/* Space Terminated ? */
			*s = '\0';			/* C String Terminator */
			s++;				/* Point to Next Char */
		}
	}
	else {
		t = NULL;
	}

	return t;					/* Pointer to String */
}

#if defined(MONITOR_BREAK) || !defined(NO_YPOS_BREAK_FLICKER)
static int get_dec(char *string, UWORD * decval)
{
	int idecval;
	char *t;

	t = get_token(string);
	if (t) {
		sscanf(t, "%d", &idecval);
		*decval = idecval;
		return 1;
	}
	return 0;
}
#endif

static int get_hex(UWORD *hexval)
{
	unsigned int ihexval;
	char *t;

	t = get_token(NULL);
	if (t) {
		sscanf(t, "%X", &ihexval);
		*hexval = ihexval;
		return 1;
	}
	return 0;
}

static int get_hex2(UWORD *hexval1, UWORD *hexval2)
{
	return get_hex(hexval1) && get_hex(hexval2);
}

static int get_hex3(UWORD *hexval1, UWORD *hexval2, UWORD *hexval3)
{
	return get_hex(hexval1) && get_hex(hexval2) && get_hex(hexval3);
}

static void show_regs(void)
{
	int i;
	printf("PC=%04X  A=%02X  S=%02X  X=%02X  Y=%02X  P=",
		regPC, regA, regS, regX, regY);
	for (i = 0; i < 8; i++)
		putchar(regP & (0x80 >> i) ? "NV*BDIZC"[i] : '-');
	putchar('\n');
}

UWORD ypos_break_addr = 0xffff;

#ifdef MONITOR_BREAK
UWORD break_addr;
UBYTE break_step = 0;
UBYTE break_cim = 0;
UBYTE break_here = 0;
UBYTE show_inst = 0;
UBYTE break_ret = 0;
int ret_nesting = 0;
int brkhere = 0;
#endif

int monitor(void)
{
	UWORD addr;
	char s[128];
	static char old_s[128] = ""; /* GOLDA CHANGED */

	addr = regPC;

	CPU_GetStatus();
	
#ifdef MONITOR_BREAK
	if (show_inst && !break_step) {
		/* break was caused by "O" command */
		break_addr = 0;
	}
	if (break_here) {
		printf("(Break due to BRK opcode)\n");
		show_inst = 1;
	}
	if (show_inst) { /* this part will disassemble actual instruction & show some hints */
		UWORD value = 0;
		UBYTE optype;

		if (break_ret)
			printf("(return)\n");

		show_inst = 0;
		show_instruction(regPC, 20);
		optype = optype6502[dGetByte(regPC)] >> 4;
		switch (optype) {
		case 0x1: /* abcd */
			value = dGetWord(regPC + 1);
			value = GetByte(value);
			break;
		case 0x2: /* #ab */
			value = dGetByte(regPC + 1);
			break;
		case 0x3: /* abcd,X */
			value = dGetWord(regPC + 1) + regX;
			value = GetByte(value);
			break;
		case 0x4: /* abcd,Y */
			value = dGetWord(regPC + 1) + regY;
			value = GetByte(value);
			break;
		case 0x5: /* (ab,X) */
			value = dGetWord((UBYTE) (dGetByte(regPC + 1) + regX));
			value = GetByte(value);
			break;
		case 0x6: /* (ab),Y */
			value = dGetWord(dGetByte(regPC + 1)) + regY;
			value = dGetByte(value);
			break;
		case 0x7: /* ab,X */
			value = dGetByte((UBYTE) (dGetByte(regPC + 1) + regX));
			break;
		case 0x8: /* ab,Y */
			value = dGetByte((UBYTE) (dGetByte(regPC + 1) + regY));
			break;
		case 0x9: /* branch */
			switch (dGetByte(regPC)) {
			case 0x10:  /* BPL */
				if (!(regP & N_FLAG))
					value = 1;
				break;
			case 0x30:  /* BMI */
				if (regP & N_FLAG)
					value = 1;
				break;
			case 0x50:  /* BVC */
				if (!(regP & V_FLAG))
					value = 1;
				break;
			case 0x70:  /* BVS */
				if (regP & V_FLAG)
					value = 1;
				break;
			case 0x90:  /* BCC */
				if (!(regP & C_FLAG))
					value = 1;
				break;
			case 0xb0:  /* BCS */
				if (regP & C_FLAG)
					value = 1;
				break;
			case 0xd0:  /* BNE */
				if (!(regP & Z_FLAG))
					value = 1;
				break;
			case 0xf0:  /* BEQ */
				if (regP & Z_FLAG)
					value = 1;
				break;
			}
			printf(value != 0 ? "(Y) " : "(N) ");
			break;
		case 0xb: /* RTS */
			value = dGetWord(0x100 + (UBYTE) (regS + 1)) + 1;
			break;
		case 0xc: /* RTI */
			value = dGetWord(0x100 + (UBYTE) (regS + 2));
			break;
		case 0xd: /* (abcd) */
			value = dGetWord(regPC + 1);
			value = dGetWord(value);
			break;
		case 0xe:
			printf("(ESC %02X) ", dGetByte(regPC + 1));
			value = dGetWord(0x100 + (UBYTE) (regS + 1)) + 1;
			break;
		case 0xf:
			printf("(ESC %02X) ", dGetByte(regPC + 1));
			break;
		}
		if (optype != 0x0 && optype != 0x9 && optype != 0xa && optype != 0xf
		 && dGetByte(regPC) != 0x4c && dGetByte(regPC) != 0x20)
			printf("(%04X)  ", value);
		show_regs();
	}
	else if (break_addr == regPC)
		printf("(breakpoint at %04X)\n", (unsigned int) break_addr);
	else if (break_cim)
		printf("(CIM encountered)\n");
	break_cim = 0;
	break_here = 0;
	break_step = 0;
	break_ret = 0;
#endif

	while (TRUE) {
		char *t;

		printf("> ");
		FFLUSH_STDOUT;
		fgets(s, sizeof(s), stdin);
		RemoveLF(s);
		if (s[0])
			memcpy(old_s, s, sizeof(s));
		else {
			int i;

			/* if no command is given, restart the last one, but remove all
			 * arguments, so after a 'm 600' we will see 'm 700' ...
			 */
			memcpy(s, old_s, sizeof(s));
			for (i = 0; i < (int) sizeof(s); i++)
				if (s[i] == ' ') {
					s[i] = '\0';
					break;
				}
		}
#ifdef HAVE_SYSTEM
		if (s[0] == '!') {
			system(s + 1);
			continue;
		}
#endif
		t = get_token(s);
		if (t == NULL)
			continue;

		{
			/* uppercase the command */
			char *p;
			for (p = t; *p != '\0'; p++)
				if (*p >= 'a' && *p <= 'z')
					*p += 'A' - 'a';
		}

		if (strcmp(t, "CONT") == 0) {
#ifdef PROFILE
			memset(instruction_count, 0, sizeof(instruction_count));
#endif
			return 1;
		}
#ifdef MONITOR_BREAK
		else if (strcmp(t, "BRKHERE") == 0) {
			char *brkarg;
			brkarg = get_token(NULL);
			if (brkarg) {
				if (strcmp(brkarg, "on") == 0) {
					brkhere = 1;
				}
				else if (strcmp(brkarg, "off") == 0) {
					brkhere = 0;
				}
				else {
					printf("invalid argument: usage: BRKHERE on|off\n");
				}
			}
			else {
				printf("BRKHERE is %s\n", brkhere ? "on" : "off");
			}
		}
		else if (strcmp(t, "BREAK") == 0) {
			get_hex(&break_addr);
		}
		else if (strcmp(t, "HISTORY") == 0) {
			int i;
			for (i = 0; i < REMEMBER_PC_STEPS; i++) {
				UWORD addr1, addr;
				int j;
				addr = remember_PC[(remember_PC_curpos + i) % REMEMBER_PC_STEPS];
				j = remember_xpos[(remember_PC_curpos + i) % REMEMBER_PC_STEPS];
				printf("%3d  ", j >> 8);
				printf("%3d  ", j & 0xff);
				printf("%04X ", addr);
				addr1 = show_instruction(addr, 20);
				printf("; %Xcyc ;", cycles[dGetByte(addr)]);
				for (j = 0; j < addr1; j++)
					printf(" %02X", dGetByte((UWORD) (addr + j)));
				putchar('\n');
			}
		}
		else if (strcmp(t, "JUMPS") == 0) {
			int i;
			for (i = 0; i < REMEMBER_JMP_STEPS; i++) {
				UWORD addr = remember_JMP[(remember_jmp_curpos + i) % REMEMBER_JMP_STEPS];
				printf("%04X\t", addr);
				show_instruction(addr, 20);
				putchar('\n');
			}
		}
#endif
#if defined(MONITOR_BREAK) || !defined(NO_YPOS_BREAK_FLICKER)
		else if (strcmp(t, "YBREAK") == 0) {
			get_dec(NULL, &ypos_break_addr);
		}
#endif
		else if (strcmp(t, "DLIST") == 0) {
			UWORD tdlist = dlist;
			int done = FALSE;
			int nlines = 0;
			get_hex(&tdlist);
			while (!done) {
				UBYTE IR;

				printf("%04X: ", tdlist);

				IR = ANTIC_GetDLByte(&tdlist);

				if (IR & 0x80)
					printf("DLI ");

				switch (IR & 0x0f) {
				case 0x00:
					printf("%d BLANK", ((IR >> 4) & 0x07) + 1);
					break;
				case 0x01:
					addr = ANTIC_GetDLWord(&tdlist);
					if (IR & 0x40) {
						printf("JVB %04X", addr);
						done = TRUE;
					}
					else {
						printf("JMP %04X", addr);
						tdlist = addr;
					}
					break;
				default:
					if (IR & 0x40) {
						addr = ANTIC_GetDLWord(&tdlist);
						printf("LMS %04X ", addr);
					}
					if (IR & 0x20)
						printf("VSCROL ");

					if (IR & 0x10)
						printf("HSCROL ");

					printf("MODE %X", IR & 0x0f);
				}

				putchar('\n');
				nlines++;

				if (!done && nlines == 15) {
					char buf[100];
					printf("Press return to continue ('q' to quit): ");
					FFLUSH_STDOUT;
					fgets(buf, sizeof(buf), stdin);
					done = buf[0] == 'q' || buf[0] == 'Q';
					nlines = 0;
				}
			}
		}
		else if (strcmp(t, "SETPC") == 0) {
			get_hex(&addr);

			regPC = addr;
		}
		else if (strcmp(t, "SETS") == 0) {
			get_hex(&addr);
			regS = addr & 0xff;
		}
		else if (strcmp(t, "SETA") == 0) {
			get_hex(&addr);
			regA = addr & 0xff;
		}
		else if (strcmp(t, "SETX") == 0) {
			get_hex(&addr);
			regX = addr & 0xff;
		}
		else if (strcmp(t, "SETY") == 0) {
			get_hex(&addr);
			regY = addr & 0xff;
		}
		else if (strcmp(t, "SETN") == 0) {
			get_hex(&addr);
			if (addr)
				SetN;
			else
				ClrN;
		}
		else if (strcmp(t, "SETV") == 0) {
			get_hex(&addr);
			if (addr)
				SetV;
			else
				ClrV;
		}
		else if (strcmp(t, "SETD") == 0) {
			get_hex(&addr);
			if (addr)
				SetD;
			else
				ClrD;
		}
		else if (strcmp(t, "SETI") == 0) {
			get_hex(&addr);
			if (addr)
				SetI;
			else
				ClrI;
		}
		else if (strcmp(t, "SETZ") == 0) {
			get_hex(&addr);
			if (addr)
				SetZ;
			else
				ClrZ;
		}
		else if (strcmp(t, "SETC") == 0) {
			get_hex(&addr);
			if (addr)
				SetC;
			else
				ClrC;
		}
#ifdef TRACE
		else if (strcmp(t, "TRON") == 0)
			tron = TRUE;
		else if (strcmp(t, "TROFF") == 0)
			tron = FALSE;
#endif
#ifdef PROFILE
		else if (strcmp(t, "PROFILE") == 0) {
			int i;

			for (i = 0; i < 24; i++) {
				int max, instr;
				int j;

				max = instruction_count[0];
				instr = 0;

				for (j = 1; j < 256; j++) {
					if (instruction_count[j] > max) {
						max = instruction_count[j];
						instr = j;
					}
				}

				if (max <= 0)
					break;
				instruction_count[instr] = 0;
				printf("Opcode %02X: %-10s has been executed %d times\n",
					   instr, instr6502[instr], max);
			}
		}
#endif
		else if (strcmp(t, "SHOW") == 0)
			show_regs();
		else if (strcmp(t, "STACK") == 0) {
			UWORD ts, ta;
			for (ts = 0x101 + regS; ts < 0x200; ) {
				if (ts < 0x1ff) {
					ta = dGetWord(ts);
					if (dGetByte(ta - 2) == 0x20) {
						printf("%04X : %02X %02X\t%04X : JSR %04X\n",
							ts, dGetByte(ts), dGetByte(ts + 1), ta - 2,
							dGetWord(ta - 1));
						ts += 2;
						continue;
					}
				}
				printf("%04X : %02X\n", ts, dGetByte(ts));
				ts++;
			}
		}
#ifndef PAGED_ATTRIB
		else if (strcmp(t, "ROM") == 0) {
			UWORD addr1;
			UWORD addr2;

			if (get_hex2(&addr1, &addr2) && addr1 <= addr2) {
				SetROM(addr1, addr2);
				printf("Changed Memory from %4x to %4x into ROM\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Bad or missing Parameter) ***\n");
			}
		}
		else if (strcmp(t, "RAM") == 0) {
			UWORD addr1;
			UWORD addr2;

			if (get_hex2(&addr1, &addr2) && addr1 <= addr2) {
				SetRAM(addr1, addr2);
				printf("Changed Memory from %4x to %4x into RAM\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Bad or missing Parameter) ***\n");
			}
		}
		else if (strcmp(t, "HARDWARE") == 0) {
			UWORD addr1;
			UWORD addr2;

			if (get_hex2(&addr1, &addr2) && addr1 <= addr2) {
				SetHARDWARE(addr1, addr2);
				printf("Changed Memory from %4x to %4x into HARDWARE\n",
					   addr1, addr2);
			}
			else {
				printf("*** Memory Unchanged (Bad or missing Parameter) ***\n");
			}
		}
#endif
		else if (strcmp(t, "COLDSTART") == 0) {
			Coldstart();
			return 1;	/* perform reboot immediately */
		}
		else if (strcmp(t, "WARMSTART") == 0) {
			Warmstart();
			return 1;	/* perform reboot immediately */
		}
#ifndef PAGED_MEM
		else if (strcmp(t, "READ") == 0) {
			char *filename;

			filename = get_token(NULL);
			if (filename) {
				int status = get_hex(&addr);
				if (status) {
					UWORD nbytes;
					status = get_hex(&nbytes);
					if (status && addr + nbytes <= 0x10000) {
						FILE *f;

						if (!(f = fopen(filename, "rb")))
							perror(filename);
						else {
							if (fread(&memory[addr], 1, nbytes, f) == 0)
								perror(filename);
							fclose(f);
						}
					}
				}
			}
		}
		else if (strcmp(t, "WRITE") == 0) {
			UWORD addr1;
			UWORD addr2;
			char *filename;
			int status;

			status = get_hex2(&addr1, &addr2);

			if (!(filename = get_token(NULL))) /* ERU */
				filename = "memdump.dat"; 

			if (status) {
				FILE *f;

				if (!(f = fopen(filename, "wb")))
					perror(filename);
				else {
					size_t nbytes = addr2 - addr1 + 1;
					if (fwrite(&memory[addr1], 1, addr2 - addr1 + 1, f) < nbytes)
						perror(filename);
					fclose(f);
				}
			}
		}
#endif
		else if (strcmp(t, "SUM") == 0) {
			UWORD addr1;
			UWORD addr2;

			if (get_hex2(&addr1, &addr2)) {
				int sum = 0;
				int i;

				for (i = addr1; i <= addr2; i++)
					sum += (UWORD) dGetByte(i);
				printf("SUM: %X\n", sum);
			}
		}
		else if (strcmp(t, "M") == 0) {
			UWORD xaddr1;
			int i;
			int count;
			if (get_hex(&xaddr1))
				addr = xaddr1;
			count = 16;
			while (count) {
				printf("%04X : ", addr);
				for (i = 0; i < 16; i++)
					printf("%02X ", GetByte((UWORD) (addr + i)));
				putchar('\t');
				for (i = 0; i < 16; i++) {
					UBYTE c;
					c = GetByte((UWORD) (addr + i));
					putchar(c >= ' ' && c <= 'z' && c != '\x60' ? c : '.');
				}
				putchar('\n');
				addr += 16;
				count--;
			}
		}
#ifndef PAGED_MEM
		else if (strcmp(t, "F") == 0) {
			int addr;
			int addr1;
			int addr2;
			UWORD xaddr1;
			UWORD xaddr2;
			UWORD hexval;

			get_hex3(&xaddr1, &xaddr2, &hexval);

			addr1 = xaddr1;
			addr2 = xaddr2;

			for (addr = addr1; addr <= addr2; addr++)
				dPutByte(addr, (UBYTE) hexval);
		}
#endif
		else if (strcmp(t, "S") == 0) {
			static int n = 0;
			static UWORD xaddr1;
			static UWORD xaddr2;
			UWORD hexval;
			static UBYTE tab[64];

			if (get_hex3(&xaddr1, &xaddr2, &hexval)) {
				n = 0;
				do {
					tab[n++] = (UBYTE) hexval;
					if (hexval & 0xff00 && n < 64)
						tab[n++] = (UBYTE) (hexval >> 8);
				} while (n < 64 && get_hex(&hexval));
			}
			if (n) {
				int addr;
				int addr1 = xaddr1;
				int addr2 = xaddr2;

				for (addr = addr1; addr <= addr2; addr++) {
					int i = 0;
					while (GetByte((UWORD) (addr + i)) == tab[i]) {
						i++;
						if (i >= n) {
							printf("Found at %04X\n", addr);
							break;
						}
					}
				}
			}
		}
#ifndef PAGED_MEM
		else if (strcmp(t, "C") == 0) {
			UWORD temp = 0;

			get_hex(&addr);

			while (get_hex(&temp)) {
#ifdef PAGED_ATTRIB
				if (writemap[addr >> 8] != NULL && writemap[addr >> 8] != ROM_PutByte)
					(*writemap[addr >> 8])(addr, (UBYTE) temp);
#else
				if (attrib[addr] == HARDWARE)
					Atari800_PutByte(addr, (UBYTE) temp);
#endif
				else /* RAM, ROM */
					dPutByte(addr, (UBYTE) temp);
				addr++;
				if (temp >> 8) {
#ifdef PAGED_ATTRIB
					if (writemap[addr >> 8] != NULL && writemap[addr >> 8] != ROM_PutByte)
						(*writemap[addr >> 8])(addr, (UBYTE) (temp >> 8));
#else
					if (attrib[addr] == HARDWARE)
						Atari800_PutByte(addr, (UBYTE) (temp >> 8));
#endif
					else /* RAM, ROM */
						dPutByte(addr, (UBYTE) (temp >> 8));
					addr++;
				}
			}
		}
#endif
#ifdef MONITOR_BREAK
		else if (strcmp(t, "G") == 0) {
			break_step = 1;
			show_inst = 1;
			return 1;
		}
		else if (strcmp(t, "R") == 0 ) {
			break_ret = 1;
			show_inst = 1;
			ret_nesting = 0;
			return 1;
		}
		else if (strcmp(t, "O") == 0) {
			/* with RTS, RTI, JMP, SKW, ESCRTS we simply do step */
			switch (dGetByte(regPC)) {
			case 0x60:
			case 0x40:
			case 0x0c:
			case 0x4c:
			case 0x6c:
			case 0xd2:
			case 0x1c:
			case 0x3c:
			case 0x5c:
			case 0x7c:
			case 0xdc:
			case 0xfc:
			case 0x9c:
				break_step = 1;
				show_inst = 1;
				return 1;
			default:
				show_inst = 1;
				break_addr = regPC + (optype6502[dGetByte(regPC)] & 0x3);
				return 1;
			}
		}
#endif /* MONITOR_BREAK */
		else if (strcmp(t, "D") == 0) {
			UWORD addr1;
			addr1 = addr;
			get_hex(&addr1);
			addr = disassemble(addr1, 0);
		}
		else if (strcmp(t, "LOOP") == 0) {
			UWORD addr1;
			UWORD addr2;
			int ok = FALSE;
			addr1 = addr;
			get_hex(&addr1);
			for (addr2 = addr1; addr2 <= (UWORD) (addr1 + 0x7e); ) {
				UBYTE opcode = dGetByte(addr2);
				if ((opcode & 0x1f) == 0x10) {
					/* branch */
					UWORD target = addr2 + 2 + (SBYTE) dGetByte(addr2 + 1);
					if (target <= addr1) {
						addr = disassemble(target, 0);
						ok = TRUE;
						break;
					}
				}
				addr2 += optype6502[opcode] & 3;
			}
			if (!ok)
				printf("Conditional loop containing instruction at %04X not detected\n", addr1);
		}
		else if (strcmp(t, "ANTIC") == 0) {
			printf("DMACTL=%02X    CHACTL=%02X    DLISTL=%02X    "
				   "DLISTH=%02X    HSCROL=%02X    VSCROL=%02X\n",
				   DMACTL, CHACTL, dlist & 0xff, dlist >> 8, HSCROL, VSCROL);
			printf("PMBASE=%02X    CHBASE=%02X    VCOUNT=%02X    "
				   "NMIEN= %02X    ypos=%4d\n",
				   PMBASE, CHBASE, ypos >> 1, NMIEN, ypos);
		}
		else if (strcmp(t, "PIA") == 0) {
			printf("PACTL= %02X    PBCTL= %02X    PORTA= %02X    "
				   "PORTB= %02X\n", PACTL, PBCTL, PORTA, PORTB);
		}
		else if (strcmp(t, "GTIA") == 0) {
			printf("HPOSP0=%02X    HPOSP1=%02X    HPOSP2=%02X    "
				   "HPOSP3=%02X    HPOSM0=%02X    HPOSM1=%02X\n",
				   HPOSP0, HPOSP1, HPOSP2, HPOSP3, HPOSM0, HPOSM1);
			printf("HPOSM2=%02X    HPOSM3=%02X    SIZEP0=%02X    "
				   "SIZEP1=%02X    SIZEP2=%02X    SIZEP3=%02X\n",
				   HPOSM2, HPOSM3, SIZEP0, SIZEP1, SIZEP2, SIZEP3);
			printf("SIZEM= %02X    GRAFP0=%02X    GRAFP1=%02X    "
				   "GRAFP2=%02X    GRAFP3=%02X    GRAFM =%02X\n",
				   SIZEM, GRAFP0, GRAFP1, GRAFP2, GRAFP3, GRAFM);
			printf("COLPM0=%02X    COLPM1=%02X    COLPM2=%02X    "
				   "COLPM3=%02X    COLPF0=%02X    COLPF1=%02X\n",
				   COLPM0, COLPM1, COLPM2, COLPM3, COLPF0, COLPF1);
			printf("COLPF2=%02X    COLPF3=%02X    COLBK= %02X    "
				   "PRIOR= %02X    VDELAY=%02X    GRACTL=%02X\n",
				   COLPF2, COLPF3, COLBK, PRIOR, VDELAY, GRACTL);
		}
		else if (strcmp(t, "POKEY") == 0) {
			printf("AUDF1= %02X    AUDF2= %02X    AUDF3= %02X    "
				   "AUDF4= %02X    AUDCTL=%02X    KBCODE=%02X\n",
				   AUDF[CHAN1], AUDF[CHAN2], AUDF[CHAN3], AUDF[CHAN4], AUDCTL[0], KBCODE);
			printf("AUDC1= %02X    AUDC2= %02X    AUDC3= %02X    "
				   "AUDC4= %02X    IRQEN= %02X    IRQST= %02X\n",
				   AUDC[CHAN1], AUDC[CHAN2], AUDC[CHAN3], AUDC[CHAN4], IRQEN, IRQST);
#ifdef STEREO_SOUND
			if (stereo_enabled) {
				printf("second chip:\n");
				printf("AUDF1= %02X    AUDF2= %02X    AUDF3= %02X    "
					   "AUDF4= %02X    AUDCTL=%02X\n",
					   AUDF[CHAN1 + CHIP2], AUDF[CHAN2 + CHIP2], AUDF[CHAN3 + CHIP2], AUDF[CHAN4 + CHIP2], AUDCTL[1]);
				printf("AUDC1= %02X    AUDC2= %02X    AUDC3= %02X    "
					   "AUDC4= %02X\n",
					   AUDC[CHAN1 + CHIP2], AUDC[CHAN2 + CHIP2], AUDC[CHAN3 + CHIP2], AUDC[CHAN4 + CHIP2]);
			}
#endif
		}
#ifdef MONITOR_ASSEMBLER
		else if (strcmp(t,"A") == 0) {
			UWORD addr1;
			addr1 = addr;
			get_hex(&addr1);
			addr = assembler(addr1);
		}
#endif
		else if (strcmp(t, "HELP") == 0 || strcmp(t, "?") == 0) {
			printf("CONT                           - Continue emulation\n");
			printf("SHOW                           - Show registers\n");
			printf("STACK                          - Show stack\n");
			printf("SET{PC,A,S,X,Y} hexval         - Set register value\n");
			printf("SET{N,V,D,I,Z,C} hexval        - Set flag value\n");
			printf("C startaddr hexval...          - Change memory\n");
			printf("D [startaddr]                  - Disassemble memory\n");
			printf("F startaddr endaddr hexval     - Fill memory\n");
			printf("M [startaddr]                  - Memory list\n");
			printf("S startaddr endaddr hexval...  - Search memory\n");
			printf("LOOP [inneraddr]               - Disassemble a loop that contains inneraddr\n");
			printf("ROM startaddr endaddr          - Convert memory block into ROM\n");
			printf("RAM startaddr endaddr          - Convert memory block into RAM\n");
			printf("HARDWARE startaddr endaddr     - Convert memory block into HARDWARE\n");
			printf("READ filename startaddr nbytes - Read file into memory\n");
			printf("WRITE startaddr endaddr [file] - Write memory block to a file (memdump.dat)\n");
			printf("SUM startaddr endaddr          - SUM of specified memory range\n");
#ifdef TRACE
			printf("TRON                           - Trace on\n");
			printf("TROFF                          - Trace off\n");
#endif
#ifdef MONITOR_BREAK
			printf("BREAK [addr]                   - Set breakpoint at address\n");
			printf("YBREAK [ypos] or [1000+ypos]   - Break at scanline or flash scanline\n");

 			printf("BRKHERE on|off                 - Set BRK opcode behaviour\n");
			printf("HISTORY                        - Disasm. last %d PC addrs. giving ypos xpos\n", (int) REMEMBER_PC_STEPS);
			printf("Press return to continue: ");
			FFLUSH_STDOUT;
			getchar();

			printf("JUMPS                          - List last %d locations of JMP/JSR\n", (int) REMEMBER_JMP_STEPS);
			printf("G                              - Execute 1 instruction\n");
			printf("O                              - Step over the instruction\n");
			printf("R                              - Execute until return\n");
#elif !defined(NO_YPOS_BREAK_FLICKER)
			printf("YBREAK [1000+ypos]             - flash scanline\n");
#endif
#ifdef MONITOR_ASSEMBLER
			printf("A [startaddr]                  - Start simple assembler\n");
#endif
			printf("ANTIC, GTIA, PIA, POKEY        - Display hardware registers\n");
			printf("DLIST [startaddr]              - Show Display List\n");
#ifdef PROFILE
			printf("PROFILE                        - Display profiling statistics\n");
#endif
			printf("COLDSTART, WARMSTART           - Perform system coldstart/warmstart\n");
#ifdef HAVE_SYSTEM
			printf("!command                       - Execute shell command\n");
#endif
			printf("QUIT                           - Quit emulator\n");
			printf("HELP or ?                      - This text\n");
		}
		else if (strcmp(t, "QUIT") == 0) {
			return 0;
		}
		else {
			printf("Invalid command.\n");
		}
	}
}

unsigned int disassemble(UWORD addr1, UWORD addr2)
{
	UWORD i;
	int count;

	addr = addr1;
	count = 24;

	while (count) {
		printf("%04X\t", addr);
		addr1 = show_instruction(addr, 20);
		printf("; %Xcyc ; ", cycles[dGetByte(addr)]);
		for (i = 0; i < addr1; i++)
			printf("%02X ", dGetByte((UWORD) (addr + i)));
		putchar('\n');
		addr += addr1;
		if (addr2 != 0 && addr >= addr2 )
			break;
		count--;
	}
	return addr;
}

#ifdef MONITOR_HINTS
static int find_symbol(UWORD addr)
{
	int lo = 0, hi = symtable_size - 1, mi = 0;
	
	while (lo < hi) {
		mi = (lo + hi) / 2;
		if (symtable[mi].addr == addr)
			break;
		else if (symtable[mi].addr > addr)
			hi = mi;
		else
			lo = mi + 1;
	}
	if (symtable[mi].addr == addr)
		/* return the lowest index of symbol with given address */
		return (mi > 0 && symtable[mi - 1].addr == addr) ? mi - 1 : mi;
	else
		return -1;
}
#endif

UWORD show_instruction(UWORD inad, int wid)
{
	UBYTE instr;
	UWORD value = 0;
	char dissbf[32];
	int i;
#ifdef MONITOR_HINTS
	int result;
	UBYTE operand = 0;
#endif

	instr = dGetByte(inad);
	strcpy(dissbf, instr6502[instr]);

	for (i = 0; dissbf[i] != 0; i++) {
		if (dissbf[i] == '$') {
			wid -= i;
			dissbf[i] = 0;
			printf(dissbf);
			switch (dissbf[i + 1]) {
			case '0':
				value = (UWORD) (inad + (char) dGetByte((UWORD) (inad + 1)) + 2);
				inad = 2;
				wid -= 5;
				printf("$%04X", value);
#ifdef MONITOR_HINTS
				operand = 1;
#endif
				break;
			case '1':
				value = (UBYTE) dGetByte((UWORD) (inad + 1));
				inad = 2;
				wid -= 3;
				printf("$%02X", value);
#ifdef MONITOR_HINTS
				operand = 1;
#endif
				break;
			case '2':
				value = (UWORD) dGetByte((UWORD) (inad + 1)) | (dGetByte((UWORD) (inad + 2)) << 8);
				inad = 3;
				wid -= 5;
				printf("$%04X", value);
#ifdef MONITOR_HINTS
				operand = 1;
#endif
				break;
			}
			printf(dissbf + i + 2);
			wid -= strlen(dissbf + i + 2);
			i = 0;
			break;
		}
	}
	if (dissbf[i] == 0) {
		printf(dissbf);
		wid -= i;
		inad = 1;
	}
#ifdef MONITOR_HINTS
	if (operand && (optype6502[instr] & 0xf0) != 0xa0 && (result = find_symbol(value)) >= 0) {
		/* different names when reading/writing memory */
		if ((optype6502[instr] & 0x08) && symtable[result + 1].addr == value)
			result++;
		printf(" ;%s ", symtable[result].name);
		wid -= 3 + strlen(symtable[result].name);
	}
#endif
	for (i = wid; i > 0; i--)
		printf(" ");
	return inad;
}


#ifdef MONITOR_ASSEMBLER
static UWORD assembler(UWORD addr)
{
	char s[128];  /* input string */
	char c[128];  /* converted input */
	char *sp, *cp;
	int i, value = 0;
	int oplen;
	int branch;

	printf("Simple assembler (enter empty line to exit)\n");
	while (TRUE) {
		printf("%04X : ",(int) addr);
		FFLUSH_STDOUT;
		fgets(s, sizeof(s), stdin);
		RemoveLF(s);
		if (s[0] == '\0')
			return addr;

		/* convert string to upper case */
		for (sp = s; *sp != '\0'; sp++)
			if (*sp >= 'a' && *sp <= 'z')
				*sp += 'A' - 'a';

		oplen = 0;

		cp = c;
		sp = s;
		/* copy first three characters */
		for (i = 0; i < 3 && *sp != '\0'; i++)
			*cp++ = *sp++;

		*cp = '\0'; /* temporarily put end of string here */
		branch = (strcmp(c, "BMI") == 0) || (strcmp(c, "BPL") == 0) || (strcmp(c, "BNE") == 0) ||
		         (strcmp(c, "BEQ") == 0) || (strcmp(c, "BVC") == 0) || (strcmp(c, "BVS") == 0) ||
		         (strcmp(c, "BCC") == 0) || (strcmp(c, "BCS") == 0);

		/* insert space before operands */
		*cp++ = ' ';

		/* convert input to format of instr6502[] table */
		while (*sp != '\0') {
			/* skip white spaces */
			while (*sp == ' ' || *sp == '\t')
				sp++;
			if ( (*sp >= '0' && *sp <= '9') || (*sp >= 'A' && *sp <= 'F')) {
				/* parse hexadecimal value */
				*cp++ = '$';  /* operands are marked with $ */
				i = 0;
				value = 0;
				while (TRUE)
					if (*sp >= '0' && *sp <= '9') {
						i++;
						value = value * 16 + (*sp++ - '0');
					}
					else if (*sp >= 'A' && *sp <= 'F') {
						i++;
						value = value * 16 + 10 + (*sp++ - 'A');
					}
					else
						break;

				if (branch) {
					/* relative address */
					*cp++ = '0';
					oplen = 1;
					value = value - (addr + 2);
				}
				else if (i <= 2) {
					/* zero page adress or immediate */
					*cp++ = '1';
					oplen = 1;
				}
				else {
					*cp++ = '2';
					oplen = 2;
				}
			} /* end of parsing hex.value */
			else if (*sp == '$' || *sp == '@')
				sp++;  /* ignore $ or @ */
			else
				*cp++ = *sp++;  /* if the char is not a digit, copy it to the output */
			 
		} /* end of converting input */
		if (cp[-1] == ' ')
			cp--; /* no arguments (e.g. NOP or ASL @) */
		*cp = '\0';  /* terminate output */

		/* search table for instruction */
		for (i = 0; i < 256 && strcmp(instr6502[i], c) != 0; i++);

		if (i < 256) {
			/* instruction found */
			switch (oplen) {
			case 0:
				dPutByte(addr, (UBYTE) i);
				addr++;
				break;
			case 1:
				if (value < -128 || value > 255)
					printf("Operand out of range!\n");
				else {
					dPutByte(addr, (UBYTE) i);
					addr++;
					dPutByte(addr, (UBYTE) value);
					addr++;
				}
				break;
			case 2:
				if (value < 0 || value > 65535)
					printf("Operand out of range!\n");
				else {
					dPutByte(addr, (UBYTE) i);
					addr++;
					dPutWord(addr, (UWORD) value);
					addr += 2;
				}
				break;
			}
		}
		else
			printf("Invalid instruction!\n");
	}
}
#endif

/*
$Log$
Revision 1.25  2005/08/22 21:42:42  pfusik
fixed linking error without MONITOR_BREAK defined

Revision 1.24  2005/08/22 20:48:13  pfusik
avoid <ctype.h>

Revision 1.23  2005/08/21 15:47:06  pfusik
use fflush(stdout) where necessary;
normalized all hex output to uppercase;
improved "JUMPS": now displays the instructions;
improved "PROFILE": now displays the instructions;
accept "ASL @" in assembler

Revision 1.22  2005/08/18 23:32:23  pfusik
"LOOP"; addr defaults to regPC

Revision 1.21  2005/08/17 22:34:25  pfusik
fixed VC6 warning; "!command" in HELP only if supported

Revision 1.20  2005/08/16 23:10:52  pfusik
#include "config.h" before system headers;
#ifdef HAVE_FFLUSH; #ifdef HAVE_SYSTEM

Revision 1.19  2005/08/15 17:23:22  pfusik
HISTORY displays ypos xpos also with NEW_CYCLE_EXACT disabled;
"C", "M" and "S" support hardware registers;
display correct value for ZP,Y instruction;
PENL -> PENV; fixed indentation

Revision 1.18  2005/08/10 19:42:08  pfusik
support DL in extended memory with exclusive Antic access

Revision 1.17  2005/08/06 18:30:38  pfusik
fixed indenting; fixed a warning

Revision 1.16  2005/03/08 04:32:41  pfusik
killed gcc -W warnings

Revision 1.15  2003/02/24 09:33:04  joy
header cleanup

Revision 1.14  2003/02/19 14:07:46  joy
configure stuff cleanup

Revision 1.13  2003/02/10 10:00:59  joy
include added

Revision 1.12  2003/01/27 14:13:02  joy
Perry's cycle-exact ANTIC/GTIA emulation

Revision 1.11  2003/01/27 13:14:53  joy
Jason's changes: either PAGED_ATTRIB support (mostly), or just clean up.

Revision 1.10  2002/07/04 12:40:19  pfusik
fixed optype6502[] to match unofficial instructions

Revision 1.9  2002/03/19 13:17:02  joy
Piotrs changes

Revision 1.8  2001/10/29 17:56:05  fox
"DLIST" didn't stopped on JVB if Display List had multiple of 15 instructions

Revision 1.7  2001/07/20 19:57:07  fox
not displaying rom_inserted in "PIA" command

Revision 1.3  2001/03/25 06:57:36  knik
open() replaced by fopen()

Revision 1.2  2001/03/18 06:34:58  knik
WIN32 conditionals removed

*/
