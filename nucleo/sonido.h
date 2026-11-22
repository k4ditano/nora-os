/* sonido: el mezclador de Nora. El altavoz de la RG35XX solo admite un sonido a
 * la vez, asi que un hilo lo abre una vez y mezcla: efectos (cargados en
 * memoria, hasta 8 a la vez) y una musica de fondo (leida del fichero poco a
 * poco, en bucle y con fundidos). WAV PCM de 16 bits, mono o estereo, a
 * cualquier frecuencia: se convierte al vuelo a 44,1 kHz estereo. */
#ifndef NORA_SONIDO_H
#define NORA_SONIDO_H

int sonido_iniciar(void);                           /* 0 = bien */
void sonido_efecto(const char *wav, float volumen);
void sonido_musica(const char *wav, float volumen); /* wav NULL o "" = parar, con fundido */
void sonido_cerrar(void);

#endif
