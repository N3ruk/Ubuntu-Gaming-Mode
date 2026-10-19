/* Explicit local diagnostic capture. Disabled unless a directory is supplied.
 * Bounded per process; exclusive files, no symlink following, no game identity.
 * This is diagnostic IO at shader creation, never on the bind/draw path. */
#ifndef UGM_VRS_SHADER_CAPTURE_H
#define UGM_VRS_SHADER_CAPTURE_H
#include <fcntl.h>
#include <unistd.h>
static void capture_shader(const VkShaderModuleCreateInfo *info) {
    const char *directory=getenv("UGM_VRS_CAPTURE_DIR");
    if(!directory || !*directory || !info || !info->pCode || info->codeSize<20 ||
       info->codeSize>65536 || info->codeSize%4) return;
    static atomic_uint captures;
    unsigned index=atomic_fetch_add_explicit(&captures,1,memory_order_relaxed);
    if(index>=2048) return;
    int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(dir<0) return;
    char name[80]; snprintf(name,sizeof(name),"shader-%ld-%u.spv",(long)getpid(),index);
    int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    close(dir);
    if(fd<0) return;
    const unsigned char *data=(const void *)info->pCode; size_t remaining=info->codeSize;
    while(remaining) {
        ssize_t done=write(fd,data,remaining);
        if(done<0 && errno==EINTR) continue;
        if(done<=0) break;
        data+=done; remaining-=(size_t)done;
    }
    if(remaining) fprintf(stderr,"UGM VRS capture: incomplete shader pid=%ld index=%u remaining=%zu\n",(long)getpid(),index,remaining);
    close(fd);
}
#endif
