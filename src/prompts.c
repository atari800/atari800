#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "prompts.h"

void GetString(char *message, char *string)
{
	char gash[128];

	printf(message, string);
	fgets(gash, sizeof(gash), stdin);
	RemoveLF(gash);
	if (strlen(gash) > 0)
		strcpy(string, gash);
}

void GetNumber(char *message, int *num)
{
	char gash[128];

	printf(message, *num);
	fgets(gash, sizeof(gash), stdin);
	RemoveLF(gash);
	if (strlen(gash) > 0)
		sscanf(gash, "\n%d", num);
}

void YesNo(char *message, char *yn)
{
	char gash[128];
	char t_yn;

	do {
		printf(message, *yn);
		fgets(gash, sizeof(gash), stdin);
		RemoveLF(gash);

		if (strlen(gash) > 0)
			t_yn = gash[0];
		else
			t_yn = ' ';

		if (islower(t_yn))
			t_yn = toupper(t_yn);
	} while ((t_yn != ' ') && (t_yn != 'Y') && (t_yn != 'N'));

	if (t_yn != ' ')
		*yn = t_yn;
}

void RemoveSpaces(char *string)
{
	char *ptr = string;

	while (*string) {
		switch (*string) {
		case ' ':
		case '\n':
		case '\t':
			string++;
			break;
		default:
			*ptr++ = *string++;
			break;
		}
	}

	*ptr = '\0';
}

void RemoveLF(char *string)
{
	int len;

	len = strlen(string);
	if (len >= 2 && string[len - 1] == '\n' && string[len - 2] == '\r')
		string[len - 2] = '\0';
	else if (len >= 1 && (string[len - 1] == '\n' || string[len - 1] == '\r'))
		string[len - 1] = '\0';
}
