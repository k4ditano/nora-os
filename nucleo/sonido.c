/* sonido: ver sonido.h. Habla con el driver ALSA del kernel por ioctl (sin
 * alsa-lib); el driver de la RG35XX solo admite S32_LE. */
#include "sonido.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sound/asound.h>

#define TASA 44100
#define PERIODO 512                 /* ~11,6 ms por bloque */
#define NPERIODOS 4
#define MAXEF 8
#define MAXCACHE 32

typedef struct {                    /* un WAV de efecto ya convertido a 44,1 kHz estereo */
    char ruta[256];
    int16_t *s;                     /* intercalado L R */
    int marcos;
} clip_t;
typedef struct { clip_t *clip; int pos; float vol; } voz_t;
typedef struct {                    /* la musica: se lee del fichero */
    FILE *f;
    long datos, bytes;              /* inicio y tamaño del bloque "data" */
    int canales, tasa;
    long leidos;
    double frac;                    /* posicion fraccionaria para cambiar de frecuencia */
    int16_t a[2], b[2];             /* muestra actual y siguiente */
    float vol, fundido, objetivo;   /* fundido 0..1 hacia objetivo */
    char ruta[256];
} musica_t;

static int fd = -1;
static pthread_t hilo;
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static volatile int vivo;
static clip_t cache[MAXCACHE];
static int ncache;
static voz_t voces[MAXEF];
static musica_t mus;

/* ---------------- WAV ---------------- */
static int abre_wav(FILE *f, int *canales, int *tasa, long *datos, long *bytes)
{
    unsigned char h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return -1;
    int bits = 0;
    *canales = *tasa = 0;
    for (;;) {
        unsigned char c[8];
        if (fread(c, 1, 8, f) != 8) return -1;
        uint32_t tam = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            unsigned char fm[16];
            if (fread(fm, 1, 16, f) != 16) return -1;
            *canales = fm[2] | fm[3] << 8;
            *tasa = fm[4] | fm[5] << 8 | fm[6] << 16 | (int)fm[7] << 24;
            bits = fm[14] | fm[15] << 8;
            fseek(f, tam - 16 + (tam & 1), SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            *datos = ftell(f);
            *bytes = tam;
            break;
        } else
            fseek(f, tam + (tam & 1), SEEK_CUR);
    }
    if (bits != 16 || *canales < 1 || *canales > 2 || *tasa < 4000) return -1;
    return 0;
}

static clip_t *carga(const char *ruta)          /* con la cache; se llama con mx tomado */
{
    for (int i = 0; i < ncache; i++)
        if (!strcmp(cache[i].ruta, ruta)) return &cache[i];
    if (ncache >= MAXCACHE) return 0;
    FILE *f = fopen(ruta, "rb");
    if (!f) return 0;
    int can, tasa;
    long datos, bytes;
    if (abre_wav(f, &can, &tasa, &datos, &bytes)) { fclose(f); return 0; }
    int n = bytes / (2 * can);
    int16_t *in = malloc(bytes);
    if (!in || fread(in, 1, bytes, f) != (size_t)bytes) { free(in); fclose(f); return 0; }
    fclose(f);
    int m = (int)((long long)n * TASA / tasa);
    int16_t *out = malloc((size_t)m * 4);
    for (int i = 0; out && i < m; i++) {       /* frecuencia y canales, interpolacion lineal */
        double p = (double)i * tasa / TASA;
        int k = (int)p;
        double fr = p - k;
        int k2 = k + 1 < n ? k + 1 : k;
        for (int c = 0; c < 2; c++) {
            int cc = can == 2 ? c : 0;
            out[2 * i + c] = (int16_t)(in[k * can + cc] * (1 - fr) + in[k2 * can + cc] * fr);
        }
    }
    free(in);
    if (!out) return 0;
    clip_t *cl = &cache[ncache++];
    snprintf(cl->ruta, sizeof cl->ruta, "%s", ruta);
    cl->s = out;
    cl->marcos = m;
    return cl;
}

