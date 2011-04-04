/*
 * amiga.c - Amiga specific port code
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

#include <stdarg.h>
#include <stdio.h>

#include <exec/types.h>

#define ALL_REACTION_CLASSES
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include "gui.h"
#include "support.h"

struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *ListBrowserBase;
struct Library *TextEditorBase;
struct Library *LabelBase;
struct Library *StringBase;
struct Library *ScrollerBase;
struct Library *GetFileBase;
struct Library *ChooserBase;
struct Library *ClickTabBase;
struct Library *CheckBoxBase;
struct Library *GetScreenModeBase;

struct WindowIFace *IWindow;
struct LayoutIFace *ILayout;
struct ButtonIFace *IButton;
struct ListBrowserIFace *IListBrowser;
struct TextEditorIFace *ITextEditor;
struct LabelIFace *ILabel;
struct StringIFace *IString;
struct ScrollerIFace *IScroller;
struct GetFileIFace *IGetFile;
struct ChooserIFace *IChooser;
struct ClickTabIFace *IClickTab;
struct CheckBoxIFace *ICheckBox;
struct GetScreenModeIFace *IGetScreenMode;

#define LIBENTRY(name,version,base,iface) {name,version,(struct Library**)base,(struct Interface**) iface}
static struct
{
	STRPTR Name;
	ULONG Version;
	struct Library **Base;
	struct Interface **IFace;
} libraries[] =
{
	LIBENTRY("window.class",44,&WindowBase,&IWindow),
	LIBENTRY("gadgets/getfile.gadget",44,&GetFileBase, &IGetFile),
	LIBENTRY("gadgets/layout.gadget",44,&LayoutBase, &ILayout),
	LIBENTRY("gadgets/chooser.gadget",45,&ChooserBase, &IChooser),
	LIBENTRY("gadgets/clicktab.gadget",44,&ClickTabBase, &IClickTab),
	LIBENTRY("gadgets/button.gadget",44,&ButtonBase,&IButton),
	LIBENTRY("gadgets/listbrowser.gadget",44,&ListBrowserBase,&IListBrowser),
	LIBENTRY("gadgets/texteditor.gadget",50,&TextEditorBase,&ITextEditor),
	LIBENTRY("gadgets/string.gadget",44,&StringBase,&IString),
	LIBENTRY("gadgets/scroller.gadget",44,&ScrollerBase,&IScroller),
	LIBENTRY("gadgets/checkbox.gadget",44,&CheckBoxBase, &ICheckBox),
	LIBENTRY("gadgets/getscreenmode.gadget",44,&GetScreenModeBase, &IGetScreenMode),
	LIBENTRY("images/label.image",44,&LabelBase,&ILabel),
};

/****************************************
 Close libraries
*****************************************/
static void CloseGUILibs(void)
{
	int i;

	for (i=0;i<sizeof(libraries)/sizeof(libraries[0]);i++)
	{
		CloseLibraryInterface(*libraries[i].Base,*libraries[i].IFace);
		*libraries[i].Base = NULL;
		*libraries[i].IFace = NULL;
	}
}

/****************************************
 Open needed Libraries
*****************************************/
static int OpenGUILibs(void)
{
	int i;

	for (i=0;i<sizeof(libraries)/sizeof(libraries[0]);i++)
	{
		if (!(*libraries[i].Base = OpenLibraryInterface(libraries[i].Name,
												  libraries[i].Version,
												  libraries[i].IFace)))
		{
			printf("Unable to open %s version %ld\n",libraries[i].Name,libraries[i].Version);
			goto out;
		}
	}
	return 1;

out:
	CloseGUILibs();
	return 0;
}

/**********************************************************************
 Get Attribute easily
***********************************************************************/
static ULONG xget(Object * obj, ULONG attribute)
{
  ULONG x;
  IIntuition->GetAttr(attribute, obj, (ULONG*)&x);
  return (x);
}

/**********************************************************************
 Set attributes of a given object.
 Difference to RefreshSetGadgetAttrs() is that the parameters are all
 Objects.
***********************************************************************/
void RefreshSetObjectAttrsA(Object *o, Object *wo, struct TagItem *tags)
{
	struct Gadget *g = (struct Gadget*)o;
	struct Window *w = (struct Window*)xget(wo,WINDOW_Window);

	if (!o) return;

	IIntuition->RefreshSetGadgetAttrsA(g,w,NULL,tags);
}

