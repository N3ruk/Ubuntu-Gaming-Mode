/* Driver simulation: dispatch, handle lifetime, extended state and allocation
 * failures. Actual GPU behavior is covered separately by the offscreen suite. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <assert.h>
static _Thread_local int fail_allocation=-1;
static void *test_calloc(size_t count,size_t size) {
    if(fail_allocation==0) return NULL;
    if(fail_allocation>0) --fail_allocation;
    return calloc(count,size);
}
#define calloc test_calloc
#ifdef UGM_TEST_AMD
#include "../../amd-vrs/layer.c"
#else
#include "../../nvidia-vrs/layer.c"
#endif
#undef calloc
#include <stdio.h>
static atomic_ulong serial=100;
static _Thread_local int backend_binds,backend_rate,dynamic_added;
static unsigned destroyed_devices,destroyed_instances,created_devices;
static void *instance_keys[2],*physical_keys[2],*device_keys[8];
static VkInstance expected_instance;
static int advertise, expect_unmodified;
static VkResult pipeline_result=VK_SUCCESS;
static int fail_gpl_shadow;
/* Inspect the original driver batch once; private GPL calls may follow. */
static const VkGraphicsPipelineCreateInfo *expect_batch;
static uint32_t expect_batch_count;
static VkPipelineCache expect_batch_cache;
static const VkAllocationCallbacks *expect_batch_allocator;
static VkPipeline *expect_batch_out;
static const VkGraphicsPipelineCreateInfo *expect_preserved;
static void fake_bind(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) { (void)c;(void)p;(void)pipeline; ++backend_binds; }
static void fake_rate(VkCommandBuffer c,const VkExtent2D *rate,const VkFragmentShadingRateCombinerOpKHR *ops) {
    (void)c; assert(ops[0]==VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR && ops[1]==VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR); backend_rate=(int)rate->width;
}
static VkResult fake_shader(VkDevice d,const VkShaderModuleCreateInfo *i,const VkAllocationCallbacks *a,VkShaderModule *out) {
    (void)d;(void)i;(void)a; *out=(VkShaderModule)(uintptr_t)atomic_fetch_add(&serial,1);return VK_SUCCESS;
}
static void destroy_shader(VkDevice d,VkShaderModule s,const VkAllocationCallbacks *a) { (void)d;(void)s;(void)a; }
static void destroy_pipeline(VkDevice d,VkPipeline p,const VkAllocationCallbacks *a) { (void)d;(void)p;(void)a; }
static VkResult fake_pipelines(VkDevice d,VkPipelineCache c,uint32_t n,const VkGraphicsPipelineCreateInfo *infos,const VkAllocationCallbacks *a,VkPipeline *out) {
    (void)d;(void)c;(void)a; dynamic_added=0;
    if(expect_batch) {
        assert(n==expect_batch_count && c==expect_batch_cache);
        assert(a==expect_batch_allocator && out==expect_batch_out);
        assert(!memcmp(infos,expect_batch,n*sizeof(*infos)));
        expect_batch=NULL;
    }
    for(uint32_t i=0;i<n;++i) {
        if(expect_preserved) {
            VkGraphicsPipelineCreateInfo original=*expect_preserved, received=infos[i];
            received.pDynamicState=original.pDynamicState;
            assert(!memcmp(&received,&original,sizeof(original)));
            if(original.pDynamicState && infos[i].pDynamicState!=original.pDynamicState) {
                assert(infos[i].pDynamicState->pNext==original.pDynamicState->pNext);
                assert(infos[i].pDynamicState->dynamicStateCount==original.pDynamicState->dynamicStateCount+1);
                assert(!memcmp(infos[i].pDynamicState->pDynamicStates,original.pDynamicState->pDynamicStates,
                    original.pDynamicState->dynamicStateCount*sizeof(VkDynamicState)));
            }
        }
        const VkPipelineDynamicStateCreateInfo *s=infos[i].pDynamicState;
        for(uint32_t j=0;s && j<s->dynamicStateCount;++j) if(s->pDynamicStates[j]==VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR) ++dynamic_added;
        out[i]=pipeline_result!=VK_SUCCESS && i==1 ? VK_NULL_HANDLE : (VkPipeline)(uintptr_t)atomic_fetch_add(&serial,1);
    }
    if(fail_gpl_shadow && n==1 && pipeline_is_gpl(infos) && dynamic_added) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return pipeline_result;
}
static void fake_destroy_device(VkDevice d,const VkAllocationCallbacks *a) { (void)d;(void)a; ++destroyed_devices; }
static void fake_destroy_instance(VkInstance i,const VkAllocationCallbacks *a) { (void)i;(void)a; ++destroyed_instances; }
static void opaque_command(void) {}
static PFN_vkVoidFunction fake_gdpa(VkDevice d,const char *name) {
    (void)d;
#define PROC(n,f) if(!strcmp(name,n)) return (PFN_vkVoidFunction)f
    PROC("vkCmdBindPipeline",fake_bind); PROC("vkCmdSetFragmentShadingRateKHR",fake_rate);
    PROC("vkCreateGraphicsPipelines",fake_pipelines); PROC("vkCreateShaderModule",fake_shader);
    PROC("vkDestroyShaderModule",destroy_shader); PROC("vkDestroyPipeline",destroy_pipeline);
    PROC("vkDestroyDevice",fake_destroy_device);
    return opaque_command;
}
static VkResult create_instance_a(const VkInstanceCreateInfo *i,const VkAllocationCallbacks *a,VkInstance *out) { (void)i;(void)a; *out=(VkInstance)&instance_keys[0]; return VK_SUCCESS; }
static VkResult create_instance_b(const VkInstanceCreateInfo *i,const VkAllocationCallbacks *a,VkInstance *out) { (void)i;(void)a; *out=(VkInstance)&instance_keys[1]; return VK_SUCCESS; }
static VkResult fake_create_device(VkPhysicalDevice p,const VkDeviceCreateInfo *ci,const VkAllocationCallbacks *a,VkDevice *out) {
    (void)p;(void)a;
    if(advertise) {
        int found=0,renderpass2=0;
        for(uint32_t j=0;j<ci->enabledExtensionCount;++j) {
            found|=!strcmp(ci->ppEnabledExtensionNames[j],VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
            renderpass2|=!strcmp(ci->ppEnabledExtensionNames[j],VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        }
        assert(found==!expect_unmodified && renderpass2==!expect_unmodified);
    }
    assert(created_devices<8); device_keys[created_devices]=&device_keys[created_devices];
    *out=(VkDevice)&device_keys[created_devices++]; return VK_SUCCESS;
}
static void properties(VkPhysicalDevice p,VkPhysicalDeviceProperties2 *out) {
    (void)p;out->properties.vendorID=advertise ? UGM_VRS_VENDOR_ID : 0;
    out->properties.deviceID=0x1234;out->properties.apiVersion=VK_API_VERSION_1_3;
    for(VkBaseOutStructure *q=out->pNext;q;q=q->pNext)
        if(q->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES)
            ((VkPhysicalDeviceDriverProperties *)q)->driverID=UGM_VRS_DRIVER_ID;
}
static void features(VkPhysicalDevice p,VkPhysicalDeviceFeatures2 *out) { (void)p; ((VkPhysicalDeviceFragmentShadingRateFeaturesKHR *)out->pNext)->pipelineFragmentShadingRate=VK_TRUE; }
static VkResult extensions(VkPhysicalDevice p,const char *name,uint32_t *count,VkExtensionProperties *out) {
    (void)p;(void)name;
    if(out) { strcpy(out[0].extensionName,VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME); strcpy(out[1].extensionName,VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME); }
    *count=2;return VK_SUCCESS;
}
static VkResult rates(VkPhysicalDevice p,uint32_t *count,VkPhysicalDeviceFragmentShadingRateKHR *out) {
    (void)p;if(out) { out->fragmentSize=(VkExtent2D){2,2};out->sampleCounts=VK_SAMPLE_COUNT_1_BIT; }
    *count=1;return VK_SUCCESS;
}
static PFN_vkVoidFunction common_gipa(VkInstance inst,const char *name) {
    if(inst) assert(inst==expected_instance);
    PROC("vkCreateDevice",fake_create_device); PROC("vkDestroyInstance",fake_destroy_instance);
    PROC("vkGetPhysicalDeviceProperties2",properties); PROC("vkGetPhysicalDeviceFeatures2",features);
    PROC("vkEnumerateDeviceExtensionProperties",extensions); PROC("vkGetPhysicalDeviceFragmentShadingRatesKHR",rates);
    return opaque_command;
}
static PFN_vkVoidFunction gipa_a(VkInstance inst,const char *name) {
    if(!strcmp(name,"vkCreateInstance")) return (PFN_vkVoidFunction)create_instance_a;
    assert(!inst || inst==(VkInstance)&instance_keys[0]); return common_gipa(inst,name);
}
static PFN_vkVoidFunction gipa_b(VkInstance inst,const char *name) {
    if(!strcmp(name,"vkCreateInstance")) return (PFN_vkVoidFunction)create_instance_b;
    assert(!inst || inst==(VkInstance)&instance_keys[1]); return common_gipa(inst,name);
}
static VkDevice make_device(unsigned owner) {
    expected_instance=(VkInstance)&instance_keys[owner];
    VkLayerDeviceLink link={.pfnNextGetInstanceProcAddr=owner ? gipa_b : gipa_a,.pfnNextGetDeviceProcAddr=fake_gdpa};
    VkLayerDeviceCreateInfo chain={.sType=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO,.function=VK_LAYER_LINK_INFO,.u.pLayerInfo=&link};
    VkDeviceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.pNext=&chain};
    VkDevice out; assert(vkCreateDevice((VkPhysicalDevice)&physical_keys[owner],&ci,NULL,&out)==VK_SUCCESS);
    assert(ci.enabledExtensionCount==0 && ci.pNext==&chain);return out;
}
static VkGraphicsPipelineCreateInfo fixture(void) {
    static VkPipelineShaderStageCreateInfo stages[]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=(VkShaderModule)1,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=(VkShaderModule)2,.pName="main"}};
    static VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    static VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL};
    static VkPipelineColorBlendAttachmentState attachment;
    static VkPipelineColorBlendStateCreateInfo blend={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&attachment};
    return (VkGraphicsPipelineCreateInfo){.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,.pMultisampleState=&ms,.pRasterizationState=&rs,.pColorBlendState=&blend};
}
static void *worker(void *arg) {
    Device *d=arg; void *key=d->key; VkCommandBuffer cmd=(VkCommandBuffer)&key;
    VkGraphicsPipelineCreateInfo ci=fixture();
    for(int i=0;i<500;++i) {
        VkPipeline p; assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&p)==VK_SUCCESS);assert(dynamic_added==1);
        backend_rate=0;vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p);assert(backend_rate==2);
        vkDestroyPipeline(d->handle,p,NULL);
    }
    return NULL;
}
/* Cross-vendor compatibility and telemetry semantics, including unchanged
 * application-owned chains and audit-only behavior. */
