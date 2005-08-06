#ifndef _CYCLE_MAP_H_
#define _CYCLE_MAP_H_

#define CPU2ANTIC_SIZE (114 + 9)
extern int cpu2antic[CPU2ANTIC_SIZE * (17 * 7 + 1)];
extern int antic2cpu[CPU2ANTIC_SIZE * (17 * 7 + 1)];
void create_cycle_map(void);

#endif /* _CYCLE_MAP_H_ */