void VARARGS68K RefreshSetObjectAttrs(Object *o, Object *wo, ...)
{
	ULONG *tags;
	va_list args;
	
	va_startlinear(args,wo);

	tags = va_getlinearva(args,ULONG*);
	RefreshSetObjectAttrsA(o,wo,(struct TagItem *)tags);
	va_end(args);
}

/****** GUI ********/
static Object *main_wnd;
static Object *displaytype_chooser;
static Object *screenmode_getscreenmode;
//static Object *bestscreenmode_checkbox;

static int quitting;

enum
{
	GID_QUIT = 1,
	GID_SAVE,
	GID_USE
};

/**********************************************************************
 Handle
***********************************************************************/
static int Handle(struct AtariConfig *config)
{
	int rc = 0;
	ULONG result;
	UWORD code;

	while ((result = RA_HandleInput(main_wnd, &code) ) != WMHI_LASTMSG )
	{
		switch (result & WMHI_CLASSMASK)
		{
			case	WMHI_CLOSEWINDOW:
					rc = 1;
					break;

			case	WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK)
					{
						case	GID_QUIT: rc = 1; quitting = 1; break;
						case	GID_SAVE: rc = 1; break;
						case	GID_USE: rc = 1; break;
					}
		}
	}
	return rc;
}

/*******************************
 Eventlloop
********************************/
static void Loop(struct AtariConfig *config)
{
	int ready = 0;
	ULONG mainMask;

	while (!ready)
	{
		ULONG mask;

		IIntuition->GetAttr(WINDOW_SigMask, main_wnd, &mainMask);

		mask = IExec->Wait(SIGBREAKF_CTRL_C | mainMask);

		if (mask & mainMask)
			ready = Handle(config);

		if (mask & SIGBREAKF_CTRL_C) ready = 1;
	}
}

/**********************************************************************
 Configure atari
***********************************************************************/
BOOL Configure(struct AtariConfig *config)
{
	static char * const tabLabels[] = {"General","Graphics","Sound","Paths",NULL};
	static char * const screenTypeLabels[] = {"Custom","Window", "Sizeable Window", NULL};

	if (!OpenGUILibs()) goto out;

	main_wnd = (Object*)WindowObject,
			WA_Title, "Atari800",
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_CloseGadget, TRUE,
			WA_SizeGadget, TRUE,
			WA_IDCMP, IDCMP_INTUITICKS,
//			WINDOW_IDCMPHook, &idcmpHook,
			WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_ParentGroup, VGroupObject,
				LAYOUT_SpaceOuter, TRUE,
				LAYOUT_DeferLayout, TRUE,

				LAYOUT_AddChild, ClickTabObject,
					GA_Text, &tabLabels,
					CLICKTAB_PageGroup, PageObject,
						PAGE_Add, VGroupObject,
						EndGroup,
						
						PAGE_Add, VGroupObject, /* Graphics */
							LAYOUT_AddChild, displaytype_chooser = ChooserObject,
								CHOOSER_LabelArray, screenTypeLabels,
								CHOOSER_Selected, 0,
							End,
							MemberLabel("Displaytype"),

							LAYOUT_AddChild, screenmode_getscreenmode = GetScreenModeObject,
							End,
							MemberLabel("Screenmode"),
							CHILD_WeightedHeight, 0,

							LAYOUT_AddChild, CheckBoxObject,
								CHECKBOX_Checked, config->UseBestID,
							End,
							MemberLabel("Use best screen mode"),
						EndGroup,
						CHILD_WeightedHeight, 0,

						PAGE_Add, VGroupObject,
							LAYOUT_AddChild, CheckBoxObject,
							End,
							MemberLabel("Enable Sound"),
						EndGroup,


						PAGE_Add, VGroupObject, /* Sound */
						EndGroup,
					End, /* Page */
				End, /* Clicktab */
				LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, Button("Start & Save",GID_SAVE),
					LAYOUT_AddChild, Button("Start & Use",GID_USE),
					LAYOUT_AddChild, Button("Quit",GID_QUIT),
				EndGroup,
				CHILD_WeightedHeight, 0,
			EndGroup,
		EndWindow;

	if (!main_wnd) goto out;

	RA_OpenWindow(main_wnd);

	Loop(config);

	IIntuition->DisposeObject(main_wnd);
	CloseGUILibs();
	return quitting?FALSE:TRUE;

out:
	IIntuition->DisposeObject(main_wnd);
	CloseGUILibs();
	return FALSE;
}


