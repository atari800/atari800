#ifndef DOS_SB_H_
#define DOS_SB_H_

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

#endif /* DOS_SB_H_ */
