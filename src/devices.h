#ifndef __DEVICES__
#define __DEVICES__

void Device_Initialise(int *argc, char *argv[]);

void Device_HHOPEN(void);
void Device_HHCLOS(void);
void Device_HHREAD(void);
void Device_HHWRIT(void);
void Device_HHSTAT(void);
void Device_HHSPEC(void);
void Device_HHINIT(void);

void Device_PHOPEN(void);
void Device_PHCLOS(void);
void Device_PHREAD(void);
void Device_PHWRIT(void);
void Device_PHSTAT(void);
void Device_PHSPEC(void);
void Device_PHINIT(void);

#endif
