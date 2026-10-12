/* UGM experimental Vulkan layer: per-game RADV pipeline VRS. */
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct Device {
    VkDevice handle;
    PFN_vkGetDeviceProcAddr next_gdpa;
    PFN_vkCreateGraphicsPipelines create_pipelines;
    PFN_vkCreateShaderModule create_shader;
    PFN_vkDestroyShaderModule destroy_shader;
    PFN_vkDestroyDevice destroy;
    PFN_vkDestroyPipeline destroy_pipeline;
    PFN_vkAllocateCommandBuffers allocate_commands;
    PFN_vkFreeCommandBuffers free_commands;
    PFN_vkDestroyCommandPool destroy_pool;
    PFN_vkCmdBindPipeline bind_pipeline;
    PFN_vkCmdSetFragmentShadingRateKHR set_rate;
    int active;
    struct Device *next;
} Device;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static PFN_vkGetInstanceProcAddr next_gipa;
static VkInstance instance;
static Device *devices;
typedef struct Pipeline { VkPipeline handle; Device *device; struct Pipeline *next; } Pipeline;
typedef struct Shader { VkShaderModule handle; Device *device; unsigned flags; struct Shader *next; } Shader;
typedef struct Command { VkCommandBuffer handle; VkCommandPool pool; Device *device; struct Command *next; } Command;
static Pipeline *pipelines;
static Shader *shaders;
#include "spirv_policy.h"
static Command *commands;
#define TRACE(msg) do { if (getenv("UGM_VRS_DEBUG")) fprintf(stderr, "UGM VRS: %s\n", msg); } while (0)

static Device *lookup(VkDevice handle) {
    for (Device *d = devices; d; d = d->next) if (d->handle == handle) return d;
    return NULL;
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
    TRACE("create instance enter");
    VkLayerInstanceCreateInfo *link = instance_link(ci);
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    TRACE("create instance next");
    VkResult result = create(ci, allocator, out);
    TRACE("create instance returned");
    if (result == VK_SUCCESS) {
        pthread_mutex_lock(&mutex);
        next_gipa = gipa;
        instance = *out;
        pthread_mutex_unlock(&mutex);
    }
    return result;
}

