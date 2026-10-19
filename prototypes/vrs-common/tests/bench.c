/* CPU interceptor only: mocked driver, not GPU time or game FPS. */
#define _POSIX_C_SOURCE 200809L
#ifndef LAYER_SOURCE
#define LAYER_SOURCE "../../nvidia-vrs/layer.c"
#endif
#include LAYER_SOURCE
#include <assert.h>
#include <time.h>
#include <unistd.h>
static volatile unsigned calls;
static void bind_noop(VkCommandBuffer c, VkPipelineBindPoint b, VkPipeline p) {
    (void)c; (void)b; (void)p; calls++;
}
static void rate_noop(VkCommandBuffer c, const VkExtent2D *r, const VkFragmentShadingRateCombinerOpKHR *o) {
    (void)c; (void)r; (void)o; calls++;
}
static double now(void) {
    struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC, &t));
    return t.tv_sec * 1e9 + t.tv_nsec;
}
int main(int argc, char **argv) {
    unsigned n = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 10000;
    unsigned iterations = 20000;
    void *key = &key;
    VkCommandBuffer cmd = (VkCommandBuffer)&key;
    Device d = {.handle=(VkDevice)&key, .active=1, .bind_pipeline=bind_noop, .set_rate=rate_noop};
    devices=&d;
#ifdef UGM_VRS_SHARED_CORE
    d.key=key; assert(!pthread_mutex_init(&d.objects_lock, NULL));
    assert(!pthread_mutex_init(&d.rate.lock, NULL));
    assert(table_init(&d.pipelines));
#else
    Command c = {.handle=cmd, .device=&d}; commands=&c;
#endif
    for(unsigned i=1;i<=n;++i) {
        Pipeline *p=calloc(1,sizeof(*p)); assert(p);
#ifdef UGM_VRS_SHARED_CORE
        p->injected=1; p->node.key=i; table_insert(&d.pipelines,&p->node);
#else
        p->handle=(VkPipeline)(uintptr_t)i; p->device=&d; p->next=pipelines; pipelines=p;
#endif
    }
    char path[]="/tmp/ugm-vrs-bench-XXXXXX"; int fd=mkstemp(path); assert(fd>=0);
    assert(write(fd,"2x2\n",4)==4); close(fd);
    for(unsigned file=0;file<2;++file) {
        if(file) assert(!setenv("UGM_VRS_CONFIG_FILE",path,1));
        else { unsetenv("UGM_VRS_CONFIG_FILE"); assert(!setenv("UGM_VRS_RATE","2x2",1)); }
        for(unsigned warm=0;warm<1000;++warm) vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,(VkPipeline)(uintptr_t)1);
        double start=now();
        for(unsigned i=0;i<iterations;++i) vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,(VkPipeline)(uintptr_t)1);
        printf("pipelines=%u file=%u ns/bind=%.1f iterations=%u\n",n,file,(now()-start)/iterations,iterations);
    }
    unlink(path);
#ifdef UGM_VRS_SHARED_CORE
    table_clear(&d.pipelines); rate_destroy(&d.rate); pthread_mutex_destroy(&d.objects_lock);
#else
    while(pipelines) { Pipeline *p=pipelines; pipelines=p->next; free(p); }
#endif
    return 0;
}
