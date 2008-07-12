#ifndef DOS_INTS_H_
#define DOS_INTS_H_

/* Thanks, Allegro! */
#define  BPS_TO_TIMER(x)            (1193182L / (long)(x))
#define  END_OF_FUNCTION(x)         void x##_end(void) {}
#define  END_OF_STATIC_FUNCTION(x)  static void x##_end(void) {}
#define  LOCK_VARIABLE(x)           _go32_dpmi_lock_data((void*)&x, sizeof(x))
#define  LOCK_FUNCTION(x)           _go32_dpmi_lock_code(x, (long)x##_end - (long)x)

#define  DISABLE_INTS()             __asm__ __volatile__ ("cli")
#define  ENABLE_INTS()              __asm__ __volatile__ ("sti")

#endif /* DOS_INTS_H_ */