static void compatibility_tests(Device *d) {
    VkGraphicsPipelineCreateInfo ci=fixture();
    VkPipelineRasterizationStateCreateInfo rs=*ci.pRasterizationState;
    VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provoking={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,
        .provokingVertexMode=VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT};
    VkPipelineRasterizationDepthClipStateCreateInfoEXT clip={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
        .pNext=&provoking,.depthClipEnable=VK_TRUE};
    rs.pNext=&clip; ci.pRasterizationState=&rs;
    VkPipelineRasterizationStateCreateInfo original_rs=rs;
    VkPipelineRasterizationDepthClipStateCreateInfoEXT original_clip=clip;
    VkDynamicState values[]={VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,VK_DYNAMIC_STATE_LOGIC_OP_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT};
    VkPipelineDynamicStateCreateInfo ds={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount=4,.pDynamicStates=values};
    ci.pDynamicState=&ds;
    assert(legacy_pipeline_policy(d,&ci)==EXTENDED_STATE);
    assert(pipeline_policy(d,&ci)==ACCEPT);
    d->debug=1; atomic_store(&d->next_report_sec,ULONG_MAX);
    VkPipeline p; expect_preserved=&ci;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&p)==VK_SUCCESS);
    assert(dynamic_added==1 && atomic_load(&d->accepted)==1 && atomic_load(&d->injected)==1);
    assert(!memcmp(&rs,&original_rs,sizeof(rs)) && !memcmp(&clip,&original_clip,sizeof(clip)));
    assert(d->detail_count>=6);
    void *key=d->key; VkCommandBuffer cmd=(VkCommandBuffer)&key;
    backend_rate=0; vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p); assert(backend_rate==2);
    assert(atomic_load(&d->requested_2x2)==1 && atomic_load(&d->recorded_2x2)==1);
    setenv("UGM_VRS_RATE","1x1",1);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p); assert(backend_rate==1);
    assert(atomic_load(&d->requested_1x1)==1 && atomic_load(&d->recorded_1x1)==1);
    vkDestroyPipeline(d->handle,p,NULL);
    setenv("UGM_VRS_RATE","2x2",1);
    d->audit_only=1;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&p)==VK_SUCCESS);
    assert(!dynamic_added && !((Pipeline *)table_find(&d->pipelines,HANDLE_KEY(p)))->injected);
    assert(((Pipeline *)table_find(&d->pipelines,HANDLE_KEY(p)))->rejection==EXTENDED_STATE);
    backend_rate=0; vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,p); assert(!backend_rate);
    assert(atomic_load(&d->requested_2x2)==2 && atomic_load(&d->recorded_2x2)==1);
    assert(atomic_load(&d->passthrough_binds)==1 && atomic_load(&d->accepted)==1);
    assert(atomic_load(&d->bind_excluded[EXTENDED_STATE])==1);
    vkDestroyPipeline(d->handle,p,NULL); d->audit_only=0; expect_preserved=NULL;
    VkBaseInStructure unknown={.sType=(VkStructureType)0x7fff0000};
    ci.pNext=&unknown;
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    int found=0;
    for(unsigned j=0;j<d->detail_count;++j)
        if(d->details[j].value==0x7fff0000 && !strcmp(d->details[j].site,"pipeline.pNext")) found=1;
    assert(found);
    ci.pNext=&clip; assert(pipeline_policy(d,&ci)==EXTENDED_STATE); /* wrong site */
    ci=fixture(); ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR; assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    ci.pMultisampleState=(const void *)(uintptr_t)1; /* ignored GPL state must not be dereferenced */
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    ci=fixture();
    ci.flags=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR; assert(pipeline_policy(d,&ci)==NATIVE);
    ci.flags=VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT; assert(pipeline_policy(d,&ci)==ACCEPT);
