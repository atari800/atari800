#ifndef __PROMPTS__
#define __PROMPTS__

void GetString(char *message, char *string);
void GetNumber(char *message, int *num);
void GetYesNo(char *message, char *yn);
void GetYesNoAsInt(char *message, int *num);
void RemoveLF(char *string);
void RemoveSpaces(char *string);

#endif