/* siguiente marco de la musica (estereo, 44,1 kHz); 0 si no hay musica */
static int lee_marco_fichero(int16_t o[2])
{
    int16_t s[2];
    if (mus.leidos + 2 * mus.canales > mus.bytes) {   /* al final: vuelta al principio */
        fseek(mus.f, mus.datos, SEEK_SET);
        mus.leidos = 0;
    }
    if (fread(s, 2, mus.canales, mus.f) != (size_t)mus.canales) return -1;
    mus.leidos += 2 * mus.canales;
    o[0] = s[0];
    o[1] = mus.canales == 2 ? s[1] : s[0];
    return 0;
}
static int marco_musica(int32_t o[2])
{
    if (!mus.f) return 0;
    mus.frac += (double)mus.tasa / TASA;
    while (mus.frac >= 1) {
        mus.a[0] = mus.b[0];
        mus.a[1] = mus.b[1];
        if (lee_marco_fichero(mus.b)) return 0;
        mus.frac -= 1;
    }
    /* fundido lento (unos 0,8 s) hacia el objetivo */
    float paso = 1.0f / (0.8f * TASA);
    if (mus.fundido < mus.objetivo) mus.fundido = mus.fundido + paso > mus.objetivo ? mus.objetivo : mus.fundido + paso;
    if (mus.fundido > mus.objetivo) mus.fundido = mus.fundido - paso < mus.objetivo ? mus.objetivo : mus.fundido - paso;
    if (mus.fundido <= 0 && mus.objetivo <= 0) {        /* fundido a cero: se cierra */
        fclose(mus.f);
        mus.f = 0;
        return 0;
    }
    float g = mus.vol * mus.fundido, fr = (float)mus.frac;
    o[0] += (int32_t)((mus.a[0] * (1 - fr) + mus.b[0] * fr) * g);
    o[1] += (int32_t)((mus.a[1] * (1 - fr) + mus.b[1] * fr) * g);
    return 1;
}

/* ---------------- ALSA ---------------- */
static void pon_mascara(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
    struct snd_mask *m = &p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
    memset(m->bits, 0, sizeof m->bits);
    m->bits[bit >> 5] |= 1u << (bit & 31);
}
static void pon_valor(struct snd_pcm_hw_params *p, int n, unsigned v)
{
    struct snd_interval *i = &p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    i->min = i->max = v;
    i->integer = 1;
}
static int abre_alsa(void)
{
    fd = open("/dev/snd/pcmC0D0p", O_RDWR);
    if (fd < 0) return -1;
    struct snd_pcm_hw_params hw;
    memset(&hw, 0, sizeof hw);
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_MASK; n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
        memset(hw.masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK].bits, 0xff, sizeof hw.masks[0].bits);
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
        hw.intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min = 0;
        hw.intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].max = UINT_MAX;
    }
    hw.rmask = ~0u;
    hw.info = ~0u;
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S32_LE);
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 32);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_FRAME_BITS, 64);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, 2);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_RATE, TASA);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, PERIODO);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_PERIODS, NPERIODOS);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0) {
        fprintf(stderr, "nora: sonido HW_PARAMS: %s\n", strerror(errno));
        close(fd);
        fd = -1;
        return -1;
    }
    struct snd_pcm_sw_params sw;
    memset(&sw, 0, sizeof sw);
    unsigned long buffer = PERIODO * NPERIODOS;
    sw.period_step = 1;
    sw.avail_min = PERIODO;
    sw.start_threshold = PERIODO;
    sw.stop_threshold = buffer;
    sw.boundary = buffer;
    while (sw.boundary * 2 <= (unsigned long)INT_MAX - buffer) sw.boundary *= 2;
    ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw);
    ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
    return 0;
}