#ifdef VK_KHR_maintenance5
    VkPipelineCreateFlags2CreateInfoKHR flags2={.sType=VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
        .flags=VK_PIPELINE_CREATE_2_DISABLE_OPTIMIZATION_BIT_KHR};
    ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR; ci.pNext=&flags2; /* flags2 overrides legacy flags */
    assert(pipeline_policy(d,&ci)==ACCEPT);
#ifdef VK_EXT_descriptor_heap
    flags2.flags=VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
    assert(pipeline_policy(d,&ci)==ACCEPT);
    VkShaderDescriptorSetAndBindingMappingInfoEXT mapping={.sType=VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
    assert(chain_policy(d,&mapping,PS_STAGE)==ACCEPT);
    assert(chain_policy(d,&mapping,PS_MODULE)==ACCEPT);
    assert(chain_policy(d,&mapping,PS_RASTER)==EXTENDED_STATE);
#endif
    flags2.flags=UINT64_C(1)<<60; assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    flags2.flags=VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR; assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
#endif
    ci=fixture();
    VkPipelineMultisampleStateCreateInfo ms=*ci.pMultisampleState; VkSampleMask mask=0;
    ms.pSampleMask=&mask;ci.pMultisampleState=&ms;
    assert(pipeline_policy(d,&ci)==MULTISAMPLE);
    d->caps.fragmentShadingRateWithSampleMask=VK_TRUE; assert(pipeline_policy(d,&ci)==ACCEPT);
    d->caps.fragmentShadingRateWithSampleMask=VK_FALSE;
    mask=1; assert(pipeline_policy(d,&ci)==ACCEPT);
    values[0]=VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;ds.dynamicStateCount=1;ci.pDynamicState=&ds;
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    d->caps.fragmentShadingRateWithSampleMask=VK_TRUE; assert(pipeline_policy(d,&ci)==ACCEPT);
    ms.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT; assert(pipeline_policy(d,&ci)==MULTISAMPLE);
    ci.pDynamicState=NULL;ms.pSampleMask=NULL; d->experimental_msaa=1;
    d->rate_samples[3]=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;
    assert(pipeline_policy(d,&ci)==ACCEPT);
    ms.rasterizationSamples=VK_SAMPLE_COUNT_8_BIT;assert(pipeline_policy(d,&ci)==MULTISAMPLE);
    d->rate_samples[1]=VK_SAMPLE_COUNT_8_BIT;assert(pipeline_policy(d,&ci)==ACCEPT);
    ms.sampleShadingEnable=VK_TRUE;assert(pipeline_policy(d,&ci)==MULTISAMPLE);ms.sampleShadingEnable=VK_FALSE;
    ms.alphaToCoverageEnable=VK_TRUE;assert(pipeline_policy(d,&ci)==MULTISAMPLE);ms.alphaToCoverageEnable=VK_FALSE;
    ms.pSampleMask=&mask;mask=1;assert(pipeline_policy(d,&ci)==MULTISAMPLE);
    mask=255;assert(pipeline_policy(d,&ci)==ACCEPT);
    d->experimental_msaa=0;memset(d->rate_samples,0,sizeof(d->rate_samples));

    ci=fixture();
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        .conservativeRasterizationMode=VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT};
    rs.pNext=&conservative;ci.pRasterizationState=&rs;
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    d->caps.fragmentShadingRateWithConservativeRasterization=VK_TRUE;assert(pipeline_policy(d,&ci)==ACCEPT);
    d->caps.fragmentShadingRateWithConservativeRasterization=VK_FALSE;
    conservative.conservativeRasterizationMode=VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    assert(pipeline_policy(d,&ci)==ACCEPT);
    ci=fixture();ms=*ci.pMultisampleState;
    VkPipelineSampleLocationsStateCreateInfoEXT locations={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,.sampleLocationsEnable=VK_TRUE};
    ms.pNext=&locations;ci.pMultisampleState=&ms;
    assert(pipeline_policy(d,&ci)==MULTISAMPLE);
    d->caps.fragmentShadingRateWithCustomSampleLocations=VK_TRUE;assert(pipeline_policy(d,&ci)==ACCEPT);
    d->caps.fragmentShadingRateWithCustomSampleLocations=VK_FALSE;
    locations.sampleLocationsEnable=VK_FALSE;assert(pipeline_policy(d,&ci)==ACCEPT);
    /* Both dynamic states are legal only on hardware supporting custom samples
     * with coarse shading, regardless of the enable value set after bind. */
    ci=fixture(); ci.pDynamicState=&ds; ds.dynamicStateCount=2;
    values[0]=VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT;
    values[1]=VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT;
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    d->caps.fragmentShadingRateWithCustomSampleLocations=VK_TRUE;
    assert(pipeline_policy(d,&ci)==ACCEPT);
    d->caps.fragmentShadingRateWithCustomSampleLocations=VK_FALSE;
    ds.dynamicStateCount=1;
    ci=fixture(); values[0]=(VkDynamicState)0x7fff0001; ci.pDynamicState=&ds;
    assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    VkPipelineViewportStateCreateInfo viewport={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.pNext=&unknown};
    ci=fixture();ci.pViewportState=&viewport;assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    VkPipelineRasterizationLineStateCreateInfoEXT line={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT};
    ci=fixture();rs.pNext=&line;ci.pRasterizationState=&rs;assert(pipeline_policy(d,&ci)==ACCEPT);
    line.stippledLineEnable=VK_TRUE;assert(pipeline_policy(d,&ci)==EXTENDED_STATE);
    /* Diagnostic capacity is finite and overflow is visible, never an OOM. */
    for(unsigned j=0;j<VRS_DETAIL_LIMIT+5;++j) policy_detail(d,"test","unknown",j,"overflow-probe");
    assert(d->detail_count==VRS_DETAIL_LIMIT && d->detail_overflow>0);
    d->debug=0;d->detail_count=0;d->detail_overflow=0;
    memset(&d->caps,0,sizeof(d->caps));
    puts("Generic VRS: preserved chains, device gates, flags2, unknown states, audit-only and counter separation OK");
}
/* Original libraries remain static; only complete proven links use private
 * shader-bearing variants. Includes ignored pointers, nested and mixed links. */
