/* Bounded SPIR-V analysis timing; no driver/shader compilation measured. */
#define _POSIX_C_SOURCE 200809L
#ifndef LAYER_SOURCE
#define LAYER_SOURCE "../../nvidia-vrs/layer.c"
#endif
#include LAYER_SOURCE
#include <assert.h>
#include <time.h>
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec; }
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    uint32_t *data=malloc((size_t)size);assert(data);assert(fread(data,1,(size_t)size,f)==(size_t)size);fclose(f);
    VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=(size_t)size,.pCode=data};
    volatile unsigned flags=0;unsigned count=20000;double start=now();
    for(unsigned i=0;i<count;++i) flags=shader_flags(&ci);
    printf("%s flags=%u ns/analysis=%.1f bytes=%ld iterations=%u\n",argv[1],flags,(now()-start)/count,size,count);
    free(data);return 0;
}
