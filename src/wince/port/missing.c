#include <windows.h>
#include <tchar.h>
#include <string.h>
#include "config.h"

char *getcwd(char *buffer, int maxlen);

char *tmpnam(char *string)
{
	TCHAR pTemp[MAX_PATH+1], cwdw[MAX_PATH+1];
	static char buffer[MAX_PATH+1], *fname;
	char cwd[MAX_PATH+1];

	getcwd(cwd, MAX_PATH);
	MultiByteToWideChar(CP_ACP, 0, cwd, -1, cwdw, MAX_PATH);
	GetTempFileName(cwdw, TEXT("A8_"), 0, pTemp);
	WideCharToMultiByte(CP_ACP, 0, pTemp, -1, buffer, MAX_PATH, NULL, NULL);
	fname = strrchr(buffer, '\\');
	if (!fname)
		fname = buffer;
	else
		fname++;
	
	if(string)
	{
		strncpy(string, fname, FILENAME_MAX);
		return string;
	}
	else
		return fname;
}

FILE *tmpfile()
{
	TCHAR pTemp[MAX_PATH+1], cwdw[MAX_PATH+1];
	char cwd[MAX_PATH+1];

	getcwd(cwd, MAX_PATH);
	MultiByteToWideChar(CP_ACP, 0, cwd, -1, cwdw, MAX_PATH);
	GetTempFileName(cwdw, TEXT("A8_"), 0, pTemp);
	return _wfopen(pTemp, TEXT("w+b"));
}

char cwd[MAX_PATH+1] = "";
char *getcwd(char *buffer, int maxlen)
{
	TCHAR fileUnc[MAX_PATH+1];
	char* plast;
	
	if(cwd[0] == 0)
	{
		GetModuleFileName(NULL, fileUnc, MAX_PATH);
		WideCharToMultiByte(CP_ACP, 0, fileUnc, -1, cwd, MAX_PATH, NULL, NULL);
		plast = strrchr(cwd, '\\');
		if(plast)
			*plast = 0;
		/* Special trick to keep start menu clean... */
		if(_stricmp(cwd, "\\windows\\start menu") == 0)
			strcpy(cwd, "\\Atari800");
	}
	if(buffer)
		strncpy(buffer, cwd, maxlen);
	return cwd;
}

/*
Windows CE fopen has non-standard behavior -- not
fully qualified paths refer to root folder rather
than current folder (concept not implemented in CE).
*/
#undef fopen

FILE* wce_fopen(const char* fname, const char* fmode)
{
	char fullname[MAX_PATH+1];
	
	if(!fname || fname[0] == '\0')
		return NULL;
	if(fname[0] != '\\' && fname[0] != '/')
	{
		getcwd(fullname, MAX_PATH);
		strncat(fullname, "\\", MAX_PATH-strlen(fullname)-1);
		strncat(fullname, fname, MAX_PATH-strlen(fullname)-strlen(fname));
		return fopen(fullname, fmode);
	}
	else
		return fopen(fname, fmode);
}

/* The only environment variable used by Atari800 at this point is HOME */
char* getenv(const char* varname)
{
	if(strcmp(varname, "HOME") == 0)
		return getcwd(NULL, 0);
	else
		return NULL;
}