static void gpl_tests(Device *d) {
    VkPipeline libs[4];
    for(unsigned j=0;j<4;++j) {
        VkGraphicsPipelineCreateInfo ci=fixture();
        VkGraphicsPipelineLibraryCreateInfoEXT part={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,.flags=1u<<j};
        VkPipelineCreateFlags2CreateInfoKHR flags2={.sType=VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            .pNext=&part,.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT};
#ifdef VK_EXT_descriptor_heap
        flags2.flags|=VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
#endif
        ci.pNext=&flags2;
        ci.stageCount=(j==1 || j==2) ? 1 : 0;
        if(j==2) ++ci.pStages;
        /* The caller may supply arbitrary ignored pointers for other subsets. */
        if(j!=1 && j!=2) ci.pStages=(const void *)(uintptr_t)1;
        if(j!=1) ci.pRasterizationState=(const void *)(uintptr_t)1;
        if(j!=2 && j!=3) ci.pMultisampleState=(const void *)(uintptr_t)1;
        if(j!=3) ci.pColorBlendState=(const void *)(uintptr_t)1;
        if(j!=0) ci.pVertexInputState=(const void *)(uintptr_t)1;
        VkGraphicsPipelineCreateInfo before=ci;
        assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&libs[j])==VK_SUCCESS);
        assert(!memcmp(&before,&ci,sizeof(ci)));
        Pipeline *p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(libs[j]));
        assert(p && p->library && !p->injected && p->parts==(1u<<j));
        assert((p->shadow!=VK_NULL_HANDLE)==(j==1 || j==2));
    }
    VkPipelineLibraryCreateInfoKHR link={.sType=VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,.libraryCount=4,.pLibraries=libs};
    VkGraphicsPipelineCreateInfo ci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&link};
    VkPipeline executable;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    Pipeline *p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(executable));assert(p && p->injected && !p->library);
    void *key=d->key;VkCommandBuffer cmd=(VkCommandBuffer)&key;
    backend_rate=0;vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,executable);assert(backend_rate==2);
    setenv("UGM_VRS_RATE","1x1",1);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,executable);assert(backend_rate==1);
    setenv("UGM_VRS_RATE","2x2",1);vkDestroyPipeline(d->handle,executable,NULL);
    /* Intermediate libraries summarize proofs; no application pointers retained. */
    link.libraryCount=2;link.pLibraries=&libs[1];ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    VkPipeline middle;assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&middle)==VK_SUCCESS);
    VkPipeline nested[]={libs[0],middle,libs[3]};link.libraryCount=3;link.pLibraries=nested;ci.flags=0;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(executable));assert(p && p->injected);
    vkDestroyPipeline(d->handle,executable,NULL);vkDestroyPipeline(d->handle,middle,NULL);
    /* Local vertex subset plus linked VI/FS/output. */
    VkPipeline mixed[]={libs[0],libs[2],libs[3]};link.pLibraries=mixed;
    VkGraphicsPipelineLibraryCreateInfoEXT local={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,.flags=GPL_PRE,.pNext=&link};
    ci=fixture();ci.stageCount=1;ci.pNext=&local;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(executable));assert(p && p->injected);
    vkDestroyPipeline(d->handle,executable,NULL);
    /* Untracked/unsafe/overlapping inputs cannot become coarse. */
    ci=(VkGraphicsPipelineCreateInfo){.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&link};
    VkPipeline bad[]={libs[0],(VkPipeline)(uintptr_t)999,libs[2],libs[3]};link.libraryCount=4;link.pLibraries=bad;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    assert(!table_find(&d->pipelines,HANDLE_KEY(executable)));
    backend_rate=0;vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,executable);assert(!backend_rate);
    vkDestroyPipeline(d->handle,executable,NULL);
    bad[1]=libs[0];assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    assert(!table_find(&d->pipelines,HANDLE_KEY(executable)));vkDestroyPipeline(d->handle,executable,NULL);
    /* Audit never creates private GPL variants or injects linked executables. */
    link.pLibraries=libs;d->audit_only=1;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    assert(!table_find(&d->pipelines,HANDLE_KEY(executable)));vkDestroyPipeline(d->handle,executable,NULL);d->audit_only=0;
    for(unsigned j=0;j<4;++j) vkDestroyPipeline(d->handle,libs[j],NULL);
    assert(!d->pipelines.count);
    /* Private creation/OOM cannot turn successful application creation into an
     * error or leave dynamic state on an untracked application handle. */
    ci=fixture();local.flags=GPL_PRE;local.pNext=NULL;ci.pNext=&local;ci.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;ci.stageCount=1;
    fail_gpl_shadow=1;
    assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
    assert(!table_find(&d->pipelines,HANDLE_KEY(executable)));vkDestroyPipeline(d->handle,executable,NULL);fail_gpl_shadow=0;
    for(int fail=0;fail<9;++fail) {
        fail_allocation=fail;
        assert(vkCreateGraphicsPipelines(d->handle,0,1,&ci,NULL,&executable)==VK_SUCCESS);
        fail_allocation=-1;
        p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(executable));
        if(p) assert(p->library && p->shadow && !p->injected);
        vkDestroyPipeline(d->handle,executable,NULL);assert(!d->pipelines.count);
    }
    puts("GPL: private variants, full/nested/mixed links, ignored pointers, audit, OOM and driver-failure fallback OK");
}
/* Whole-batch semantics must survive the GPL staging shortcut. Driver errors
 * can still return live handles, and audit/mixed batches keep the old route. */
