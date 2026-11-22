/* Precargada: saca el log de Android (que en la consola no existe) por stderr. */
#include <stdarg.h>
#include <stddef.h>
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
long write(int fd, const void *b, size_t n);
size_t strlen(const char *s);
static const char P[] = "??VDIWEFS";
static void sale(int prio, const char *tag, const char *txt)
{
    char l[1100];
    int n = 0;
    l[n++] = (prio >= 0 && prio < 9) ? P[prio] : '?';
    l[n++] = '/';
    for (const char *t = tag ? tag : "-"; *t && n < 60; t++) l[n++] = *t;
    l[n++] = ':'; l[n++] = ' ';
    for (const char *t = txt ? txt : ""; *t && n < 1090; t++) l[n++] = *t;
    if (n && l[n - 1] != '\n') l[n++] = '\n';
    write(2, l, n);
}
int __android_log_write(int prio, const char *tag, const char *txt) { sale(prio, tag, txt); return 1; }
int __android_log_buf_write(int b, int prio, const char *tag, const char *txt) { sale(prio, tag, txt); return 1; }
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap)
{
    char t[1024];
    vsnprintf(t, sizeof t, fmt, ap);
    sale(prio, tag, t);
    return 1;
}
int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); __android_log_vprint(prio, tag, fmt, ap); va_end(ap); return 1;
}
int __android_log_buf_print(int b, int prio, const char *tag, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); __android_log_vprint(prio, tag, fmt, ap); va_end(ap); return 1;
}
void __android_log_assert(const char *cond, const char *tag, const char *fmt, ...)
{
    char t[1024] = "assert";
    if (fmt) { va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap); }
    sale(7, tag, t);
    __builtin_trap();
}
