/* Offline diagnostic for locally captured SPIR-V; no game-specific policy. */
#define _POSIX_C_SOURCE 200809L
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../spirv_policy.h"
static void report(void *context,const char *reason,uint32_t value) {
    (void)context; printf(" %s=%u",reason,value);
}
int main(int argc,char **argv) {
    for(int i=1;i<argc;++i) {
        FILE *f=fopen(argv[i],"rb");
        if(!f || fseek(f,0,SEEK_END)) return 2;
        long size=ftell(f); if(size<20 || size>16*1024*1024 || size%4) return 2;
        rewind(f); uint32_t *code=malloc((size_t)size); if(!code) return 2;
        if(fread(code,1,(size_t)size,f)!=(size_t)size) return 2;
        fclose(f);
        VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=(size_t)size,.pCode=code};
        unsigned flags=shader_flags(&ci);
        printf("%s flags=%u",argv[i],flags);
        (void)shader_flags_report(&ci,report,NULL); puts(""); free(code);
    }
    return 0;
}
