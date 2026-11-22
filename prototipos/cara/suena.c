/* suena: reproduce un WAV PCM de 16 bits en la RG35XX hablando directamente
 * con el driver ALSA del kernel (ioctl), sin alsa-lib ni aplay.
 *
 *     suena fichero.wav [dispositivo]      (por defecto /dev/snd/pcmC0D0p)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sound/asound.h>

static struct snd_mask *mascara(struct snd_pcm_hw_params *p, int n)
{
    return &p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}
static struct snd_interval *intervalo(struct snd_pcm_hw_params *p, int n)
{
    return &p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}
static void pon_mascara(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
    struct snd_mask *m = mascara(p, n);
    memset(m->bits, 0, sizeof m->bits);
    m->bits[bit >> 5] |= 1u << (bit & 31);
}
static void pon_valor(struct snd_pcm_hw_params *p, int n, unsigned v)
{
    struct snd_interval *i = intervalo(p, n);
    i->min = i->max = v;
    i->integer = 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uso: suena fichero.wav [dispositivo]\n"); return 1; }
    const char *disp = argc > 2 ? argv[2] : "/dev/snd/pcmC0D0p";

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }

    /* cabecera WAV: buscar "fmt " y "data" */
    unsigned char h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        fprintf(stderr, "no es un WAV\n");
        return 1;
    }
    unsigned canales = 0, tasa = 0, bits = 0;
    uint32_t datos = 0;
    for (;;) {
        unsigned char c[8];
        if (fread(c, 1, 8, f) != 8) { fprintf(stderr, "WAV sin datos\n"); return 1; }
        uint32_t tam = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            unsigned char fm[16];
            if (fread(fm, 1, 16, f) != 16) return 1;
            canales = fm[2] | fm[3] << 8;
            tasa = fm[4] | fm[5] << 8 | fm[6] << 16 | (unsigned)fm[7] << 24;
            bits = fm[14] | fm[15] << 8;
            fseek(f, tam - 16 + (tam & 1), SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            datos = tam;
            break;
        } else
            fseek(f, tam + (tam & 1), SEEK_CUR);
    }
    if (bits != 16 || canales < 1 || canales > 2) {
        fprintf(stderr, "solo WAV de 16 bits mono o estereo (este: %u bits, %u canales)\n", bits, canales);
        return 1;
    }

    int fd = open(disp, O_RDWR);
    if (fd < 0) { perror(disp); return 1; }

    const unsigned periodo = 1024, nperiodos = 4;
    struct snd_pcm_hw_params hw;
    memset(&hw, 0, sizeof hw);
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_MASK; n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
        memset(mascara(&hw, n)->bits, 0xff, sizeof mascara(&hw, n)->bits);
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
        intervalo(&hw, n)->min = 0;
        intervalo(&hw, n)->max = UINT_MAX;
    }
    hw.rmask = ~0u;
    hw.cmask = 0;
    hw.info = ~0u;
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S32_LE);
    pon_mascara(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 32);   /* el driver solo admite S32_LE */
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_FRAME_BITS, 32 * canales);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, canales);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_RATE, tasa);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, periodo);
    pon_valor(&hw, SNDRV_PCM_HW_PARAM_PERIODS, nperiodos);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0) {
        fprintf(stderr, "HW_PARAMS (%u Hz, %u canales): %s\n", tasa, canales, strerror(errno));
        return 1;
    }

    unsigned long buffer = periodo * nperiodos;
    struct snd_pcm_sw_params sw;
    memset(&sw, 0, sizeof sw);
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = periodo;
    sw.start_threshold = periodo;
    sw.stop_threshold = buffer;
    sw.boundary = buffer;
    while (sw.boundary * 2 <= (unsigned long)INT_MAX - buffer) sw.boundary *= 2;
    if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0) { perror("SW_PARAMS"); return 1; }
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) { perror("PREPARE"); return 1; }

    unsigned marco = 2 * canales;
    static unsigned char trozo[1024 * 4];
    uint32_t quedan = datos;
    while (quedan > 0) {
        size_t pide = quedan < sizeof trozo ? quedan : sizeof trozo;
        size_t leido = fread(trozo, 1, pide, f);
        if (leido < marco) break;
        quedan -= leido;
        /* 16 -> 32 bits: la muestra va en los 16 bits altos */
        static int32_t ancho[1024 * 2];
        unsigned muestras = leido / 2;
        for (unsigned i = 0; i < muestras; i++)
            ancho[i] = (int32_t)(int16_t)(trozo[2 * i] | trozo[2 * i + 1] << 8) << 16;
        unsigned char *p = (unsigned char *)ancho;
        snd_pcm_uframes_t marcos = leido / marco;
        while (marcos > 0) {
            struct snd_xferi x = { .buf = p, .frames = marcos };
            if (ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0) {
                if (errno == EPIPE) { ioctl(fd, SNDRV_PCM_IOCTL_PREPARE); continue; }   /* se vacio */
                perror("WRITEI");
                return 1;
            }
            p += x.result * marco * 2;
            marcos -= x.result;
        }
    }
    ioctl(fd, SNDRV_PCM_IOCTL_DRAIN);
    close(fd);
    return 0;
}
