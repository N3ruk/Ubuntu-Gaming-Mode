#define _POSIX_C_SOURCE 200809L
#include <vulkan/vulkan.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdatomic.h>
#include "../rate.h"
static RateCache cache={.lock=PTHREAD_MUTEX_INITIALIZER};
static atomic_int stop;
static void write_token(const char *path,const char *value) {
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600); assert(fd>=0);
    size_t n=strlen(value); assert(write(fd,value,n)==(ssize_t)n); assert(!close(fd));
}
static void *reader(void *unused) {
    (void)unused;
    while(!atomic_load(&stop)) { VkExtent2D r=requested_rate(&cache); assert((r.width==1 || r.width==2) && r.width==r.height); }
    return NULL;
}
int main(void) {
    char dir[]="/tmp/ugm-rate-XXXXXX"; assert(mkdtemp(dir));
    char path[256],other[256]; snprintf(path,sizeof(path),"%s/rate",dir); snprintf(other,sizeof(other),"%s/new",dir);
    assert(!setenv("UGM_VRS_CONFIG_FILE",path,1));
    assert(requested_rate(&cache).width==1); /* not created yet */
    write_token(path,"2x2\n"); assert(requested_rate(&cache).width==2);
    uint64_t reads=cache.reads;
    for(int i=0;i<10000;++i) assert(requested_rate(&cache).width==2);
    assert(cache.reads==reads);
    const char *bad[]={"", "2x1", "2x2\nx", "2x2garbage", "garbage", "2x2\r\n"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i) { write_token(path,bad[i]); assert(requested_rate(&cache).width==1); }
    for(int i=0;i<100;++i) {
        write_token(other,"2x2\n"); assert(!rename(other,path)); assert(requested_rate(&cache).width==2);
        write_token(path,"1x1\n"); assert(requested_rate(&cache).width==1);
    }
    write_token(path,"2x2\n"); assert(requested_rate(&cache).width==2);
    assert(!chmod(path,0000)); if(geteuid()!=0) assert(requested_rate(&cache).width==1);
    assert(!chmod(path,0600)); assert(requested_rate(&cache).width==2);
    assert(!unlink(path)); assert(requested_rate(&cache).width==1);
    assert(!mkfifo(path,0600)); assert(requested_rate(&cache).width==1); assert(!unlink(path));
    write_token(other,"2x2"); assert(!symlink(other,path)); assert(requested_rate(&cache).width==2);
    assert(!unlink(other)); assert(requested_rate(&cache).width==1); assert(!unlink(path));
    pthread_t threads[4]; for(int i=0;i<4;++i) assert(!pthread_create(&threads[i],NULL,reader,NULL));
    for(int i=0;i<1000;++i) { write_token(other,i&1 ? "1x1\n" : "2x2\n"); assert(!rename(other,path)); }
    atomic_store(&stop,1); for(int i=0;i<4;++i) assert(!pthread_join(threads[i],NULL));
    write_token(path,"1x1\n"); assert(requested_rate(&cache).width==1);
    write_token(path,"2x2\n"); assert(requested_rate(&cache).width==2);
    assert(cache.changes>=200);
    printf("QAM cache: unchanged reads=0/10000; rename/write/delete/chmod/FIFO/symlink and 4-reader stress OK; reads=%llu changes=%llu\n",(unsigned long long)cache.reads,(unsigned long long)cache.changes);
    rate_destroy(&cache); unlink(path); rmdir(dir); return 0;
}
