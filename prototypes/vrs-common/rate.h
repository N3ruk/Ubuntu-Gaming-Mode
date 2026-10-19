/* QAM is an ordinary small local file, not a stream. Check metadata at every
 * eligible bind; read only on change. No polling interval, background thread,
 * inotify limits/overflow/rearming, or stale positive on deletion/chmod/rename.
 * A writer racing a read may temporarily select 1x1, never a torn 2x2 token. */
#ifndef UGM_VRS_RATE_H
#define UGM_VRS_RATE_H
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <linux/stat.h>
#include <sys/syscall.h>
/* Use the Linux statx syscall ABI (kernel >= 4.11), not the newer glibc
 * stat/time64 symbols. Steam containers share the host kernel. */
extern long syscall(long number, ...);
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x1000
#endif
typedef struct RateCache {
    pthread_mutex_t lock;
    struct statx metadata;
    char path[PATH_MAX];
    int valid;
    unsigned value;
    /* Updated under lock; read at device destruction after external sync. */
    uint64_t reads, changes, failures, checks;
} RateCache;
static VkExtent2D parse_rate(const char *value) {
    unsigned rate=value && (!strcmp(value,"2x2") || !strcmp(value,"2x2\n")) ? 2 : 1;
    return (VkExtent2D){rate,rate};
}
static int rate_stat(int fd,const char *path,int flags,struct statx *out) {
    memset(out,0,sizeof(*out));
    if(syscall(SYS_statx,fd,path,flags,STATX_BASIC_STATS,out)) return -1;
    const unsigned required=STATX_TYPE|STATX_MODE|STATX_INO|STATX_SIZE|STATX_UID|STATX_GID|STATX_MTIME|STATX_CTIME;
    return (out->stx_mask&required)==required ? 0 : -1;
}
static int rate_same(const struct statx *a,const struct statx *b) {
    return a->stx_dev_major==b->stx_dev_major && a->stx_dev_minor==b->stx_dev_minor &&
           a->stx_ino==b->stx_ino && a->stx_size==b->stx_size &&
           a->stx_mode==b->stx_mode && a->stx_uid==b->stx_uid && a->stx_gid==b->stx_gid &&
           a->stx_mtime.tv_sec==b->stx_mtime.tv_sec && a->stx_mtime.tv_nsec==b->stx_mtime.tv_nsec &&
           a->stx_ctime.tv_sec==b->stx_ctime.tv_sec && a->stx_ctime.tv_nsec==b->stx_ctime.tv_nsec;
}
static VkExtent2D requested_rate(RateCache *c) {
    const char *path=getenv("UGM_VRS_CONFIG_FILE");
    if(!path || !*path) return parse_rate(getenv("UGM_VRS_RATE"));
    struct statx before,opened,after,final;
    /* Metadata queries do not serialize independent recording threads. The
     * lock only protects the shared snapshot; recheck before a changed read. */
    int valid=strlen(path)<sizeof(c->path) && !rate_stat(AT_FDCWD,path,0,&before) &&
              S_ISREG(before.stx_mode) && (before.stx_size==3 || before.stx_size==4);
    pthread_mutex_lock(&c->lock);
    unsigned rate=1;
    ++c->checks;
    if(!valid) {
        c->valid=0; ++c->failures;
    } else if(c->valid && !strcmp(path,c->path) && rate_same(&before,&c->metadata)) {
        rate=c->value;
    } else {
        c->valid=0;
        int fd=-1;
        if(!rate_stat(AT_FDCWD,path,0,&before) && S_ISREG(before.stx_mode) &&
           (before.stx_size==3 || before.stx_size==4))
            fd=open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK);
        if(fd>=0) {
            char value[5]={0};
            ++c->reads;
            ssize_t n=-1;
            if(!rate_stat(fd,"",AT_EMPTY_PATH,&opened) && S_ISREG(opened.stx_mode) && rate_same(&before,&opened)) {
                do { n=read(fd,value,4); } while(n<0 && errno==EINTR);
            }
            if(n>=0 && (uint64_t)n==before.stx_size && !memchr(value,0,(size_t)n) &&
               !rate_stat(fd,"",AT_EMPTY_PATH,&after) && rate_same(&before,&after) &&
               !rate_stat(AT_FDCWD,path,0,&final) && rate_same(&after,&final)) {
                rate=parse_rate(value).width;
                strcpy(c->path,path); c->metadata=final; c->valid=1;
            }
            close(fd);
        }
        if(!c->valid) ++c->failures;
    }
    if(c->value && rate!=c->value) ++c->changes;
    c->value=rate;
    pthread_mutex_unlock(&c->lock);
    return (VkExtent2D){rate,rate};
}
static void rate_destroy(RateCache *c) { pthread_mutex_destroy(&c->lock); }
#endif
