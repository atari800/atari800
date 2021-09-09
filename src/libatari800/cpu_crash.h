#ifndef LIBATARI800_API_H_
#define LIBATARI800_API_H_

#include <stdio.h>

#include "config.h"

#ifdef HAVE_SETJMP
#include <setjmp.h>
extern jmp_buf libatari800_cpu_crash;
#endif /* HAVE_SETJMP */

#include "libatari800/libatari800.h"

extern int libatari800_continue_on_brk;

#endif /* LIBATARI800_API_H_ */
