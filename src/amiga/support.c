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

#include <string.h>

#include <libraries/gadtools.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

/**************************************************************************
 Some MUI support functions
**************************************************************************/
LONG xget(Object *obj,ULONG attribute)
{
	LONG x;
	get(obj,attribute,&x);
	return(x);
}

char *getstr(Object *obj)
{
	return((char *)xget(obj,MUIA_String_Contents));
}

BOOL getbool(Object *obj)
{
	return((BOOL)xget(obj,MUIA_Selected));
}

Object *MUI_NewObject          (char *classname,Tag tag1,...)
{
	return MUI_NewObjectA(classname, (struct TagItem*)&tag1);
}

BOOL MUI_AslRequestTags(APTR requester, Tag Tag1, ...)
{
	return MUI_AslRequest(requester, (struct TagItem*)&Tag1);
}

Object *MUI_MakeObject         (LONG type,...)
{
	return MUI_MakeObjectA(type, ((ULONG*)&type)+1);
}

LONG    MUI_Request            (APTR app,APTR win,LONGBITS flags,char *title,char *gadgets,char *format,...)
{
	return MUI_RequestA(app,win,flags,title,gadgets,format,((ULONG*)&format)+1);
}


Object *MakeLabel(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,0)); }
Object *MakeLabel1(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_SingleFrame)); }
Object *MakeLabel2(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_DoubleFrame)); }
Object *MakeLLabel(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_LeftAligned)); }
Object *MakeLLabel1(STRPTR str)  { return(MUI_MakeObject(MUIO_Label,str,MUIO_Label_LeftAligned|MUIO_Label_SingleFrame)); }

Object *MakeCheck(BOOL check)
{
	Object *obj = MUI_MakeObject(MUIO_Checkmark,NULL);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeButton(STRPTR str)
{
	Object *obj = MUI_MakeObject(MUIO_Button,str);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeCycle(STRPTR *array)
{
	Object *obj = MUI_MakeObject(MUIO_Cycle, NULL, array);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}

Object *MakeString(STRPTR def, LONG maxlen)
{
	Object *obj = MUI_MakeObject(MUIO_String,NULL,maxlen);

	if (obj)
	{
		SetAttrs(obj,
			MUIA_CycleChain,1,
			MUIA_String_Contents,def,
			TAG_DONE);
	}

	return(obj);
}

/*Object *MakeInteger(LONG num)
{
	Object *obj = MUI_MakeObject(MUIO_String,NULL,10);

	if (obj)
	{
		SetAttrs(obj,
			MUIA_CycleChain,1,
  		MUIA_String_Accept, "0123456789",
  		MUIA_String_Integer,0,//num,
			TAG_DONE);
	}

	return(obj);
}*/

Object *MakeImageButton(ULONG image)
{
	return ImageObject,ImageButtonFrame,
								MUIA_Image_Spec,image,
								MUIA_Background, MUII_ButtonBack,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
								MUIA_Image_FreeVert, TRUE,
								MUIA_CycleChain,1,
								End;

}

Object *MakeLV(APTR pool)
{
	return(
		ListviewObject,
			MUIA_Listview_Input, TRUE,
			MUIA_Listview_List , ListObject,
				InputListFrame,
				MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
				MUIA_List_DestructHook , MUIV_List_DestructHook_String,
				pool?MUIA_List_Pool:TAG_IGNORE,pool,
				End,
			End
	);
}

ULONG DoSuperNew(struct IClass *cl,Object *obj,ULONG tag1,...)
{
	return(DoSuperMethod(cl,obj,OM_NEW,&tag1,NULL));
}

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
	struct Screen *defscr = LockPubScreen( NULL );
	struct ViewPort *vp = NULL;
	ULONG displayID;
	if( defscr ) vp = &defscr->ViewPort;

	displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									BIDTAG_MonitorID, GetVPModeID( vp ) & MONITOR_ID_MASK,
									TAG_DONE);

	if( displayID == INVALID_ID )
	{
		displayID = BestModeID( BIDTAG_Depth,depth,
									BIDTAG_NominalWidth, width,
									BIDTAG_NominalHeight, height,
									TAG_DONE);
	}

	

	if( defscr ) UnlockPubScreen( NULL, defscr );
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

