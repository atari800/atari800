#ifndef ACIDTEST_H_
#define ACIDTEST_H_

/* Automated Acid800 test suite checker, see test/acid800.atr */

extern int ACIDTEST_enabled;

void ACIDTEST_Init(const char *filename);
void ACIDTEST_Frame(void);

#endif /* ACIDTEST_H_ */
