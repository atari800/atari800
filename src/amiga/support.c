/*
 * support.c - user interface support code
 *
 * Copyright (c) 2000 Sebastian Bauer
 * Copyright (c) 2000-2003 Atari800 development team (see DOC/CREDITS)
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

#define __USE_INLINE__
#define DoSuperMethod IDoSuperMethod
#define DoSuperMethodA IDoSuperMethodA
#define DoMethod IDoMethod
#define DoMethodA IDoMethodA

#include <string.h>

#include <libraries/gadtools.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/**************************************************************************
 Some general supports
**************************************************************************/
LONG StrLen( const STRPTR str)
{
	if(str) return (LONG)strlen(str);
	return 0;
}

STRPTR StrCopy( const STRPTR str )
{
	STRPTR dst;
	if( !str ) return NULL;
	if( !*str) return NULL;

	dst = (STRPTR)AllocVec(strlen(str)+1,0);
	if(dst) strcpy(dst,str);
	return dst;
}

STRPTR GetFullPath( STRPTR drw, STRPTR file)
{
	WORD dl = StrLen(drw);
	WORD fl = StrLen( file );
	LONG length = dl + fl + 6;
	STRPTR fp = (STRPTR)AllocVec( length+1, 0 );

	if( fp )
	{
		strcpy( fp, drw );

		if( AddPart( fp, file, length ))	return fp;
		else FreeVec( fp );
	}
	return NULL;
}

STRPTR AddSuffix(const STRPTR name, const STRPTR suf)
{
	STRPTR str;
	if(!strstr(name,suf))
	{
		LONG len = StrLen(name)+StrLen(suf)+2;
		str = (STRPTR)AllocVec(len,0);
		if(str)
		{
			strcpy(str,name);
			strcat(str,suf);
		}
	}	else str = StrCopy(name);
	return str;
}

ULONG GetBestID( ULONG width, ULONG height, ULONG depth )
{
	struct Screen *defscr;
	ULONG displayID;

	if ((defscr = LockPubScreen(NULL)))
	{
		struct ViewPort *vp;

		vp = &defscr->ViewPort;

		displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									BIDTAG_MonitorID, GetVPModeID( vp ) & MONITOR_ID_MASK,
									TAG_DONE);

		UnlockPubScreen( NULL, defscr );
	} else displayID = INVALID_ID;

	if (displayID == INVALID_ID)
	{
		displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									TAG_DONE);
	}
	return displayID;
}

STRPTR GetDisplayName(ULONG displayid)
{
	STATIC struct NameInfo DisplayNameInfo;
	STATIC char DisplayNameBuffer[256];

	LONG i, v;

	i	= 0;
	v	= GetDisplayInfoData(NULL,	(UBYTE *) &DisplayNameInfo, sizeof(DisplayNameInfo),
									DTAG_NAME, displayid);

	if(v > sizeof(struct QueryHeader))
    {
		for(; (i < sizeof(DisplayNameBuffer) - 1) && DisplayNameInfo.Name[i]; i++)
			DisplayNameBuffer[i]	= DisplayNameInfo.Name[i];
	}

	if(displayid == INVALID_ID)
		strcpy(DisplayNameBuffer, "InvalidID"/*GetMessage(MSG_INVALID)*/);
	else
	{
		if(i < sizeof(DisplayNameBuffer) - sizeof(" (0x00000000)"))
		{
			DisplayNameBuffer[i++]	= ' ';
			DisplayNameBuffer[i++]	= '(';
			DisplayNameBuffer[i++]	= '0';
			DisplayNameBuffer[i++]	= 'x';

			for(v = 28; (v >= 0) && (!((displayid >> v) & 0xf)); v -= 4);

			if(v < 0)
				DisplayNameBuffer[i++]	= '0';

			for(; (v >= 0); v -= 4)
			{
				if(((displayid >> v) & 0xf) > 9)
					DisplayNameBuffer[i++]	= ((displayid >> v) & 0xf) + 'a' - 10;
				else
					DisplayNameBuffer[i++]	= ((displayid >> v) & 0xf) + '0';
			}
			DisplayNameBuffer[i++]	= ')';
		}

		DisplayNameBuffer[i++]	= 0;
	}

	return DisplayNameBuffer;
}

APTR FindUserData( struct Menu *menu, APTR userdata)
{
	while(menu)
	{
		struct MenuItem *mi;

		if(GTMENU_USERDATA( menu ) == userdata) return menu;

		mi = menu->FirstItem;
		while(mi)
		{
			struct MenuItem *smi;

			if(GTMENUITEM_USERDATA( mi ) == userdata) return mi;

			smi = mi->SubItem;
			while(smi)
			{
				if(GTMENUITEM_USERDATA( smi ) == userdata) return smi;
				smi = smi->NextItem;
			}
			mi = mi->NextItem;
		}
		menu = menu->NextMenu;
	}
	return NULL;
}


/**************************************************************************
 ...
**************************************************************************/
struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr)
{
	struct Library *lib = OpenLibrary(name,version);
	struct Interface *iface;
	if (!lib) return NULL;

	iface = GetInterface(lib,"main",1,NULL);
	if (!iface)
	{
		CloseLibrary(lib);
		return NULL;
	}
	*((struct Interface**)interface_ptr) = iface;
	return lib;
}

/**************************************************************************
 ...
**************************************************************************/
void CloseLibraryInterface(struct Library *lib, void *interface)
{
	DropInterface(interface);
	CloseLibrary(lib);
}
