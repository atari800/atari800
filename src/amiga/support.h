#ifndef _SUPPORT_H_
#define _SUPPORT_H_

#define InnerWidth(w) (w->Width - w->BorderLeft - w->BorderRight)
#define InnerHeight(w) (w->Height - w->BorderTop - w->BorderBottom)

/* Some MUI Support Functions */

LONG xget(Object *obj,ULONG attribute);
char *getstr(Object *obj);
BOOL getbool(Object *obj);
Object *MUI_NewObject(char *classname,Tag tag1,...);
BOOL MUI_AslRequestTags(APTR requester, Tag Tag1, ...);
Object *MUI_MakeObject(LONG type,...);
LONG    MUI_Request(APTR app, APTR win, LONGBITS flags,
                    char *title,char *gadgets,char *format,...);
Object *MakeLabel(STRPTR str);
Object *MakeLabel1(STRPTR str);
Object *MakeLabel2(STRPTR str);
Object *MakeLLabel(STRPTR str);
Object *MakeLLabel1(STRPTR str);
Object *MakeCheck(BOOL check);
Object *MakeButton(STRPTR str);
Object *MakeCycle(STRPTR *array);
Object *MakeString(STRPTR def, LONG maxlen);
Object *MakeImageButton(ULONG image);
Object *MakeLV(APTR pool);

ULONG DoSuperNew(struct IClass *cl,Object *obj,ULONG tag1,...);

#define nnsetstring(obj,s)  nnset(obj,MUIA_String_Contents,s)

LONG StrLen( const STRPTR str);
STRPTR StrCopy( const STRPTR str );
STRPTR GetFullPath( STRPTR drw, STRPTR file);
STRPTR AddSuffix(const STRPTR name, const STRPTR suf);
ULONG GetBestID( ULONG width, ULONG height, ULONG depth );
STRPTR GetDisplayName(ULONG displayid);
APTR FindUserData( struct Menu *menu, APTR userdata);

#endif /* _SUPPORT_H_ */