static void *mezcla(void *x)
{
    (void)x;
    static int32_t bloque[PERIODO * 2];
    while (vivo) {
        pthread_mutex_lock(&mx);
        for (int i = 0; i < PERIODO; i++) {
            int32_t o[2] = {0, 0};
            marco_musica(o);
            for (int v = 0; v < MAXEF; v++) {
                voz_t *vz = &voces[v];
                if (!vz->clip) continue;
                o[0] += (int32_t)(vz->clip->s[2 * vz->pos] * vz->vol);
                o[1] += (int32_t)(vz->clip->s[2 * vz->pos + 1] * vz->vol);
                if (++vz->pos >= vz->clip->marcos) vz->clip = 0;
            }
            for (int c = 0; c < 2; c++) {          /* recortar y pasar a 32 bits */
                int32_t s = o[c] > 32767 ? 32767 : o[c] < -32768 ? -32768 : o[c];
                bloque[2 * i + c] = s << 16;
            }
        }
        pthread_mutex_unlock(&mx);
        unsigned char *p = (unsigned char *)bloque;
        snd_pcm_uframes_t quedan = PERIODO;
        while (quedan && vivo) {
            struct snd_xferi xf = {.buf = p, .frames = quedan};
            if (ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xf) < 0) {
                if (errno == EPIPE) { ioctl(fd, SNDRV_PCM_IOCTL_PREPARE); continue; }   /* se vacio */
                usleep(10000);
                break;
            }
            p += xf.result * 8;
            quedan -= xf.result;
        }
    }
    return 0;
}

int sonido_iniciar(void)
{
    if (abre_alsa()) return -1;
    vivo = 1;
    if (pthread_create(&hilo, 0, mezcla, 0)) { vivo = 0; return -1; }
    return 0;
}

void sonido_efecto(const char *wav, float vol)
{
    if (fd < 0 || !wav) return;
    pthread_mutex_lock(&mx);
    clip_t *c = carga(wav);
    if (c) {
        int libre = 0;
        for (int v = 0; v < MAXEF; v++)              /* una voz libre, o la mas avanzada */
            if (!voces[v].clip || (voces[libre].clip && voces[v].pos > voces[libre].pos)) {
                libre = v;
                if (!voces[v].clip) break;
            }
        voces[libre].clip = c;
        voces[libre].pos = 0;
        voces[libre].vol = vol;
    } else
        fprintf(stderr, "nora: no puedo cargar el sonido %s\n", wav);
    pthread_mutex_unlock(&mx);
}

void sonido_musica(const char *wav, float vol)
{
    if (fd < 0) return;
    pthread_mutex_lock(&mx);
    if (!wav || !*wav) {
        mus.objetivo = 0;                             /* fundido a cero y se cierra sola */
    } else if (mus.f && !strcmp(mus.ruta, wav)) {
        mus.vol = vol;                                /* la misma: solo el volumen */
        mus.objetivo = 1;
    } else {
        FILE *f = fopen(wav, "rb");
        int can, tasa;
        long datos, bytes;
        if (f && !abre_wav(f, &can, &tasa, &datos, &bytes)) {
            if (mus.f) fclose(mus.f);
            memset(&mus, 0, sizeof mus);
            mus.f = f;
            mus.datos = datos;
            mus.bytes = bytes;
            mus.canales = can;
            mus.tasa = tasa;
            mus.vol = vol;
            mus.objetivo = 1;
            snprintf(mus.ruta, sizeof mus.ruta, "%s", wav);
        } else {
            if (f) fclose(f);
            fprintf(stderr, "nora: no puedo abrir la musica %s\n", wav);
        }
    }
    pthread_mutex_unlock(&mx);
}

void sonido_cerrar(void)
{
    if (!vivo) return;
    vivo = 0;
    pthread_join(hilo, 0);
    if (fd >= 0) { ioctl(fd, SNDRV_PCM_IOCTL_DROP); close(fd); fd = -1; }
    if (mus.f) fclose(mus.f);
}
