/* libunwind: gralloc solo lo usa para sacar trazas si algo falla. Decimos que no. */
typedef struct { int x; } cursor;
int _Uarm_init_local(void *c, void *ctx) { return -1; }
int _Uarm_step(void *c) { return 0; }
int _Uarm_get_reg(void *c, int reg, unsigned long *v) { if (v) *v = 0; return -1; }
int _Uarm_get_proc_name_by_ip(void *as, unsigned long ip, char *buf, unsigned long len,
                               unsigned long *off, void *arg) { return -1; }
void *_Uarm_local_addr_space;
