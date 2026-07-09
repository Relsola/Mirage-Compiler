#include<stddef.h>

#define nullptr 0
#define _MAX_DRIVE 3
#define _MAX_DIR 256
#define SEEK_END 2

typedef struct FILE FILE;
FILE * __acrt_iob_func(unsigned);

#define stdin  (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))

struct stat {
  char _[512];
};

typedef struct {
  size_t gl_pathc;
  char **gl_pathv;
  size_t gl_offs;
  char _[512];
} glob_t;

typedef int                           errno_t;

int* _errno(void);
#define errno (*_errno())

int _splitpath_s(const char *_FullPath, char *_Drive, size_t _DriveCount, char *_Dir, size_t _DirCount, char *_Filename, size_t _FilenameCount, char *_Ext, size_t _ExtCount);

int _unlink(const char *_FileName);
void *malloc(long size);
void *calloc(long nmemb, long size);
void *realloc(void *buf, long size);
char *_strdup(const char *_Source);
int *__errno_location();
char *strerror(int errnum);
FILE *fopen(char *pathname, char *mode);
int fseek(FILE *_Stream, long _Offset, int _Origin);
long ftell(FILE *_Stream);
void rewind(FILE *_Stream);
int ferror(FILE *_Stream);
int fopen_s(FILE **_Stream, const char *_FileName, const char *_Mode);
int strncpy_s(char *_Destination, size_t _SizeInBytes, const char *_Source, size_t _MaxCount);
int strerror_s(char *_Buffer, size_t _SizeInBytes, int _ErrorNumber);
int _vscprintf(const char *const _Format, va_list _ArgList);
int vsnprintf(char *const _Buffer, const size_t _BufferCount, const char *const _Format, va_list _ArgList);
FILE *open_memstream(char **ptr, size_t *sizeloc);
long fread(void *ptr, long size, long nmemb, FILE *stream);
size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fflush(FILE *stream);
int fclose(FILE *fp);
int fputc(int c, FILE *stream);
int feof(FILE *stream);
int glob(char *pattern, int flags, void *errfn, glob_t *pglob);
void globfree(glob_t *pglob);
int stat(char *pathname, struct stat *statbuf);
char *dirname(char *path);
int strcmp(char *s1, char *s2);
int strncasecmp(char *s1, char *s2, long n);
int _strnicmp(char *s1, char *s2, long n);
int memcmp(char *s1, char *s2, long n);
int printf(char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
int fprintf(FILE *fp, char *fmt, ...);
int vfprintf(FILE *fp, char *fmt, va_list ap);
long strlen(char *p);
int strncmp(char *p, char *q, long n);
void *memcpy(char *dst, char *src, long n);
char *strdup(char *p);
char *strndup(char *p, long n);
char *strdup(char *p);
int isspace(int c);
int ispunct(int c);
int isdigit(int c);
int isxdigit(int c);
char *strstr(char *haystack, char *needle);
char *strchr(char *s, int c);
double strtod(char *nptr, char **endptr);
long strtoul(char *nptr, char **endptr, int base);
long strtoull(char *nptr, char **endptr, int base);
void exit(int code);
char *basename(char *path);
char *strrchr(char *s, int c);
int unlink(char *pathname);
int mkstemp(char *template);
int close(int fd);
int fork(void);
int execvp(char *file, char **argv);
void _exit(int code);
int wait(int *wstatus);
int atexit(void (*)(void));
FILE *open_memstream(char **ptr, size_t *sizeloc);
char *dirname(char *path);
char *strncpy(char *dest, char *src, long n);
int stat(char *pathname, struct stat *statbuf);
int stat(char *pathname, struct stat *statbuf);
char *dirname(char *path);
char *basename(char *path);
char *strrchr(char *s, int c);
int unlink(char *pathname);
int mkstemp(char *template);
int close(int fd);
int fork(void);
int execvp(char *file, char **argv);
void _exit(int code);
int wait(int *wstatus);
int atexit(void (*)(void));
