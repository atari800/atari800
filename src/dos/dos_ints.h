#ifndef _DOS_INTS_H_
#define _DOS_INTS_H_

/* Thanks, Allegro! */
#define  BPS_TO_TIMER(x)            (1193182L / (long)(x))
#define  END_OF_FUNCTION(x)         void x##_end(void) {}
#define  END_OF_STATIC_FUNCTION(x)  static void x##_end(void) {}
#define  LOCK_VARIABLE(x)           _go32_dpmi_lock_data((void*)&x, sizeof(x))
#define  LOCK_FUNCTION(x)           _go32_dpmi_lock_code(x, (long)x##_end - (long)x)

#define  DISABLE_INTS()             __asm__ __volatile__ ("cli")
#define  ENABLE_INTS()              __asm__ __volatile__ ("sti")

#endif /* _DOS_INTS_H_ */

/*
** $Log$
** Revision 1.2  2003/02/24 09:33:20  joy
** header cleanup
**
** Revision 1.1.1.1  2000/10/10 13:27:18  joy
** Imported using TkCVS
**
** Revision 1.6  2000/08/11 01:40:07  matt
** cleanups
**
** Revision 1.5  2000/07/17 01:52:30  matt
** made sure last line of all source files is a newline
**
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
