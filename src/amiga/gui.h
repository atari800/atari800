#ifndef _GUI_H
#define _GUI_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

struct AtariConfig
{

	/* Amiga */
	ULONG DisplayType;
	ULONG DisplayID;
	BOOL UseBestID;
};

BOOL Configure(struct AtariConfig *config);

#endif