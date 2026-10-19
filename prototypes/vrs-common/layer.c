/* Shared UGM VRS core. Dispatch keys follow Khronos LoaderLayerInterface.
 * Valid Vulkan lifetime/external synchronization is required of the caller. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _TIME_BITS
#define _TIME_BITS 64
#endif
#define UGM_VRS_SHARED_CORE 1
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include "table.h"
#include "spirv_policy.h"
#include "rate.h"

typedef struct Instance {
    VkInstance handle;
    void *key;
    uint32_t api;
    PFN_vkGetInstanceProcAddr gipa;
    PFN_GetPhysicalDeviceProcAddr gpdpa;
    PFN_vkDestroyInstance destroy;
    struct Instance *next;
} Instance;
typedef struct Pipeline {
    Node node;
    VkPipeline shadow; /* private GPL variant; original application handle unchanged */
    unsigned parts;
    int library, injected;
    VkSampleCountFlags samples;
    unsigned rejection; /* debug-only first exclusion; never a GPL proof */
} Pipeline;
typedef struct Shader { Node node; unsigned flags; } Shader;
#define VRS_IDENTIFIER_LIMIT 8192u
typedef struct ShaderProof {
    uint32_t size;
    unsigned flags;
    uint8_t identifier[VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT];
} ShaderProof;
enum Exclusion { ACCEPT, NATIVE, EXTENDED_STATE, STAGES, FRAGMENT, POSITION, MULTISAMPLE, BLENDING, ALLOCATION, REASON_COUNT };
/* Bounded diagnostics: no allocations or unbounded log growth per pipeline. */
#define VRS_DETAIL_LIMIT 192
typedef struct PolicyDetail {
    const char *site, *name, *rule;
    uint64_t value;
    unsigned long count, reported;
} PolicyDetail;
typedef struct Device {
    VkDevice handle;
    void *key;
    PFN_vkGetDeviceProcAddr next_gdpa;
    PFN_vkCreateGraphicsPipelines create_pipelines;
    PFN_vkCreateShaderModule create_shader;
    PFN_vkDestroyShaderModule destroy_shader;
    PFN_vkGetShaderModuleCreateInfoIdentifierEXT shader_identifier;
    ShaderProof *shader_proofs;
    PFN_vkDestroyDevice destroy;
    PFN_vkDestroyPipeline destroy_pipeline;
    PFN_vkCmdBindPipeline bind_pipeline;
    PFN_vkCmdSetFragmentShadingRateKHR set_rate;
    pthread_mutex_t objects_lock;
    Table pipelines, shaders;
    RateCache rate;
    int active, debug, audit_only;
    VkExtent2D experimental_rate;
    unsigned supported_rates;
    VkSampleCountFlags rate_samples[4];
    int experimental_msaa;
    atomic_ulong recorded_2x1, recorded_1x2, unsupported_rate;
    atomic_ulong bind_excluded[REASON_COUNT+1];
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR caps;
    PolicyDetail details[VRS_DETAIL_LIMIT];
    unsigned detail_count;
    unsigned long detail_overflow;
    atomic_ulong accepted, create_failed, graphics_binds, requested_1x1, requested_2x2;
    atomic_ulong recorded_1x1, recorded_2x2, passthrough_binds, next_report_sec;
    atomic_ulong analyzed, injected, binds, gpl_libraries, gpl_shadows, gpl_linked;
    atomic_ulong excluded[REASON_COUNT], fragment_reasons[7];
    struct Device *next;
} Device;
static pthread_mutex_t registry_lock=PTHREAD_MUTEX_INITIALIZER;
static Instance *instances;
static Device *devices;
static atomic_uint registry_epoch=1;
#define TRACE(msg) do { if (getenv("UGM_VRS_DEBUG")) fprintf(stderr, "UGM VRS: %s\n", msg); } while (0)
static void *dispatch_key(const void *handle) { return handle ? *(void *const *)handle : NULL; }
static Instance *instance_lookup(const void *handle) {
    void *key=dispatch_key(handle);
    pthread_mutex_lock(&registry_lock);
    Instance *i=instances; while(i && i->key!=key) i=i->next;
    pthread_mutex_unlock(&registry_lock);
    return i;
}
static Device *lookup(const void *handle) {
    void *key=dispatch_key(handle);
#if UINTPTR_MAX > UINT32_MAX
    /* x86-64 TLS has a stable loader ABI. Current i386 linkers add a new
     * GLIBC_ABI_GNU_TLS requirement even for traditional TLS relocations.
     * Keep i386 compatible with older Steam runtimes via the short registry lock. */
    static _Thread_local void *cached_key;
    static _Thread_local Device *cached_device;
    static _Thread_local unsigned cached_epoch;
    unsigned epoch=atomic_load_explicit(&registry_epoch,memory_order_acquire);
    if(cached_epoch==epoch && cached_key==key) return cached_device;
#endif
    pthread_mutex_lock(&registry_lock);
    Device *d=devices; while(d && d->key!=key) d=d->next;
#if UINTPTR_MAX > UINT32_MAX
    cached_key=key; cached_device=d;
    cached_epoch=atomic_load_explicit(&registry_epoch,memory_order_relaxed);
#endif
    pthread_mutex_unlock(&registry_lock);
    return d;
}

/* The ordinary QAM parser remains 1x1/2x2. Experimental rates are opt-in,
 * immutable per device, and only replace a requested 2x2. */
