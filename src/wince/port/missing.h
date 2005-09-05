/* Missing types and forward declarations for windows ce port */

#ifndef _WCEMISSING_H_
#define _WCEMISSING_H_

/* Headers not present in Windows CE SDK */

/**************** dirent.h ***************/
/* It would not be a bad idea to take this thing from gcc distro and port
   it properly. For now only required part is ported. */

struct dirent
{
	long		d_ino;		/* Always zero. */
	unsigned short	d_reclen;	/* Always zero. */
	unsigned short	d_namlen;	/* Length of name in d_name. */
	char*		d_name; 	/* File name. */
	/* NOTE: The name in the dirent structure points to the name in the
	 *		 finddata_t structure in the DIR. */
};

/*
 * This is an internal data structure. Good programmers will not use it
 * except as an argument to one of the functions below.
 */
typedef struct
{
	/* disk transfer area for this dir */
/*	struct _finddata_t	dd_dta; */

	/* dirent struct to return from dir (NOTE: this makes this thread
	 * safe as long as only one thread uses a particular DIR struct at
	 * a time) */
	struct dirent		dd_dir;

	/* _findnext handle */
	long			dd_handle;

	/*
		 * Status of search:
	 *	 0 = not started yet (next entry to read is first entry)
	 *	-1 = off the end
	 *	 positive = 0 based index of next entry
	 */
	short			dd_stat;

	/* given path for dir with search pattern (struct is extended) */
	char			dd_name[1];
} DIR;


DIR*		opendir (const char*);
struct dirent*	readdir (DIR*);
int 	closedir (DIR*);



/**************** errno.h ***************/
extern int errno;

#define ENOENT 1
#define ENOTEMPTY 2


/**************** time.h ***************/
struct timeval
{
	int tv_sec;
	int tv_usec;
};

void gettimeofday(struct timeval* tp, void* dummy);
void usleep(long usec);
#include <stdlib.h>

struct tm
{
	short tm_year;
	short tm_mon;
	short tm_mday;
	short tm_wday;
	short tm_hour;
	short tm_min;
	short tm_sec;
};

time_t time(time_t* dummy);
struct tm* localtime(time_t* dummy);

unsigned int clock();



/**************** types.h ***************/
typedef unsigned short _ino_t;
typedef unsigned int _dev_t;
typedef long _off_t;

/**************** stat.h ***************/
struct stat {
	_dev_t st_dev;
	_ino_t st_ino;
	unsigned short st_mode;
	short st_nlink;
	short st_uid;
	short st_gid;
	_dev_t st_rdev;
	_off_t st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
};


#define _S_IFDIR        0040000         /* directory */
#define S_IFDIR  _S_IFDIR

int stat(const char *, struct stat *);




/* some extra defines not covered in config.h */
#define NO_YPOS_BREAK_FLICKER
#define DIRTYRECT
#define SUPPORTS_ATARI_CONFIGINIT
#define DONT_USE_RTCONFIGUPDATE

/* workarounds and old code */
#define stricmp _stricmp
#define CLK_TCK	1000
#define snprintf _snprintf

#define perror(s) wce_perror(s)
void wce_perror(char* s);

#ifndef _FILE_DEFINED
   typedef void FILE;
   #define _FILE_DEFINED
#endif
FILE* __cdecl wce_fopen(const char* fname, const char* fmode);
#define fopen wce_fopen
#define strerror(a) ""

/* Those are somewhat Unix-specific... */
#define S_IRUSR 1
#define S_IWUSR 2
#define S_IXUSR 2
#define S_IRGRP 1
#define S_IWGRP 2
#define S_IXGRP 2
#define S_IROTH 1
#define S_IWOTH 2
#define S_IXOTH 2

char* getenv(const char* varname);
int rename(const char* oldname, const char* newname);
int chmod(const char* name, int mode);
int fstat(int handle, struct stat* buffer);
int mkdir(const char* name, int param);
int rmdir(const char* name);
int umask(int pmode);
void rewind(FILE *stream);
char *tmpnam(char *string);
int remove(const char* path);
FILE *tmpfile();
time_t time(time_t* res);
char *strdup(const char *strSource);
char *getcwd(char *buffer, int maxlen);

/* truncation from int to char */
#pragma warning(disable:4305)

#endif
