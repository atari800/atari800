#ifndef _SCREEN_H_
#define _SCREEN_H_

void Screen_FindScreenshotFilename(char *buffer);
int Screen_SaveScreenshot(const char *filename, int interlaced);
void Screen_SaveNextScreenshot(int interlaced);

#endif /* _SCREEN_H_ */
