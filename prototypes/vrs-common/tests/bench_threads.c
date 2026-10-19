#define _POSIX_C_SOURCE 200809L
#ifndef LAYER_SOURCE
#define LAYER_SOURCE "../../nvidia-vrs/layer.c"
#endif
#include LAYER_SOURCE
#include <assert.h>
#include <time.h>
#include <unistd.h>
static unsigned iterations=20000;
static void bind_noop(VkCommandBuffer c,VkPipelineBindPoint b,VkPipeline p) { (void)c;(void)b;(void)p;__asm__ volatile("" ::: "memory"); }
static void rate_noop(VkCommandBuffer c,const VkExtent2D *r,const VkFragmentShadingRateCombinerOpKHR *o) { (void)c;(void)r;(void)o;__asm__ volatile("" ::: "memory"); }
static void *worker(void *cmd) { for(unsigned i=0;i<iterations;++i) vkCmdBindPipeline((VkCommandBuffer)cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,(VkPipeline)(uintptr_t)1);return NULL; }
int main(int argc,char **argv) {
    assert(argc==3);unsigned n=(unsigned)strtoul(argv[1],NULL,10),threads=(unsigned)strtoul(argv[2],NULL,10);assert(threads>0 && threads<=8);
    void *key=&key,*keys[8]; pthread_t workers[8];
    Device d={.handle=(VkDevice)&key,.active=1,.bind_pipeline=bind_noop,.set_rate=rate_noop};devices=&d;
#ifdef UGM_VRS_SHARED_CORE
    d.key=key;pthread_mutex_init(&d.objects_lock,NULL);pthread_mutex_init(&d.rate.lock,NULL);assert(table_init(&d.pipelines));
#else
    Command cmd[8];
#endif
    for(unsigned j=0;j<threads;++j) { keys[j]=key;
#ifndef UGM_VRS_SHARED_CORE
        cmd[j]=(Command){.handle=(VkCommandBuffer)&keys[j],.device=&d,.next=commands};commands=&cmd[j];
#endif
    }
    for(unsigned j=1;j<=n;++j) { Pipeline *p=calloc(1,sizeof(*p));assert(p);
#ifdef UGM_VRS_SHARED_CORE
        p->injected=1; p->node.key=j;table_insert(&d.pipelines,&p->node);
#else
        p->handle=(VkPipeline)(uintptr_t)j;p->device=&d;p->next=pipelines;pipelines=p;
#endif
    }
    char path[]="/tmp/ugm-vrs-threads-XXXXXX";int fd=mkstemp(path);assert(fd>=0);assert(write(fd,"2x2\n",4)==4);close(fd);setenv("UGM_VRS_CONFIG_FILE",path,1);
    struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);
    for(unsigned j=0;j<threads;++j) assert(!pthread_create(&workers[j],NULL,worker,&keys[j]));
    for(unsigned j=0;j<threads;++j) assert(!pthread_join(workers[j],NULL));
    clock_gettime(CLOCK_MONOTONIC,&b);
    printf("threads=%u pipelines=%u file=1 aggregate-ns/bind=%.1f binds=%u\n",threads,n,((b.tv_sec-a.tv_sec)*1e9+b.tv_nsec-a.tv_nsec)/(threads*iterations),threads*iterations);
    unlink(path);
#ifdef UGM_VRS_SHARED_CORE
    table_clear(&d.pipelines);rate_destroy(&d.rate);pthread_mutex_destroy(&d.objects_lock);
#else
    while(pipelines) { Pipeline *p=pipelines;pipelines=p->next;free(p); }
#endif
    return 0;
}