static void gpl_batch_tests(Device *d) {
    VkGraphicsPipelineLibraryCreateInfoEXT part={
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,.flags=GPL_VI};
    VkPipelineCreateFlags2CreateInfoKHR flags2={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
        .pNext=&part,.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR};
    VkGraphicsPipelineCreateInfo batch[3];
    VkPipeline handles[3];
    VkAllocationCallbacks allocator={0}; /* Mock only: never called. */
    VkPipelineCache cache=(VkPipelineCache)(uintptr_t)73;
    VkResult results[]={VK_SUCCESS,VK_ERROR_OUT_OF_DEVICE_MEMORY,VK_PIPELINE_COMPILE_REQUIRED_EXT};
    for(unsigned audit=0;audit<2;++audit) for(unsigned debug=0;debug<2;++debug)
    for(unsigned r=0;r<3;++r) {
        for(unsigned i=0;i<3;++i) batch[i]=(VkGraphicsPipelineCreateInfo){
            .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&flags2};
        VkGraphicsPipelineCreateInfo before[3];memcpy(before,batch,sizeof(batch));
        d->audit_only=(int)audit;d->debug=(int)debug;pipeline_result=results[r];
        uint64_t analyzed=atomic_load(&d->analyzed),libraries=atomic_load(&d->gpl_libraries);
        expect_batch=batch;expect_batch_count=3;expect_batch_cache=cache;
        expect_batch_allocator=&allocator;expect_batch_out=handles;
        assert(vkCreateGraphicsPipelines(d->handle,cache,3,batch,&allocator,handles)==results[r]);
        assert(!expect_batch && !memcmp(before,batch,sizeof(batch)));
        unsigned live=0;
        for(unsigned i=0;i<3;++i) {
            assert(!!handles[i]==(r==0 || i!=1));
            if(!handles[i]) continue;
            ++live;
            Pipeline *p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(handles[i]));
            if(!audit) assert(p && p->library && p->parts==GPL_VI && !p->injected && !p->shadow);
            else assert(!p || (!p->library && !p->injected));
            vkDestroyPipeline(d->handle,handles[i],&allocator);
        }
        assert(!d->pipelines.count);
        assert(atomic_load(&d->analyzed)==analyzed+(debug ? 3 : 0));
        assert(atomic_load(&d->gpl_libraries)==libraries+(debug && !audit ? live : 0));
    }
    d->debug=0;d->audit_only=0;pipeline_result=VK_SUCCESS;
    /* Preserve derivative base indices/handles and flags2 without splitting. */
    for(unsigned i=0;i<3;++i) batch[i]=(VkGraphicsPipelineCreateInfo){
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&part,
        .flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_DERIVATIVE_BIT,
        .basePipelineIndex=i==1 ? 0 : -1,
        .basePipelineHandle=i==1 ? VK_NULL_HANDLE : (VkPipeline)(uintptr_t)77};
    batch[0].flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    expect_batch=batch;expect_batch_count=3;expect_batch_cache=cache;
    expect_batch_allocator=&allocator;expect_batch_out=handles;
    assert(vkCreateGraphicsPipelines(d->handle,cache,3,batch,&allocator,handles)==VK_SUCCESS);
    assert(!expect_batch && !d->pipelines.count);
    for(unsigned i=0;i<3;++i) vkDestroyPipeline(d->handle,handles[i],&allocator);
    /* Monolithic and GPL entries in either order retain their own policy. */
    for(unsigned reverse=0;reverse<2;++reverse) {
        batch[reverse]=(VkGraphicsPipelineCreateInfo){
            .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&flags2};
        batch[1-reverse]=fixture();
        assert(vkCreateGraphicsPipelines(d->handle,cache,2,batch,&allocator,handles)==VK_SUCCESS);
        Pipeline *lib=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(handles[reverse]));
        Pipeline *mono=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(handles[1-reverse]));
        assert(lib && lib->library && !lib->injected);
        assert(mono && mono->injected && !mono->library);
        for(unsigned i=0;i<2;++i) vkDestroyPipeline(d->handle,handles[i],&allocator);
        assert(!d->pipelines.count);
    }
    puts("GPL batches: success/partial/compile-required, flags2, derivatives, audit/debug, mixed order and allocator forwarding OK");
}
static void identifier_tests(void) {
    Device d={0}; assert(!pthread_mutex_init(&d.objects_lock,NULL));
    uint32_t id=1; unsigned flags=123;
    assert(!shader_proof_find(&d,sizeof(id),&id,&flags));
    fail_allocation=0; shader_proof_store(&d,sizeof(id),&id,SHADER_VALID); fail_allocation=-1;
    assert(!d.shader_proofs);
    shader_proof_store(&d,sizeof(id),&id,SHADER_VALID|SHADER_POSITION_COARSE);
    assert(shader_proof_find(&d,sizeof(id),&id,&flags) && flags==(SHADER_VALID|SHADER_POSITION_COARSE));
    uint32_t collision=2;
    while(shader_proof_hash(sizeof(id),(const uint8_t *)&id)!=shader_proof_hash(sizeof(collision),(const uint8_t *)&collision)) ++collision;
    shader_proof_store(&d,sizeof(collision),&collision,SHADER_VALID|SHADER_PIXEL_INPUT);
    assert(shader_proof_find(&d,sizeof(collision),&collision,&flags) && flags==(SHADER_VALID|SHADER_PIXEL_INPUT));
    assert(shader_proof_find(&d,sizeof(id),&id,&flags) && flags==(SHADER_VALID|SHADER_POSITION_COARSE));
    VkPipelineShaderStageModuleIdentifierCreateInfoEXT ident={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT,
        .identifierSize=sizeof(id),.pIdentifier=(const uint8_t *)&id};
    VkPipelineShaderStageCreateInfo stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext=&ident,.stage=VK_SHADER_STAGE_VERTEX_BIT,.pName="main"};
    VkGraphicsPipelineCreateInfo ci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=1,.pStages=&stage};
    assert(stage_flags(&d,&ci,VK_SHADER_STAGE_VERTEX_BIT)==(SHADER_VALID|SHADER_POSITION_COARSE));
    shader_proof_store(&d,sizeof(id),&id,SHADER_VALID);
    assert(stage_flags(&d,&ci,VK_SHADER_STAGE_VERTEX_BIT)==0); /* conflicting evidence */
    shader_proof_store(&d,sizeof(id),&id,SHADER_VALID|SHADER_POSITION_COARSE);
    assert(stage_flags(&d,&ci,VK_SHADER_STAGE_VERTEX_BIT)==0); /* sticky invalid */
    assert(!shader_proof_find(&d,0,&id,&flags));
    assert(!shader_proof_find(&d,VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT+1,&id,&flags));
    assert(!shader_proof_find(&d,4,NULL,&flags));
    for(uint32_t n=2;n<VRS_IDENTIFIER_LIMIT+32;++n) shader_proof_store(&d,sizeof(n),&n,SHADER_VALID);
    uint32_t absent=UINT32_MAX;
    shader_proof_store(&d,sizeof(absent),&absent,SHADER_VALID);
    assert(!shader_proof_find(&d,sizeof(absent),&absent,&flags));
    free(d.shader_proofs); pthread_mutex_destroy(&d.objects_lock);
    puts("Shader IDs: exact collision handling, observed-only, conflicting evidence, OOM and bounded capacity OK");
}
static void intermediate_tests(void) {
    Device d={0};
    VkExtent2D r=device_rate(&d,(VkExtent2D){2,2}); assert(r.width==2 && r.height==2);
    for(unsigned w=1;w<=2;++w) {
        d.experimental_rate=(VkExtent2D){w,3-w};
        d.supported_rates=1; d.debug=1;
        r=device_rate(&d,(VkExtent2D){2,2}); assert(r.width==1 && r.height==1);
        d.supported_rates|=rate_bit(d.experimental_rate);
        r=device_rate(&d,(VkExtent2D){2,2}); assert(r.width==w && r.height==3-w);
        r=device_rate(&d,(VkExtent2D){1,1}); assert(r.width==1 && r.height==1);
    }
    assert(atomic_load(&d.unsupported_rate)==2);
    assert(!rate_bit((VkExtent2D){0,2}) && !rate_bit((VkExtent2D){4,4}));
    puts("Intermediate rates: support mask, orientation, QAM disable and fallback OK");
}
int main(void) {
    intermediate_tests();
    identifier_tests();
    unsetenv("RADV_FORCE_VRS");unsetenv("RADV_FORCE_VRS_CONFIG_FILE");unsetenv("UGM_VRS_CONFIG_FILE");setenv("UGM_VRS_RATE","2x2",1);
    for(unsigned j=0;j<2;++j) {
        instance_keys[j]=&instance_keys[j];physical_keys[j]=instance_keys[j];expected_instance=(VkInstance)&instance_keys[j];
        VkLayerInstanceLink link={.pfnNextGetInstanceProcAddr=j ? gipa_b : gipa_a};
        VkLayerInstanceCreateInfo chain={.sType=VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO,.function=VK_LAYER_LINK_INFO,.u.pLayerInfo=&link};
        VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.apiVersion=VK_API_VERSION_1_1};
        VkInstanceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pNext=&chain,.pApplicationInfo=&app};
        VkInstance out;assert(vkCreateInstance(&ci,NULL,&out)==VK_SUCCESS);
    }
    VkDevice a=make_device(0),b=make_device(1);
    assert(!lookup(a)->active && !lookup(b)->active);
    void *key=dispatch_key(a);VkCommandBuffer cmd=(VkCommandBuffer)&key;
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,(VkPipeline)999);assert(backend_binds==1);
    vkDestroyDevice(b,NULL);
    expected_instance=(VkInstance)&instance_keys[1];vkDestroyInstance(expected_instance,NULL);
    assert(destroyed_instances==1);
    /* Dispatch from the first instance must remain valid after the second dies. */
    advertise=1;VkDevice active=make_device(0);Device *d=lookup(active);assert(d->active);
    fail_allocation=4;expect_unmodified=1;
    VkDevice oom=make_device(0);fail_allocation=-1;expect_unmodified=0;
    assert(!lookup(oom)->active);vkDestroyDevice(oom,NULL);
    int ext_enabled=0;
    VkDeviceCreateInfo feature_ci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    const char *bypass[]={VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,"VK_EXT_shader_object","VK_NV_device_generated_commands","VK_EXT_device_generated_commands"};
    for(unsigned j=0;j<sizeof(bypass)/sizeof(*bypass);++j) {
        feature_ci.enabledExtensionCount=1; feature_ci.ppEnabledExtensionNames=&bypass[j];
        assert(!eligible((VkPhysicalDevice)&physical_keys[0],gipa_a,expected_instance,VK_API_VERSION_1_1,&feature_ci,&ext_enabled,&d->caps));
    }
    feature_ci.enabledExtensionCount=0;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR own_features={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
    feature_ci.pNext=&own_features;
    assert(!eligible((VkPhysicalDevice)&physical_keys[0],gipa_a,expected_instance,VK_API_VERSION_1_1,&feature_ci,&ext_enabled,&d->caps));
    feature_ci.pNext=NULL;
    if(UGM_VRS_VENDOR_ID==0x1002) {
        setenv("RADV_FORCE_VRS","2x2",1);
        assert(!eligible((VkPhysicalDevice)&physical_keys[0],gipa_a,expected_instance,VK_API_VERSION_1_1,&feature_ci,&ext_enabled,&d->caps));
        unsetenv("RADV_FORCE_VRS");
    }
    for(uint64_t j=1;j<=2;++j) { Shader *s=calloc(1,sizeof(*s));assert(s);s->node.key=j;s->flags=SHADER_VALID|(j==1 ? SHADER_POSITION_COARSE : 0);table_insert(&d->shaders,&s->node); }
    compatibility_tests(d);
    gpl_tests(d);
    gpl_batch_tests(d);
    VkGraphicsPipelineCreateInfo ci=fixture();assert(pipeline_policy(d,&ci)==ACCEPT);
    VkDynamicState unsafe[]={VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT};
    for(unsigned i=0;i<3;++i) { VkPipelineDynamicStateCreateInfo dynamic={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=1,.pDynamicStates=&unsafe[i]};ci.pDynamicState=&dynamic;assert(pipeline_policy(d,&ci)==EXTENDED_STATE); }
    ci=fixture(); VkPipelineFragmentShadingRateStateCreateInfoKHR native={.sType=VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR};ci.pNext=&native;assert(pipeline_policy(d,&ci)==NATIVE);ci=fixture();
    /* Allocation failure must never inject state without registered tracking. */
    for(int failure=0;failure<=6;++failure) {
        fail_allocation=failure;VkPipeline p;assert(vkCreateGraphicsPipelines(active,0,1,&ci,NULL,&p)==VK_SUCCESS);fail_allocation=-1;
        assert((table_find(&d->pipelines,HANDLE_KEY(p))!=NULL)==(dynamic_added!=0));vkDestroyPipeline(active,p,NULL);
    }
    pipeline_result=VK_ERROR_OUT_OF_DEVICE_MEMORY;VkPipeline p;
    assert(vkCreateGraphicsPipelines(active,0,1,&ci,NULL,&p)==pipeline_result);assert(table_find(&d->pipelines,HANDLE_KEY(p)));vkDestroyPipeline(active,p,NULL);pipeline_result=VK_SUCCESS;
    VkGraphicsPipelineCreateInfo batch[3]={fixture(),fixture(),fixture()};VkPipeline partial[3];
    pipeline_result=VK_ERROR_OUT_OF_DEVICE_MEMORY;
    assert(vkCreateGraphicsPipelines(active,0,3,batch,NULL,partial)==pipeline_result);
    assert(partial[0] && !partial[1] && partial[2] && d->pipelines.count==2);
    for(unsigned j=0;j<3;++j) vkDestroyPipeline(active,partial[j],NULL);
    assert(!d->pipelines.count);pipeline_result=VK_SUCCESS;
    /* Reset/begin/execute/shader objects are passed to the correct device. No
     * cached command state exists to survive reset, secondary CBs or reuse. */
    assert(vkGetDeviceProcAddr(active,"vkResetCommandPool")==opaque_command);
    assert(vkGetDeviceProcAddr(active,"vkResetCommandBuffer")==opaque_command);
    assert(vkGetDeviceProcAddr(active,"vkBeginCommandBuffer")==opaque_command);
    assert(vkGetDeviceProcAddr(active,"vkCmdExecuteCommands")==opaque_command);
    pthread_t threads[8];for(unsigned i=0;i<8;++i) assert(!pthread_create(&threads[i],NULL,worker,d));
    for(unsigned i=0;i<8;++i) assert(!pthread_join(threads[i],NULL));
    assert(!d->pipelines.count);
    /* Same low 32 bits must not alias on 32-bit Vulkan. */
    Pipeline *low=calloc(1,sizeof(*low)),*high=calloc(1,sizeof(*high));assert(low&&high);low->node.key=7;high->node.key=UINT64_C(0x100000007);
    table_insert(&d->pipelines,&low->node);table_insert(&d->pipelines,&high->node);
    assert(table_find(&d->pipelines,7)==&low->node);assert(table_find(&d->pipelines,UINT64_C(0x100000007))==&high->node);
    table_remove(&d->pipelines,7);table_remove(&d->pipelines,UINT64_C(0x100000007));
    vkDestroyDevice(active,NULL);vkDestroyDevice(a,NULL);
    assert(destroyed_devices==4 && !devices);
    /* Reuse the previous active device's exact dispatch address. */
    created_devices=2;advertise=0;VkDevice reused=make_device(0);assert(!lookup(reused)->active);
    key=dispatch_key(reused);backend_rate=0;vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,(VkPipeline)999);assert(!backend_rate);
    vkDestroyDevice(reused,NULL);assert(destroyed_devices==5);
    expected_instance=(VkInstance)&instance_keys[0];vkDestroyInstance(expected_instance,NULL);assert(!instances);
    puts("VRS core: multi-instance/device dispatch, OOM, partial batches, 8-thread lifecycle and 64-bit handle keys OK");return 0;
}
