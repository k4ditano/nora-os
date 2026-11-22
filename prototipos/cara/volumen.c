/* volumen: lista los controles del mezclador ALSA o cambia uno.
 *   volumen                 lista todos con su valor
 *   volumen NUMID VALOR     pone ese valor (en todos sus canales)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sound/asound.h>
int main(int argc, char **argv){
  int fd=open("/dev/snd/controlC0",O_RDWR); if(fd<0){perror("controlC0");return 1;}
  if(argc==3){
    struct snd_ctl_elem_info in; memset(&in,0,sizeof in); in.id.numid=atoi(argv[1]);
    if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_INFO,&in)<0){perror("INFO");return 1;}
    struct snd_ctl_elem_value v; memset(&v,0,sizeof v); v.id.numid=in.id.numid;
    for(unsigned i=0;i<in.count;i++) v.value.integer.value[i]=atol(argv[2]);
    if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_WRITE,&v)<0){perror("WRITE");return 1;}
    printf("%s = %s\n",in.id.name,argv[2]); return 0;}
  struct snd_ctl_elem_list l; memset(&l,0,sizeof l);
  ioctl(fd,SNDRV_CTL_IOCTL_ELEM_LIST,&l);
  struct snd_ctl_elem_id ids[128]; l.space=l.count<128?l.count:128; l.pids=ids;
  if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_LIST,&l)<0){perror("LIST");return 1;}
  for(unsigned k=0;k<l.used;k++){
    struct snd_ctl_elem_info in; memset(&in,0,sizeof in); in.id=ids[k];
    if(ioctl(fd,SNDRV_CTL_IOCTL_ELEM_INFO,&in)<0) continue;
    struct snd_ctl_elem_value v; memset(&v,0,sizeof v); v.id=ids[k];
    ioctl(fd,SNDRV_CTL_IOCTL_ELEM_READ,&v);
    printf("%3u %-36s ",in.id.numid,in.id.name);
    if(in.type==SNDRV_CTL_ELEM_TYPE_INTEGER) printf("int %ld..%ld =",in.value.integer.min,in.value.integer.max);
    else if(in.type==SNDRV_CTL_ELEM_TYPE_BOOLEAN) printf("bool =");
    else if(in.type==SNDRV_CTL_ELEM_TYPE_ENUMERATED) printf("enum(%u) =",in.value.enumerated.items);
    else printf("tipo%d =",in.type);
    for(unsigned i=0;i<in.count&&i<4;i++) printf(" %ld",in.type==SNDRV_CTL_ELEM_TYPE_ENUMERATED?(long)v.value.enumerated.item[i]:v.value.integer.value[i]);
    printf("\n");}
  return 0;}
