#ifndef DOWNLOAD_H_
#define DOWNLOAD_H_

#include "config.h"

#ifdef HAVE_DOWNLOAD

const char *Download_And_Extract(const char *url, const char *exts[], const char *dest_dir);

#endif /* HAVE_DOWNLOAD */

#endif /* DOWNLOAD_H_ */
