#ifndef _DOS_SB_H_
#define _DOS_SB_H_

typedef void (*sbmix_t)(void *buffer, int size);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int  sb_init(int *sample_rate, int *bps, int *buf_size, int *stereo);
extern void sb_shutdown(void);
extern int  sb_startoutput(sbmix_t fillbuf);
extern void sb_stopoutput(void);
extern void sb_setrate(int rate);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _DOS_SB_H_ */

/*
** $Log$
** Revision 1.2  2003/02/24 09:33:22  joy
** header cleanup
**
** Revision 1.1.1.1  2000/10/10 13:27:18  joy
** Imported using TkCVS
**
** Revision 1.7  2000/08/11 01:40:33  matt
** major rewrite - far cleaner, and fixed an SB detection reboot bug =(
**
** Revision 1.6  2000/07/17 01:52:30  matt
** made sure last line of all source files is a newline
**
** Revision 1.5  2000/07/10 13:52:21  matt
** oops...  boolean was inappropriately typed
**
** Revision 1.4  2000/06/09 15:12:27  matt
** initial revision
**
*/
