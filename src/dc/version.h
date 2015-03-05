/* provides Atari800DC version */

#ifndef __VERSIONDC_H_
#define __VERSIONDC_H_

#define A800DCBETA    1      /* set to 1 for beta versions, to 0 for release versions */
#define A800DCVERHI   0
#define A800DCVERLO   79
#define A800DCVERASC1 "0.79"
#if A800DCBETA
#define A800DCVERASC  A800DCVERASC1 "beta"
#else
#define A800DCVERASC  A800DCVERASC1
#endif

#endif /* #ifndef __VERSIONDC_H_ */
