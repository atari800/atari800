#ifndef _ATARIPCX_H_
#define _ATARIPCX_H_

/* Find unused filename for PCX screenshot
   Return pointer to static buffer or NULL on error */
char *Find_PCX_name(void);

/* Write PCX screenshot to a file
   Return TRUE on OK, FALSE on error */
UBYTE Save_PCX_file(int interlace, char *filename);

/* With these macros we don't need to modify existing sources :) */
#define Save_PCX(dummy) Save_PCX_file(FALSE, Find_PCX_name())
#define Save_PCX_interlaced() Save_PCX_file(TRUE, Find_PCX_name())

#endif /* _ATARIPCX_H_ */