static unsigned rate_bit(VkExtent2D r) {
    return r.width>=1 && r.width<=2 && r.height>=1 && r.height<=2 ?
        1u<<((r.width-1)*2+r.height-1) : 0;
}
static VkExtent2D device_rate(Device *d,VkExtent2D r) {
    if(r.width==2 && r.height==2 && d->experimental_rate.width) {
        r=d->experimental_rate;
        if(!(d->supported_rates&rate_bit(r))) {
            if(d->debug) atomic_fetch_add_explicit(&d->unsupported_rate,1,memory_order_relaxed);
            return (VkExtent2D){1,1};
        }
    }
    return r;
}
static unsigned query_rates(VkPhysicalDevice physical,PFN_vkGetInstanceProcAddr gipa,VkInstance inst,VkSampleCountFlags *samples) {
    PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR get=(PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)
        gipa(inst,"vkGetPhysicalDeviceFragmentShadingRatesKHR");
    uint32_t n=0;
    if(!get || get(physical,&n,NULL)!=VK_SUCCESS || !n || n>128) return 1;
    VkPhysicalDeviceFragmentShadingRateKHR r[128]={0};
    for(uint32_t j=0;j<n;++j) r[j].sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
    if(get(physical,&n,r)!=VK_SUCCESS) return 1;
    unsigned mask=1;
    for(uint32_t j=0;j<n;++j) {
        VkExtent2D size=r[j].fragmentSize;
        if(!rate_bit(size)) continue;
        samples[(size.width-1)*2+size.height-1]|=r[j].sampleCounts;
        if(r[j].sampleCounts&VK_SAMPLE_COUNT_1_BIT) mask|=rate_bit(size);
    }
    return mask;
}

static VkLayerInstanceCreateInfo *instance_link(const VkInstanceCreateInfo *ci) {
    for (const VkBaseInStructure *p = ci->pNext; p; p = p->pNext) {
        if (p->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO) {
            VkLayerInstanceCreateInfo *link = (VkLayerInstanceCreateInfo *)p;
            if (link->function == VK_LAYER_LINK_INFO) return link;
        }
    }
    return NULL;
}

