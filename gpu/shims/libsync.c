/* libsync: solo sync_merge, que pide gralloc. Fusiona dos fences del kernel. */
int ioctl(int fd, int req, ...);
char *strncpy(char *d, const char *s, unsigned long n);
struct sync_merge_data { int fd2; char name[32]; int fence; };
#define SYNC_IOC_MERGE 0xc0283e01   /* _IOWR('>', 1, struct sync_merge_data) */
int sync_merge(const char *name, int fd1, int fd2)
{
    struct sync_merge_data d;
    d.fd2 = fd2;
    strncpy(d.name, name ? name : "nora", sizeof d.name);
    d.fence = -1;
    if (ioctl(fd1, SYNC_IOC_MERGE, &d) < 0) return -1;
    return d.fence;
}
