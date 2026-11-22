#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sound/asound.h>
int main(int c, char **v){
  const char *d = c>1?v[1]:"/dev/snd/pcmC0D0p";
  int fd=open(d,O_RDWR); if(fd<0){perror(d);return 1;}
  struct snd_pcm_hw_params p; memset(&p,0,sizeof p);
  for(int n=0;n<=SNDRV_PCM_HW_PARAM_LAST_MASK;n++) memset(p.masks[n].bits,0xff,sizeof p.masks[n].bits);
  for(int n=0;n<=SNDRV_PCM_HW_PARAM_LAST_INTERVAL-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;n++){p.intervals[n].min=0;p.intervals[n].max=UINT_MAX;}
  p.rmask=~0u;
  if(ioctl(fd,SNDRV_PCM_IOCTL_HW_REFINE,&p)<0){perror("REFINE");return 1;}
  printf("access=%08x format=%08x %08x\n",p.masks[0].bits[0],p.masks[1].bits[0],p.masks[1].bits[1]);
  const char *nom[]={"sample_bits","frame_bits","channels","rate","period_time","period_size","period_bytes","periods","buffer_time","buffer_size","buffer_bytes","tick_time"};
  for(int n=0;n<12;n++) printf("%s=%u..%u\n",nom[n],p.intervals[n].min,p.intervals[n].max);
  return 0;}