static VkLayerDeviceCreateInfo *device_link(const VkDeviceCreateInfo *ci) {
    for (const VkBaseInStructure *p = ci->pNext; p; p = p->pNext) {
        if (p->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO) {
            VkLayerDeviceCreateInfo *link = (VkLayerDeviceCreateInfo *)p;
            if (link->function == VK_LAYER_LINK_INFO) return link;
        }
    }
    return NULL;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *ci,
        const VkAllocationCallbacks *allocator, VkInstance *out) {
    VkLayerInstanceCreateInfo *link=instance_link(ci);
    if(!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_GetPhysicalDeviceProcAddr gpdpa=link->u.pLayerInfo->pfnNextGetPhysicalDeviceProcAddr;
    PFN_vkCreateInstance create=(PFN_vkCreateInstance)gipa(NULL,"vkCreateInstance");
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    if(!create) return VK_ERROR_INITIALIZATION_FAILED;
    Instance *i=calloc(1,sizeof(*i)); if(!i) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult result=create(ci,allocator,out);
    if(result!=VK_SUCCESS) { free(i); return result; }
    i->handle=*out; i->key=dispatch_key(*out); i->gipa=gipa; i->gpdpa=gpdpa;
    i->api=ci->pApplicationInfo && ci->pApplicationInfo->apiVersion ? ci->pApplicationInfo->apiVersion : VK_API_VERSION_1_0;
    i->destroy=(PFN_vkDestroyInstance)gipa(*out,"vkDestroyInstance");
    pthread_mutex_lock(&registry_lock); i->next=instances; instances=i;
    pthread_mutex_unlock(&registry_lock);
    return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance inst, const VkAllocationCallbacks *allocator) {
    if(!inst) return;
    pthread_mutex_lock(&registry_lock);
    Instance **p=&instances,*i=NULL;
    while(*p) { if((*p)->handle==inst) { i=*p; *p=i->next; break; } p=&(*p)->next; }
    pthread_mutex_unlock(&registry_lock);
    if(i) { i->destroy(inst,allocator); free(i); }
}
static int eligible(VkPhysicalDevice physical, PFN_vkGetInstanceProcAddr gipa,
                    VkInstance inst, uint32_t api, const VkDeviceCreateInfo *ci, int *ext_enabled,
                    VkPhysicalDeviceFragmentShadingRatePropertiesKHR *caps) {
    for (const VkBaseInStructure *p=ci->pNext;p;p=p->pNext)
        if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR ||
           p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV ||
           p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT)
            return 0;
    for(uint32_t j=0;j<ci->enabledExtensionCount;++j)
        if(!strcmp(ci->ppEnabledExtensionNames[j],VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) ||
           !strcmp(ci->ppEnabledExtensionNames[j],VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME) ||
           !strcmp(ci->ppEnabledExtensionNames[j],VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME) ||
           !strcmp(ci->ppEnabledExtensionNames[j],"VK_EXT_shader_object") ||
           !strcmp(ci->ppEnabledExtensionNames[j],"VK_NV_device_generated_commands") ||
           !strcmp(ci->ppEnabledExtensionNames[j],"VK_EXT_device_generated_commands"))
            return 0;
    PFN_vkGetPhysicalDeviceProperties2 properties2 =
        (PFN_vkGetPhysicalDeviceProperties2)gipa(inst, "vkGetPhysicalDeviceProperties2");
    PFN_vkEnumerateDeviceExtensionProperties extensions =
        (PFN_vkEnumerateDeviceExtensionProperties)gipa(inst, "vkEnumerateDeviceExtensionProperties");
    PFN_vkGetPhysicalDeviceFeatures2 features2 =
        (PFN_vkGetPhysicalDeviceFeatures2)gipa(inst, "vkGetPhysicalDeviceFeatures2");
    PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR rates =
        (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)
        gipa(inst, "vkGetPhysicalDeviceFragmentShadingRatesKHR");
    if (!properties2 || !extensions || !features2 || !rates) {
        TRACE("missing physical query proc"); return 0;
    }
    VkPhysicalDeviceDriverProperties driver = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
    properties2(physical, &props);
    if(getenv("UGM_VRS_DEBUG")) fprintf(stderr,"UGM VRS: route=%s GPU=%s vendor=%04x device=%04x driverID=%u driver=%s info=%s\n",
        UGM_VRS_VENDOR,props.properties.deviceName,props.properties.vendorID,props.properties.deviceID,(unsigned)driver.driverID,driver.driverName,driver.driverInfo);
    if (props.properties.vendorID != UGM_VRS_VENDOR_ID ||
        driver.driverID != UGM_VRS_DRIVER_ID) {
        TRACE("vendor/driver mismatch"); return 0;
    }
    if (UGM_VRS_VENDOR_ID == 0x1002) {
        if (getenv("RADV_FORCE_VRS")) return 0;
        const uint32_t native_apus[] = {0x1435,0x1506,0x163f,0x164d,0x164e,0x1681};
        for (unsigned j=0;j<sizeof(native_apus)/sizeof(*native_apus);++j)
            if(props.properties.deviceID==native_apus[j] &&
               props.properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
               getenv("RADV_FORCE_VRS_CONFIG_FILE")) return 0;
    }
    uint32_t count = 0;
    if (extensions(physical, NULL, &count, NULL) != VK_SUCCESS || count == 0 || count > 4096)
        return 0;
    VkExtensionProperties *exts = calloc(count, sizeof(*exts));
    if (!exts) return 0;
    int available = 0, renderpass2 = api >= VK_API_VERSION_1_2;
    if (extensions(physical, NULL, &count, exts) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; i++) {
            if (!strcmp(exts[i].extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)) available=1;
            if (!strcmp(exts[i].extensionName, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) renderpass2=1;
        }
    free(exts);
    if (!available || !renderpass2) { TRACE("extension unavailable"); return 0; }
    *caps=(VkPhysicalDeviceFragmentShadingRatePropertiesKHR){
        .sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 rate_props={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,.pNext=caps};
    properties2(physical,&rate_props);
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR shading = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &shading};
    features2(physical, &features);
    if (!shading.pipelineFragmentShadingRate) { TRACE("pipeline feature unavailable"); return 0; }
    count = 0;
    if (rates(physical, &count, NULL) != VK_SUCCESS || count == 0 || count > 128)
        return 0;
    VkPhysicalDeviceFragmentShadingRateKHR *supported = calloc(count, sizeof(*supported));
    if (!supported) return 0;
    for (uint32_t i = 0; i < count; i++)
        supported[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
    int has_2x2 = 0;
    if (rates(physical, &count, supported) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; i++) {
            if (!(supported[i].sampleCounts & VK_SAMPLE_COUNT_1_BIT)) continue;
            if (supported[i].fragmentSize.width == 2 &&
                supported[i].fragmentSize.height == 2) has_2x2 = 1;
        }
    free(supported);
    if (!has_2x2) { TRACE("2x2 unavailable"); return 0; }
    *ext_enabled = 0;
    for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
        if (!strcmp(ci->ppEnabledExtensionNames[i], VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
            *ext_enabled = 1;
    /* Do not alter a feature structure already supplied by the application. */
    for (const VkBaseInStructure *p = ci->pNext; p; p = p->pNext) {
        if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
            return 0;
        if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            for (const VkBaseInStructure *nested = p->pNext; nested; nested = nested->pNext)
                if (nested->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
                    return 0;
    }
    TRACE("eligible vendor pipeline 2x2");
    return 1;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical,
        const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *allocator, VkDevice *out) {
    TRACE("create device enter");
    VkLayerDeviceCreateInfo *link = device_link(ci);
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    Instance *owner=instance_lookup(physical);
    VkInstance inst=owner ? owner->handle : VK_NULL_HANDLE;
    PFN_vkCreateDevice create = (PFN_vkCreateDevice)gipa(inst, "vkCreateDevice");
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    int ext_enabled = 0;
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR caps={0};
    TRACE("eligible enter");
    int active = owner && owner->api >= VK_API_VERSION_1_1 && eligible(physical, gipa, inst, owner->api, ci, &ext_enabled, &caps);
    TRACE("eligible returned");
    if (getenv("UGM_VRS_DEBUG")) fprintf(stderr,"UGM VRS: active=%d\n",active);
    VkDeviceCreateInfo modified = *ci;
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR shading = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
        .pNext = (void *)ci->pNext, .pipelineFragmentShadingRate = VK_TRUE};
    VkPhysicalDeviceFeatures2 features_copy;
    const VkBaseInStructure *features_top = NULL;
    for (const VkBaseInStructure *p = ci->pNext; p; p = p->pNext)
        if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
            features_top = p; break;
        }
    /* Copy only a Features2 chain at the head; never modify application memory. */
    /* Loader link nodes can precede Features2; their chain is left untouched. */
    const char **names = NULL;
    if (active && !ext_enabled) {
        names = calloc((size_t)ci->enabledExtensionCount + 2, sizeof(*names));
        if (!names) active = 0;
        else {
            for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
                names[i] = ci->ppEnabledExtensionNames[i];
            names[ci->enabledExtensionCount] = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
            modified.ppEnabledExtensionNames = names;
            modified.enabledExtensionCount++;
            int has_renderpass2=owner->api >= VK_API_VERSION_1_2;
            for(uint32_t j=0;j<ci->enabledExtensionCount;++j)
                if(!strcmp(names[j],VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) has_renderpass2=1;
            if(!has_renderpass2) names[modified.enabledExtensionCount++]=VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
        }
    }
    if (active) {
        if (features_top && features_top == ci->pNext) {
            features_copy = *(const VkPhysicalDeviceFeatures2 *)features_top;
            shading.pNext = features_copy.pNext;
            features_copy.pNext = &shading;
            modified.pNext = &features_copy;
        } else modified.pNext = &shading;
    }
    Device *d = calloc(1, sizeof(*d));
    if (!d) { free(names); return VK_ERROR_OUT_OF_HOST_MEMORY; }
    if(pthread_mutex_init(&d->objects_lock,NULL)) { free(d); free(names); return VK_ERROR_OUT_OF_HOST_MEMORY; }
    if(pthread_mutex_init(&d->rate.lock,NULL)) {
        pthread_mutex_destroy(&d->objects_lock); free(d); free(names); return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    if(active && (!table_init(&d->pipelines) || !table_init(&d->shaders))) {
        table_clear(&d->pipelines); table_clear(&d->shaders);
        active=0; /* Tracking OOM must not prevent a normal, unmodified device. */
        TRACE("object table allocation failed; device passthrough");
    }
    TRACE("create device next");
    VkResult result = create(physical, active ? &modified : ci, allocator, out);
    TRACE("create device returned");
    free(names);
    if (result != VK_SUCCESS) {
        table_clear(&d->pipelines); table_clear(&d->shaders);
        rate_destroy(&d->rate); pthread_mutex_destroy(&d->objects_lock); free(d); return result;
    }
    d->debug=getenv("UGM_VRS_DEBUG")!=NULL;
    const char *audit=getenv("UGM_VRS_AUDIT_ONLY");
    d->audit_only=audit && !strcmp(audit,"1");
    d->caps=caps;
    const char *experimental=getenv("UGM_VRS_EXPERIMENTAL_RATE");
    if(experimental && !strcmp(experimental,"2x1")) d->experimental_rate=(VkExtent2D){2,1};
    else if(experimental && !strcmp(experimental,"1x2")) d->experimental_rate=(VkExtent2D){1,2};
    const char *msaa=getenv("UGM_VRS_EXPERIMENTAL_MSAA");
    d->experimental_msaa=msaa && !strcmp(msaa,"1");
    if(active && (d->experimental_rate.width || d->experimental_msaa))
        d->supported_rates=query_rates(physical,gipa,inst,d->rate_samples);
    if(d->debug && d->experimental_msaa) fprintf(stderr,
        "UGM VRS MSAA experimental=1 sampleCounts-1x2=%u sampleCounts-2x1=%u sampleCounts-2x2=%u\n",
        d->rate_samples[1],d->rate_samples[2],d->rate_samples[3]);
    if(d->debug && d->experimental_rate.width) fprintf(stderr,
        "UGM VRS EXPERIMENT rate=%ux%u supported=%d mask=%u fallback=1x1\n",
        d->experimental_rate.width,d->experimental_rate.height,
        !!(d->supported_rates&rate_bit(d->experimental_rate)),d->supported_rates);
    if(d->debug) fprintf(stderr,"UGM VRS policy: revision=generic-3 route=%s audit-only=%d sample-mask=%u conservative=%u sample-locations=%u gpu-effective=unmeasured\n",
        UGM_VRS_VENDOR,d->audit_only,caps.fragmentShadingRateWithSampleMask,
        caps.fragmentShadingRateWithConservativeRasterization,caps.fragmentShadingRateWithCustomSampleLocations);
    d->handle = *out;
    d->key = dispatch_key(*out);
    d->next_gdpa = gdpa;
    d->create_pipelines = (PFN_vkCreateGraphicsPipelines)gdpa(*out, "vkCreateGraphicsPipelines");
    d->create_shader = (PFN_vkCreateShaderModule)gdpa(*out, "vkCreateShaderModule");
    d->destroy_shader = (PFN_vkDestroyShaderModule)gdpa(*out, "vkDestroyShaderModule");
    int identifier_extension=0,identifier_feature=0;
    for(uint32_t j=0;j<ci->enabledExtensionCount;++j)
        if(!strcmp(ci->ppEnabledExtensionNames[j],VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME)) identifier_extension=1;
    for(const VkBaseInStructure *p=ci->pNext;p;p=p->pNext)
        if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT)
            identifier_feature=((const VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT *)p)->shaderModuleIdentifier;
    if(active && identifier_extension && identifier_feature)
        d->shader_identifier=(PFN_vkGetShaderModuleCreateInfoIdentifierEXT)gdpa(*out,"vkGetShaderModuleCreateInfoIdentifierEXT");
    d->destroy = (PFN_vkDestroyDevice)gdpa(*out, "vkDestroyDevice");
    d->destroy_pipeline = (PFN_vkDestroyPipeline)gdpa(*out, "vkDestroyPipeline");
    d->bind_pipeline = (PFN_vkCmdBindPipeline)gdpa(*out, "vkCmdBindPipeline");
    d->set_rate = (PFN_vkCmdSetFragmentShadingRateKHR)gdpa(*out, "vkCmdSetFragmentShadingRateKHR");
    d->active = active && d->create_pipelines && d->create_shader &&
                d->destroy_shader &&
                d->bind_pipeline && d->set_rate;
    pthread_mutex_lock(&registry_lock);
    d->next = devices;
    devices = d;
    atomic_fetch_add_explicit(&registry_epoch,1,memory_order_release);
    pthread_mutex_unlock(&registry_lock);
    return VK_SUCCESS;
}

static void policy_detail(Device *,const char *,const char *,uint64_t,const char *);
static void report_shader(void *context,const char *reason,uint32_t value) {
    policy_detail(context,"spirv","operand",value,reason);
}
#include "shader_identifier.h"
static unsigned analyze_shader(Device *d,const VkShaderModuleCreateInfo *info);
#include "shader_capture.h"
static unsigned analyze_shader(Device *d,const VkShaderModuleCreateInfo *info) {
    capture_shader(info);
    unsigned flags=d->debug ? shader_flags_report(info,report_shader,d) : shader_flags(info);
    if(d->shader_identifier && info && info->pCode && info->codeSize && (flags&SHADER_VALID)) {
        VkShaderModuleIdentifierEXT id={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT};
        d->shader_identifier(d->handle,info,&id);
        shader_proof_store(d,id.identifierSize,id.identifier,flags);
    }
    return flags;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
        const VkShaderModuleCreateInfo *info, const VkAllocationCallbacks *allocator, VkShaderModule *out) {
    Device *d=lookup(device); if(!d) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result=d->create_shader(device,info,allocator,out);
    if(result==VK_SUCCESS && d->active && *out) {
        Shader *s=calloc(1,sizeof(*s));
        if(s) {
            s->node.key=HANDLE_KEY(*out); s->flags=analyze_shader(d,info);
            pthread_mutex_lock(&d->objects_lock); table_insert(&d->shaders,&s->node);
            pthread_mutex_unlock(&d->objects_lock);
        }
    }
    return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule module,
        const VkAllocationCallbacks *allocator) {
    Device *d=lookup(device); if(!d) return;
    pthread_mutex_lock(&d->objects_lock); table_remove(&d->shaders,HANDLE_KEY(module));
    pthread_mutex_unlock(&d->objects_lock);
    d->destroy_shader(device,module,allocator);
}
static unsigned stage_flags(Device *d, const VkGraphicsPipelineCreateInfo *info, VkShaderStageFlagBits kind) {
    for(uint32_t i=0;info->pStages && i<info->stageCount;++i) {
        const VkPipelineShaderStageCreateInfo *s=&info->pStages[i]; if(s->stage!=kind) continue;
        if(!s->module) {
            for(const VkBaseInStructure *p=s->pNext;p;p=p->pNext) {
                if(p->sType==VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO)
                    return analyze_shader(d,(const VkShaderModuleCreateInfo *)p);
                if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT) {
                    const VkPipelineShaderStageModuleIdentifierCreateInfoEXT *id=(const void *)p;
                    unsigned flags=0;
                    int found=shader_proof_find(d,id->identifierSize,id->pIdentifier,&flags);
                    policy_detail(d,"shader-identifier","lookup",id->identifierSize,
                        found ? "observed-shader-proof-reused" : "unseen-shader-identifier");
                    return found ? flags : 0;
                }
            }
            return 0;
        }
        pthread_mutex_lock(&d->objects_lock);
        Shader *shader=(Shader *)table_find(&d->shaders,HANDLE_KEY(s->module));
        unsigned flags=shader ? shader->flags : 0;
        pthread_mutex_unlock(&d->objects_lock); return flags;
    }
    return 0;
}
static enum Exclusion legacy_pipeline_policy(Device *d,const VkGraphicsPipelineCreateInfo *info) {
    if(info->flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) return EXTENDED_STATE;
    for(const VkBaseInStructure *p=info->pNext;p;p=p->pNext) {
        if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR) return NATIVE;
        /* Linked graphics libraries, flags2, vendor coverage/rate state and
         * future unknown structures are not inferred to be safe. */
        if(p->sType!=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO &&
           p->sType!=VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO)
            return EXTENDED_STATE;
    }
    const VkPipelineDynamicStateCreateInfo *dynamic=info->pDynamicState;
    if(dynamic && dynamic->pNext) return EXTENDED_STATE;
    for(uint32_t j=0;dynamic && j<dynamic->dynamicStateCount;++j) {
        switch(dynamic->pDynamicStates[j]) {
        case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR: return NATIVE;
        case VK_DYNAMIC_STATE_VIEWPORT: case VK_DYNAMIC_STATE_SCISSOR:
        case VK_DYNAMIC_STATE_LINE_WIDTH: case VK_DYNAMIC_STATE_DEPTH_BIAS:
        case VK_DYNAMIC_STATE_BLEND_CONSTANTS: case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
        case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
        case VK_DYNAMIC_STATE_STENCIL_REFERENCE: case VK_DYNAMIC_STATE_CULL_MODE:
        case VK_DYNAMIC_STATE_FRONT_FACE: case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
        case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT: case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
        case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE: case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:
        case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE: case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:
        case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE: case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:
        case VK_DYNAMIC_STATE_STENCIL_OP: case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE:
        case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE: case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:
        case VK_DYNAMIC_STATE_VERTEX_INPUT_EXT: break;
        default: return EXTENDED_STATE; /* dynamic samples/blending/coverage etc */
        }
    }
    if(info->stageCount!=2 || !info->pStages) return STAGES;
    for(uint32_t j=0;j<info->stageCount;++j)
        if(info->pStages[j].stage!=VK_SHADER_STAGE_VERTEX_BIT && info->pStages[j].stage!=VK_SHADER_STAGE_FRAGMENT_BIT)
            return STAGES;
    unsigned fs=stage_flags(d,info,VK_SHADER_STAGE_FRAGMENT_BIT);
    unsigned vs=stage_flags(d,info,VK_SHADER_STAGE_VERTEX_BIT);
    if(!(fs&SHADER_VALID) || (fs&(SHADER_DISCARD|SHADER_FLAT|SHADER_PIXEL_INPUT|SHADER_NATIVE_RATE|SHADER_UNSUPPORTED))) return FRAGMENT;
    if(!(vs&SHADER_VALID) || !(vs&SHADER_POSITION_COARSE) || (vs&(SHADER_NATIVE_RATE|SHADER_UNSUPPORTED))) return POSITION;
    const VkPipelineMultisampleStateCreateInfo *ms=info->pMultisampleState;
    if(!ms || ms->pNext || ms->rasterizationSamples!=VK_SAMPLE_COUNT_1_BIT || ms->sampleShadingEnable || ms->alphaToCoverageEnable || ms->alphaToOneEnable) return MULTISAMPLE;
    const VkPipelineRasterizationStateCreateInfo *rs=info->pRasterizationState;
    if(!rs || rs->pNext || rs->polygonMode!=VK_POLYGON_MODE_FILL) return EXTENDED_STATE;
    const VkPipelineColorBlendStateCreateInfo *blend=info->pColorBlendState;
    if(blend && (blend->pNext || blend->logicOpEnable)) return BLENDING;
    for(uint32_t j=0;blend && blend->pAttachments && j<blend->attachmentCount;++j)
        if(blend->pAttachments[j].blendEnable) return BLENDING;
    return ACCEPT;
}

#include "compat_policy.h"
#include "diagnostics.h"
#include "gpl_policy.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache,
        uint32_t count, const VkGraphicsPipelineCreateInfo *infos,
        const VkAllocationCallbacks *allocator, VkPipeline *out) {
    Device *d = lookup(device);
    if (!d || !d->create_pipelines) return VK_ERROR_INITIALIZATION_FAILED;
    if (!d->active || !count || !infos)
        return d->create_pipelines(device, cache, count, infos, allocator, out);
    if(d->debug) atomic_fetch_add_explicit(&d->analyzed,count,memory_order_relaxed);
    /* Pure GPL batches already go unchanged to the driver. Do not allocate
     * monolithic staging arrays or take the empty pending-table lock for them.
     * Keep the original batch (including partial failures/derivatives) intact;
     * the ordinary GPL resolver still validates each returned handle. */
    uint32_t gpl_count=0;
    if(!d->audit_only) while(gpl_count<count && pipeline_is_gpl(&infos[gpl_count])) ++gpl_count;
    if(gpl_count==count) {
        VkResult result=d->create_pipelines(device,cache,count,infos,allocator,out);
        if(out) for(uint32_t i=0;i<count;++i)
            if(out[i]) gpl_finish(d,cache,&infos[i],allocator,&out[i]);
        live_report(d,0);
        return result;
    }
    VkGraphicsPipelineCreateInfo *copy = calloc(count, sizeof(*copy));
    VkPipelineDynamicStateCreateInfo *states = calloc(count, sizeof(*states));
    VkDynamicState **values = calloc(count, sizeof(*values));
    Pipeline **pending = calloc(count, sizeof(*pending));
    if (!copy || !states || !values || !pending) {
        if(d->debug) atomic_fetch_add_explicit(&d->excluded[ALLOCATION],count,memory_order_relaxed);
        free(copy); free(states); free(values); free(pending);
        return d->create_pipelines(device, cache, count, infos, allocator, out);
    }
    for (uint32_t i = 0; i < count; i++) {
        copy[i] = infos[i];
        if(pipeline_is_gpl(&infos[i]) && !d->audit_only) continue;
        enum Exclusion why=pipeline_policy(d,&infos[i]);
        if(d->audit_only) why=legacy_pipeline_policy(d,&infos[i]);
        if(why!=ACCEPT) {
            if(d->debug) {
                atomic_fetch_add_explicit(&d->excluded[why],1,memory_order_relaxed);
                if(why==FRAGMENT) {
                    unsigned flags=stage_flags(d,&infos[i],VK_SHADER_STAGE_FRAGMENT_BIT);
                    if(!(flags&SHADER_VALID)) atomic_fetch_add_explicit(&d->fragment_reasons[0],1,memory_order_relaxed);
                    for(unsigned bit=1;bit<7;++bit) if(flags&(1u<<bit))
                        atomic_fetch_add_explicit(&d->fragment_reasons[bit],1,memory_order_relaxed);
                }
            }
            if(d->debug) {
                pending[i]=calloc(1,sizeof(**pending));
                if(pending[i]) pending[i]->rejection=why;
            }
            continue;
        }
        if(d->debug) atomic_fetch_add_explicit(&d->accepted,1,memory_order_relaxed);
        const VkPipelineDynamicStateCreateInfo *old=infos[i].pDynamicState;
        uint32_t n=old ? old->dynamicStateCount : 0;
        pending[i] = calloc(1, sizeof(**pending));
        if (!pending[i]) { if(d->debug) atomic_fetch_add_explicit(&d->excluded[ALLOCATION],1,memory_order_relaxed); continue; }
        pending[i]->injected=1;
        pending[i]->samples=infos[i].pMultisampleState ? infos[i].pMultisampleState->rasterizationSamples : VK_SAMPLE_COUNT_1_BIT;
        values[i] = calloc((size_t)n + 1, sizeof(**values));
        if (!values[i]) { if(d->debug) atomic_fetch_add_explicit(&d->excluded[ALLOCATION],1,memory_order_relaxed); free(pending[i]); pending[i] = NULL; continue; }
        if (n) memcpy(values[i], old->pDynamicStates, n * sizeof(**values));
        values[i][n] = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
        states[i] = old ? *old : (VkPipelineDynamicStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        states[i].dynamicStateCount = n + 1;
        states[i].pDynamicStates = values[i];
        copy[i].pDynamicState = &states[i];
    }
    VkResult result = d->create_pipelines(device, cache, count, copy, allocator, out);
    if (out) {
        pthread_mutex_lock(&d->objects_lock);
        for (uint32_t i = 0; i < count; i++) if (pending[i] && out[i]) {
            Pipeline *p = pending[i];
            p->node.key = HANDLE_KEY(out[i]); table_insert(&d->pipelines, &p->node);
            if(d->debug && p->injected) atomic_fetch_add_explicit(&d->injected,1,memory_order_relaxed);
            pending[i] = NULL;
        }
        pthread_mutex_unlock(&d->objects_lock);
    }
    for (uint32_t i = 0; i < count; i++) {
        if(d->debug && pending[i] && pending[i]->injected) atomic_fetch_add_explicit(&d->create_failed,1,memory_order_relaxed);
        free(values[i]); free(pending[i]);
    }
    free(copy); free(states); free(values); free(pending);
    if(out && !d->audit_only) for(uint32_t i=0;i<count;++i)
        if(out[i] && pipeline_is_gpl(&infos[i]))
            gpl_finish(d,cache,&infos[i],allocator,&out[i]);
    live_report(d,0);
    return result;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
        const VkAllocationCallbacks *allocator) {
    Device *d=lookup(device); if(!d) return;
    pthread_mutex_lock(&d->objects_lock);
    Pipeline *tracked=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(pipeline));
    VkPipeline shadow=tracked ? tracked->shadow : VK_NULL_HANDLE;
    table_remove(&d->pipelines,HANDLE_KEY(pipeline));
    pthread_mutex_unlock(&d->objects_lock);
    if(shadow) d->destroy_pipeline(device,shadow,NULL);
    if(d->destroy_pipeline) d->destroy_pipeline(device,pipeline,allocator);
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer command,
        VkPipelineBindPoint point, VkPipeline pipeline) {
    Device *d=lookup(command); if(!d) return;
    d->bind_pipeline(command,point,pipeline);
    if(point!=VK_PIPELINE_BIND_POINT_GRAPHICS) return;
    if(d->debug) atomic_fetch_add_explicit(&d->graphics_binds,1,memory_order_relaxed);
    if(!d->active) { live_report(d,0); return; }
    pthread_mutex_lock(&d->objects_lock);
    Pipeline *tracked=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(pipeline));
    int ours=tracked && tracked->injected;
    VkSampleCountFlags samples=tracked ? tracked->samples : 0;
    unsigned rejection=tracked && tracked->rejection ? tracked->rejection : REASON_COUNT;
    pthread_mutex_unlock(&d->objects_lock);
    /* In debug mode also count QAM requests for rejected pipelines. A request
     * is not a recorded command, nor evidence of submitted GPU work. */
    VkExtent2D rate={1,1};
    if(ours || d->debug) rate=requested_rate(&d->rate);
    if(d->debug) atomic_fetch_add_explicit(rate.width==2 ? &d->requested_2x2 : &d->requested_1x1,1,memory_order_relaxed);
    if(ours) {
        rate=device_rate(d,rate);
        if(d->experimental_msaa && (rate.width!=1 || rate.height!=1) && samples &&
           (d->rate_samples[(rate.width-1)*2+rate.height-1]&samples)!=samples) {
            rate=(VkExtent2D){1,1};
            if(d->debug) atomic_fetch_add_explicit(&d->unsupported_rate,1,memory_order_relaxed);
        }
        if(d->debug) atomic_fetch_add_explicit(&d->binds,1,memory_order_relaxed);
        const VkFragmentShadingRateCombinerOpKHR ops[2]={VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
        d->set_rate(command,&rate,ops);
        if(d->debug) {
            atomic_ulong *counter=rate.width==2 ? (rate.height==2 ? &d->recorded_2x2 : &d->recorded_2x1) :
                (rate.height==2 ? &d->recorded_1x2 : &d->recorded_1x1);
            atomic_fetch_add_explicit(counter,1,memory_order_relaxed);
        }
    } else if(d->debug) {
        atomic_fetch_add_explicit(&d->passthrough_binds,1,memory_order_relaxed);
        atomic_fetch_add_explicit(&d->bind_excluded[rejection],1,memory_order_relaxed);
    }
    /* Non-injected pipelines have static default 1x1 (or application-owned
     * native VRS). Do not overwrite their state or leak our dynamic 2x2. */
    live_report(d,0);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,const VkAllocationCallbacks *allocator) {
    if(!device) return;
    pthread_mutex_lock(&registry_lock);
    Device **p=&devices,*d=NULL;
    while(*p) { if((*p)->handle==device) { d=*p; *p=d->next; break; } p=&(*p)->next; }
    atomic_fetch_add_explicit(&registry_epoch,1,memory_order_release);
    pthread_mutex_unlock(&registry_lock);
    if(!d) return;
    live_report(d,1);
    if(d->debug) {
        const char *reasons[REASON_COUNT]={"accepted","native","extended-state","stages","fragment-policy","position-w","multisample","blending","allocation"};
        fprintf(stderr,"UGM VRS summary: route=%s active=%d analyzed=%lu injected=%lu intervened-binds=%lu config-checks=%llu reads=%llu changes=%llu file-fallbacks=%llu (requested rate, not measured hardware shading)\n",
            UGM_VRS_VENDOR,d->active,atomic_load(&d->analyzed),atomic_load(&d->injected),atomic_load(&d->binds),
            (unsigned long long)d->rate.checks,(unsigned long long)d->rate.reads,(unsigned long long)d->rate.changes,(unsigned long long)d->rate.failures);
        const char *fragment_names[]={"invalid-or-untracked","discard","flat","pixel-sensitive","position","native-rate","unsupported-or-side-effects"};
        for(unsigned j=0;j<7;++j) if(atomic_load(&d->fragment_reasons[j]))
            fprintf(stderr,"UGM VRS fragment exclusion: %s=%lu\n",fragment_names[j],atomic_load(&d->fragment_reasons[j]));
        for(unsigned j=1;j<REASON_COUNT;++j) if(atomic_load(&d->excluded[j]))
            fprintf(stderr,"UGM VRS excluded: %s=%lu\n",reasons[j],atomic_load(&d->excluded[j]));
    }
    for(size_t j=0;d->pipelines.buckets && j<d->pipelines.capacity;++j)
        for(Node *n=d->pipelines.buckets[j];n;n=n->next) {
            Pipeline *tracked=(Pipeline *)n;
            if(tracked->shadow) d->destroy_pipeline(device,tracked->shadow,NULL);
        }
    d->destroy(device,allocator);
    table_clear(&d->pipelines); table_clear(&d->shaders);
    free(d->shader_proofs);
    rate_destroy(&d->rate); pthread_mutex_destroy(&d->objects_lock); free(d);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!strcmp(name, "vkCreateGraphicsPipelines")) return (PFN_vkVoidFunction)vkCreateGraphicsPipelines;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)vkCreateShaderModule;
    if (!strcmp(name, "vkDestroyShaderModule")) return (PFN_vkVoidFunction)vkDestroyShaderModule;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)vkDestroyDevice;
    if (!strcmp(name, "vkCmdBindPipeline")) return (PFN_vkVoidFunction)vkCmdBindPipeline;
    if (!strcmp(name, "vkDestroyPipeline")) return (PFN_vkVoidFunction)vkDestroyPipeline;
    Device *d = lookup(device);
    PFN_vkGetDeviceProcAddr gdpa = d ? d->next_gdpa : NULL;
    return gdpa ? gdpa(device, name) : NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!strcmp(name, "vkDestroyInstance")) return (PFN_vkVoidFunction)vkDestroyInstance;
    if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)vkCreateInstance;
    if (!strcmp(name, "vkCreateDevice")) return (PFN_vkVoidFunction)vkCreateDevice;
    if (!strcmp(name, "vkCreateGraphicsPipelines")) return (PFN_vkVoidFunction)vkCreateGraphicsPipelines;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)vkCreateShaderModule;
    if (!strcmp(name, "vkDestroyShaderModule")) return (PFN_vkVoidFunction)vkDestroyShaderModule;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)vkDestroyDevice;
    if (!strcmp(name, "vkCmdBindPipeline")) return (PFN_vkVoidFunction)vkCmdBindPipeline;
    if (!strcmp(name, "vkDestroyPipeline")) return (PFN_vkVoidFunction)vkDestroyPipeline;
    Instance *i=instance_lookup(inst);
    return i ? i->gipa(inst,name) : NULL;
}

static PFN_vkVoidFunction VKAPI_CALL get_physical_proc(VkInstance inst,const char *name) {
    Instance *i=instance_lookup(inst);
    return i && i->gpdpa ? i->gpdpa(inst,name) : (i ? i->gipa(inst,name) : NULL);
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
        VkNegotiateLayerInterface *iface) {
    if (!iface || iface->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (iface->loaderLayerInterfaceVersion > 2) iface->loaderLayerInterfaceVersion = 2;
    iface->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    iface->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    iface->pfnGetPhysicalDeviceProcAddr = get_physical_proc;
    return VK_SUCCESS;
}