static int eligible(VkPhysicalDevice physical, PFN_vkGetInstanceProcAddr gipa,
                    VkInstance inst, const VkDeviceCreateInfo *ci, int *ext_enabled) {
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
    if (props.properties.vendorID != 0x1002 ||
        driver.driverID != VK_DRIVER_ID_MESA_RADV) {
        TRACE("not RADV"); return 0;
    }
    uint32_t count = 0;
    if (extensions(physical, NULL, &count, NULL) != VK_SUCCESS || count == 0 || count > 4096)
        return 0;
    VkExtensionProperties *exts = calloc(count, sizeof(*exts));
    if (!exts) return 0;
    int available = 0;
    if (extensions(physical, NULL, &count, exts) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; i++)
            if (!strcmp(exts[i].extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
                available = 1;
    free(exts);
    if (!available) { TRACE("extension unavailable"); return 0; }
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
    TRACE("eligible RADV 2x2");
    return 1;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical,
        const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *allocator, VkDevice *out) {
    TRACE("create device enter");
    VkLayerDeviceCreateInfo *link = device_link(ci);
    if (!link || !link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    pthread_mutex_lock(&mutex);
    VkInstance inst = instance;
    pthread_mutex_unlock(&mutex);
    PFN_vkCreateDevice create = (PFN_vkCreateDevice)gipa(inst, "vkCreateDevice");
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    int ext_enabled = 0;
    TRACE("eligible enter");
    int active = inst && eligible(physical, gipa, inst, ci, &ext_enabled);
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
        names = calloc(ci->enabledExtensionCount + 1, sizeof(*names));
        if (!names) active = 0;
        else {
            for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
                names[i] = ci->ppEnabledExtensionNames[i];
            names[ci->enabledExtensionCount] = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
            modified.ppEnabledExtensionNames = names;
            modified.enabledExtensionCount++;
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
    TRACE("create device next");
    VkResult result = create(physical, active ? &modified : ci, allocator, out);
    TRACE("create device returned");
    free(names);
    if (result != VK_SUCCESS) { free(d); return result; }
    d->handle = *out;
    d->next_gdpa = gdpa;
    d->create_pipelines = (PFN_vkCreateGraphicsPipelines)gdpa(*out, "vkCreateGraphicsPipelines");
    d->create_shader = (PFN_vkCreateShaderModule)gdpa(*out, "vkCreateShaderModule");
    d->destroy_shader = (PFN_vkDestroyShaderModule)gdpa(*out, "vkDestroyShaderModule");
    d->destroy = (PFN_vkDestroyDevice)gdpa(*out, "vkDestroyDevice");
    d->destroy_pipeline = (PFN_vkDestroyPipeline)gdpa(*out, "vkDestroyPipeline");
    d->allocate_commands = (PFN_vkAllocateCommandBuffers)gdpa(*out, "vkAllocateCommandBuffers");
    d->free_commands = (PFN_vkFreeCommandBuffers)gdpa(*out, "vkFreeCommandBuffers");
    d->destroy_pool = (PFN_vkDestroyCommandPool)gdpa(*out, "vkDestroyCommandPool");
    d->bind_pipeline = (PFN_vkCmdBindPipeline)gdpa(*out, "vkCmdBindPipeline");
    d->set_rate = (PFN_vkCmdSetFragmentShadingRateKHR)gdpa(*out, "vkCmdSetFragmentShadingRateKHR");
    d->active = active && d->create_pipelines && d->create_shader &&
                d->destroy_shader && d->allocate_commands &&
                d->bind_pipeline && d->set_rate;
    pthread_mutex_lock(&mutex);
    d->next = devices;
    devices = d;
    pthread_mutex_unlock(&mutex);
    return VK_SUCCESS;
}

static VkExtent2D parse_rate(const char *value) {
    VkExtent2D full = {1, 1};
    if (!value) return full;
    if (!strncmp(value, "2x2", 3) && (value[3] == '\0' || value[3] == '\n'))
        return (VkExtent2D){2, 2};
    return full;
}

static VkExtent2D requested_rate(void) {
    const char *path = getenv("UGM_VRS_CONFIG_FILE");
    if (path && *path) {
        char value[8] = {0};
        FILE *file = fopen(path, "r");
        if (!file) return (VkExtent2D){1, 1};
        size_t n = fread(value, 1, sizeof(value) - 1, file);
        fclose(file);
        return n >= 3 ? parse_rate(value) : (VkExtent2D){1, 1};
    }
    return parse_rate(getenv("UGM_VRS_RATE"));
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
        const VkShaderModuleCreateInfo *info, const VkAllocationCallbacks *allocator,
        VkShaderModule *out) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    pthread_mutex_unlock(&mutex);
    if (!d || !d->create_shader) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = d->create_shader(device, info, allocator, out);
    if (result == VK_SUCCESS && d->active && out && *out) {
        Shader *shader = calloc(1, sizeof(*shader));
        if (shader) {
            shader->handle = *out;
            shader->device = d;
            shader->flags = shader_flags(info);
            pthread_mutex_lock(&mutex);
            shader->next = shaders;
            shaders = shader;
            pthread_mutex_unlock(&mutex);
        }
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule module,
        const VkAllocationCallbacks *allocator) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    Shader **cursor = &shaders;
    while (*cursor) {
        if ((*cursor)->handle == module && (*cursor)->device == d) {
            Shader *old = *cursor; *cursor = old->next; free(old); break;
        }
        cursor = &(*cursor)->next;
    }
    pthread_mutex_unlock(&mutex);
    if (d && d->destroy_shader) d->destroy_shader(device, module, allocator);
}

static unsigned stage_flags(Device *d, const VkGraphicsPipelineCreateInfo *info,
                            VkShaderStageFlagBits kind) {
    if (!info->pStages) return 0;
    for (uint32_t i = 0; i < info->stageCount; i++) {
        const VkPipelineShaderStageCreateInfo *stage = &info->pStages[i];
        if (stage->stage != kind) continue;
        if (stage->module == VK_NULL_HANDLE) {
            for (const VkBaseInStructure *p = stage->pNext; p; p = p->pNext)
                if (p->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO)
                    return shader_flags((const VkShaderModuleCreateInfo *)p);
            return 0;
        }
        unsigned flags = 0;
        pthread_mutex_lock(&mutex);
        for (Shader *shader = shaders; shader; shader = shader->next)
            if (shader->handle == stage->module && shader->device == d) {
                flags = shader->flags; break;
            }
        pthread_mutex_unlock(&mutex);
        return flags;
    }
    return 0;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache,
        uint32_t count, const VkGraphicsPipelineCreateInfo *infos,
        const VkAllocationCallbacks *allocator, VkPipeline *out) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    pthread_mutex_unlock(&mutex);
    if (!d || !d->create_pipelines) return VK_ERROR_INITIALIZATION_FAILED;
    if (!d->active || !count || !infos)
        return d->create_pipelines(device, cache, count, infos, allocator, out);
    VkGraphicsPipelineCreateInfo *copy = calloc(count, sizeof(*copy));
    VkPipelineDynamicStateCreateInfo *states = calloc(count, sizeof(*states));
    VkDynamicState **values = calloc(count, sizeof(*values));
    Pipeline **pending = calloc(count, sizeof(*pending));
    if (!copy || !states || !values || !pending) {
        free(copy); free(states); free(values); free(pending);
        return d->create_pipelines(device, cache, count, infos, allocator, out);
    }
    for (uint32_t i = 0; i < count; i++) {
        copy[i] = infos[i];
        if (infos[i].flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) continue;
        int app_vrs = 0;
        for (const VkBaseInStructure *p = infos[i].pNext; p; p = p->pNext)
            if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR)
                app_vrs = 1;
        if (app_vrs) continue;
        const VkPipelineDynamicStateCreateInfo *old = infos[i].pDynamicState;
        uint32_t n = old ? old->dynamicStateCount : 0;
        for (uint32_t j = 0; j < n; j++)
            if (old->pDynamicStates[j] == VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR)
                app_vrs = 1;
        if (app_vrs) continue;
        /* RADV-like shader selection: protect pixel-coordinate postprocessing
         * and Position.w == 1 UI/fullscreen passes. Unknown IR stays untouched.
         * Only a complete vertex+fragment pipeline is understood by this layer. */
        int unsupported_stage = infos[i].stageCount != 2 || !infos[i].pStages;
        for (uint32_t j = 0; infos[i].pStages && j < infos[i].stageCount; j++)
            if (infos[i].pStages[j].stage != VK_SHADER_STAGE_VERTEX_BIT &&
                infos[i].pStages[j].stage != VK_SHADER_STAGE_FRAGMENT_BIT)
                unsupported_stage = 1;
        if (unsupported_stage) continue;
        unsigned flags = stage_flags(d, &infos[i], VK_SHADER_STAGE_FRAGMENT_BIT);
        unsigned vertex = stage_flags(d, &infos[i], VK_SHADER_STAGE_VERTEX_BIT);
        if (!(flags & SHADER_VALID) ||
            (flags & (SHADER_DISCARD | SHADER_FLAT | SHADER_PIXEL_INPUT |
                      SHADER_NATIVE_RATE | SHADER_UNSUPPORTED))) {
            TRACE("preserve fragment shader (pixel input/discard/native/unknown)");
            continue;
        }
        if (!(vertex & SHADER_VALID) || !(vertex & SHADER_POSITION_COARSE) ||
            (vertex & (SHADER_NATIVE_RATE | SHADER_UNSUPPORTED))) {
            TRACE("preserve vertex shader (Position.w=1 or unknown)");
            continue;
        }
        const VkPipelineMultisampleStateCreateInfo *ms = infos[i].pMultisampleState;
        if (!ms || ms->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT ||
            ms->sampleShadingEnable || ms->alphaToCoverageEnable || ms->alphaToOneEnable)
            continue;
        /* Preserve alpha-blended UI and transparencies, which are especially
         * visibly degraded by forcing one shading result across 2x2 pixels. */
        const VkPipelineColorBlendStateCreateInfo *blend = infos[i].pColorBlendState;
        int blended = 0;
        if (blend && blend->pAttachments)
            for (uint32_t j = 0; j < blend->attachmentCount; j++)
                if (blend->pAttachments[j].blendEnable) { blended = 1; break; }
        if (blended) continue;
        pending[i] = calloc(1, sizeof(**pending));
        if (!pending[i]) continue;
        values[i] = calloc(n + 1, sizeof(**values));
        if (!values[i]) { free(pending[i]); pending[i] = NULL; continue; }
        if (n) memcpy(values[i], old->pDynamicStates, n * sizeof(**values));
        values[i][n] = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
        states[i] = old ? *old : (VkPipelineDynamicStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        states[i].dynamicStateCount = n + 1;
        states[i].pDynamicStates = values[i];
        copy[i].pDynamicState = &states[i];
        TRACE("pipeline dynamic VRS added");
    }
    VkResult result = d->create_pipelines(device, cache, count, copy, allocator, out);
    if (out) {
        pthread_mutex_lock(&mutex);
        for (uint32_t i = 0; i < count; i++) if (pending[i] && out[i]) {
            Pipeline *p = pending[i];
            p->handle = out[i]; p->device = d; p->next = pipelines; pipelines = p;
            pending[i] = NULL;
        }
        pthread_mutex_unlock(&mutex);
    }
    for (uint32_t i = 0; i < count; i++) { free(values[i]); free(pending[i]); }
    free(copy); free(states); free(values); free(pending);
    return result;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
        const VkAllocationCallbacks *allocator) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    Pipeline **cursor = &pipelines;
    while (*cursor) {
        if ((*cursor)->handle == pipeline && (*cursor)->device == d) {
            Pipeline *old = *cursor; *cursor = old->next; free(old); break;
        }
        cursor = &(*cursor)->next;
    }
    pthread_mutex_unlock(&mutex);
    if (d && d->destroy_pipeline) d->destroy_pipeline(device, pipeline, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device,
        const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    pthread_mutex_unlock(&mutex);
    if (!d || !d->allocate_commands) return VK_ERROR_INITIALIZATION_FAILED;
    /* Track ALL devices, including ones with native/app VRS or an unsupported
     * driver. Otherwise the bind interception would silently swallow their
     * vkCmdBindPipeline calls. Allocate tracking before calling the driver. */
    Command *pending = NULL;
    for (uint32_t i = 0; i < info->commandBufferCount; i++) {
        Command *c = calloc(1, sizeof(*c));
        if (!c) {
            while (pending) { c = pending; pending = c->next; free(c); }
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        c->next = pending; pending = c;
    }
    VkResult result = d->allocate_commands(device, info, out);
    pthread_mutex_lock(&mutex);
    for (uint32_t i = 0; pending; i++) {
        Command *c = pending; pending = c->next;
        if (result == VK_SUCCESS) {
            c->handle = out[i]; c->pool = info->commandPool; c->device = d;
            c->next = commands; commands = c;
        } else free(c);
    }
    pthread_mutex_unlock(&mutex);
    return result;
}

VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool pool,
        uint32_t count, const VkCommandBuffer *buffers) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    for (uint32_t i = 0; i < count; i++) {
        Command **cursor = &commands;
        while (*cursor) {
            if ((*cursor)->handle == buffers[i] && (*cursor)->device == d) {
                Command *old = *cursor; *cursor = old->next; free(old); break;
            }
            cursor = &(*cursor)->next;
        }
    }
    pthread_mutex_unlock(&mutex);
    if (d && d->free_commands) d->free_commands(device, pool, count, buffers);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool pool,
        const VkAllocationCallbacks *allocator) {
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    Command **cursor = &commands;
    while (*cursor) {
        if ((*cursor)->pool == pool && (*cursor)->device == d) {
            Command *old = *cursor; *cursor = old->next; free(old);
        } else cursor = &(*cursor)->next;
    }
    pthread_mutex_unlock(&mutex);
    if (d && d->destroy_pool) d->destroy_pool(device, pool, allocator);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer command,
        VkPipelineBindPoint point, VkPipeline pipeline) {
    pthread_mutex_lock(&mutex);
    Device *d = NULL;
    int ours = 0;
    for (Command *c = commands; c; c = c->next)
        if (c->handle == command) { d = c->device; break; }
    if (d) for (Pipeline *p = pipelines; p; p = p->next)
        if (p->handle == pipeline && p->device == d) { ours = 1; break; }
    pthread_mutex_unlock(&mutex);
    if (!d || !d->bind_pipeline) return;
    d->bind_pipeline(command, point, pipeline);
    if (point == VK_PIPELINE_BIND_POINT_GRAPHICS && ours && d->set_rate) {
        VkExtent2D rate = requested_rate();
        VkFragmentShadingRateCombinerOpKHR ops[2] = {
            VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
            VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
        d->set_rate(command, &rate, ops);
        TRACE(rate.height == 2 ? "bind VRS 2x2" : "bind VRS 1x1");
    }
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *allocator) {
    pthread_mutex_lock(&mutex);
    Device **cursor = &devices, *d = NULL;
    while (*cursor) {
        if ((*cursor)->handle == device) { d = *cursor; *cursor = d->next; break; }
        cursor = &(*cursor)->next;
    }
    Pipeline **p = &pipelines;
    while (*p) { if ((*p)->device == d) { Pipeline *old = *p; *p = old->next; free(old); }
                 else p = &(*p)->next; }
    Command **c = &commands;
    while (*c) { if ((*c)->device == d) { Command *old = *c; *c = old->next; free(old); }
                else c = &(*c)->next; }
    Shader **s = &shaders;
    while (*s) { if ((*s)->device == d) { Shader *old = *s; *s = old->next; free(old); }
                else s = &(*s)->next; }
    pthread_mutex_unlock(&mutex);
    if (d) { if (d->destroy) d->destroy(device, allocator); free(d); }
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!strcmp(name, "vkCreateGraphicsPipelines")) return (PFN_vkVoidFunction)vkCreateGraphicsPipelines;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)vkCreateShaderModule;
    if (!strcmp(name, "vkDestroyShaderModule")) return (PFN_vkVoidFunction)vkDestroyShaderModule;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)vkDestroyDevice;
    if (!strcmp(name, "vkCmdBindPipeline")) return (PFN_vkVoidFunction)vkCmdBindPipeline;
    if (!strcmp(name, "vkDestroyCommandPool")) return (PFN_vkVoidFunction)vkDestroyCommandPool;
    if (!strcmp(name, "vkFreeCommandBuffers")) return (PFN_vkVoidFunction)vkFreeCommandBuffers;
    if (!strcmp(name, "vkAllocateCommandBuffers")) return (PFN_vkVoidFunction)vkAllocateCommandBuffers;
    if (!strcmp(name, "vkDestroyPipeline")) return (PFN_vkVoidFunction)vkDestroyPipeline;
    pthread_mutex_lock(&mutex);
    Device *d = lookup(device);
    PFN_vkGetDeviceProcAddr gdpa = d ? d->next_gdpa : NULL;
    pthread_mutex_unlock(&mutex);
    return gdpa ? gdpa(device, name) : NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst, const char *name) {
    if (!name) return NULL;
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)vkCreateInstance;
    if (!strcmp(name, "vkCreateDevice")) return (PFN_vkVoidFunction)vkCreateDevice;
    if (!strcmp(name, "vkCreateGraphicsPipelines")) return (PFN_vkVoidFunction)vkCreateGraphicsPipelines;
    if (!strcmp(name, "vkCreateShaderModule")) return (PFN_vkVoidFunction)vkCreateShaderModule;
    if (!strcmp(name, "vkDestroyShaderModule")) return (PFN_vkVoidFunction)vkDestroyShaderModule;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)vkDestroyDevice;
    if (!strcmp(name, "vkCmdBindPipeline")) return (PFN_vkVoidFunction)vkCmdBindPipeline;
    if (!strcmp(name, "vkDestroyCommandPool")) return (PFN_vkVoidFunction)vkDestroyCommandPool;
    if (!strcmp(name, "vkFreeCommandBuffers")) return (PFN_vkVoidFunction)vkFreeCommandBuffers;
    if (!strcmp(name, "vkAllocateCommandBuffers")) return (PFN_vkVoidFunction)vkAllocateCommandBuffers;
    if (!strcmp(name, "vkDestroyPipeline")) return (PFN_vkVoidFunction)vkDestroyPipeline;
    return next_gipa ? next_gipa(inst, name) : NULL;
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
        VkNegotiateLayerInterface *iface) {
    if (!iface || iface->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (iface->loaderLayerInterfaceVersion > 2) iface->loaderLayerInterfaceVersion = 2;
    iface->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    iface->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    iface->pfnGetPhysicalDeviceProcAddr = NULL;
    return VK_SUCCESS;
}
