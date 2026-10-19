/* CPU wrapper microbenchmark with a mock driver; not pipeline compile time,
 * game startup time, GPU performance or FPS. Build before/after with the same
 * harness and override LAYER_SOURCE for the preserved baseline. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <assert.h>
static unsigned long allocations;
static void *bench_calloc(size_t n,size_t size) {
    ++allocations;
    return calloc(n,size);
}
#define calloc bench_calloc
#define UGM_VRS_VENDOR_ID 0x10de
#define UGM_VRS_DRIVER_ID VK_DRIVER_ID_NVIDIA_PROPRIETARY
#define UGM_VRS_VENDOR "nvidia"
#ifndef LAYER_SOURCE
#define LAYER_SOURCE "../layer.c"
#endif
#include LAYER_SOURCE
#undef calloc
#include <time.h>
static uintptr_t serial=100;
static VkResult create_mock(VkDevice device,VkPipelineCache cache,uint32_t count,
        const VkGraphicsPipelineCreateInfo *infos,const VkAllocationCallbacks *a,VkPipeline *out) {
    (void)device;(void)cache;(void)infos;(void)a;
    for(uint32_t i=0;i<count;++i) out[i]=(VkPipeline)++serial;
    return VK_SUCCESS;
}
static void destroy_mock(VkDevice d,VkPipeline p,const VkAllocationCallbacks *a) {(void)d;(void)p;(void)a;}
static double now(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));
    return t.tv_sec*1e9+t.tv_nsec;
}
int main(int argc,char **argv) {
    unsigned iterations=argc>1 ? (unsigned)strtoul(argv[1],NULL,10) : 200000;
    assert(iterations);
    void *key=&key;
    Device d={.handle=(VkDevice)&key,.key=key,.active=1,
        .create_pipelines=create_mock,.destroy_pipeline=destroy_mock};
    assert(!pthread_mutex_init(&d.objects_lock,NULL));
    assert(table_init(&d.pipelines) && table_init(&d.shaders));devices=&d;
    for(uint64_t i=1;i<=2;++i) {
        Shader *s=calloc(1,sizeof(*s));assert(s);s->node.key=i;
        s->flags=SHADER_VALID|(i==1 ? SHADER_POSITION_COARSE : 0);table_insert(&d.shaders,&s->node);
    }
    VkPipelineShaderStageCreateInfo stages[]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=(VkShaderModule)1,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=(VkShaderModule)2,.pName="main"}};
    VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL};
    VkGraphicsPipelineCreateInfo mono={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount=2,.pStages=stages,.pMultisampleState=&ms,.pRasterizationState=&rs};
    VkGraphicsPipelineLibraryCreateInfoEXT part={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};
    const char *names[]={"gpl-vi-1","gpl-vi-8","gpl-pre-1","monolithic-1","mixed-2"};
    for(unsigned mode=0;mode<5;++mode) {
        uint32_t count=mode==1 ? 8 : mode==4 ? 2 : 1;
        part.flags=mode==2 ? GPL_PRE : GPL_VI;
        VkGraphicsPipelineCreateInfo infos[8];VkPipeline out[8];
        for(unsigned i=0;i<count;++i) {
            infos[i]=mono;infos[i].pNext=&part;infos[i].flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
            infos[i].stageCount=mode==2 ? 1 : 0;
        }
        if(mode==3 || mode==4) infos[count-1]=mono;
        for(unsigned warm=0;warm<2000;++warm) {
            assert(vkCreateGraphicsPipelines(d.handle,0,count,infos,NULL,out)==VK_SUCCESS);
            for(unsigned i=0;i<count;++i) vkDestroyPipeline(d.handle,out[i],NULL);
        }
        allocations=0;double start=now();
        for(unsigned j=0;j<iterations;++j) {
            assert(vkCreateGraphicsPipelines(d.handle,0,count,infos,NULL,out)==VK_SUCCESS);
            for(unsigned i=0;i<count;++i) vkDestroyPipeline(d.handle,out[i],NULL);
        }
        double ns=(now()-start)/iterations;
        assert(!d.pipelines.count);
        printf("{\"case\":\"%s\",\"iterations\":%u,\"ns_per_batch\":%.3f,\"allocations_per_batch\":%.1f}\n",
            names[mode],iterations,ns,(double)allocations/iterations);
    }
    table_clear(&d.pipelines);table_clear(&d.shaders);pthread_mutex_destroy(&d.objects_lock);
    return 0;
}
