/* Generic compatibility rules, shared by NVIDIA and RADV. No game identity.
 * All application chains are read-only and forwarded intact. Unknown state
 * fails closed; the diagnostic key always includes its site and numeric value. */
#ifndef UGM_VRS_COMPAT_POLICY_H
#define UGM_VRS_COMPAT_POLICY_H
static void policy_detail(Device *d,const char *site,const char *name,uint64_t value,const char *rule) {
    if(!d->debug) return;
    pthread_mutex_lock(&d->objects_lock);
    unsigned j=0;
    for(;j<d->detail_count;++j) {
        PolicyDetail *e=&d->details[j];
        if(e->value==value && !strcmp(e->site,site) && !strcmp(e->rule,rule) && !strcmp(e->name,name)) break;
    }
    if(j==d->detail_count && j<VRS_DETAIL_LIMIT) {
        d->details[j]=(PolicyDetail){.site=site,.name=name,.rule=rule,.value=value};
        ++d->detail_count;
    }
    if(j<VRS_DETAIL_LIMIT) ++d->details[j].count; else ++d->detail_overflow;
    pthread_mutex_unlock(&d->objects_lock);
}
enum PolicySite { PS_PIPELINE, PS_DYNAMIC, PS_RASTER, PS_MULTISAMPLE, PS_BLEND,
    PS_VIEWPORT, PS_VERTEX, PS_ASSEMBLY, PS_DEPTH, PS_TESS, PS_STAGE, PS_MODULE };
static const char *const policy_sites[]={"pipeline.pNext","dynamic.pNext","raster.pNext",
    "multisample.pNext","blend.pNext","viewport.pNext","vertex-input.pNext",
    "assembly.pNext","depth-stencil.pNext","tessellation.pNext","stage.pNext","module.pNext"};
static enum Exclusion flags_policy(Device *d,uint64_t flags,int record) {
    const uint64_t native=VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
        VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
    /* Compilation/caching and descriptor binding options do not change our
     * raster/shader proof. GPL flags are resolved separately; future flags fail closed. */
    const uint64_t allowed=VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT |
        VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT | VK_PIPELINE_CREATE_DERIVATIVE_BIT |
        VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
        VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT |
        VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
        VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR |
        VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
        VK_PIPELINE_CREATE_LIBRARY_BIT_KHR |
        VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT |
        VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT
#ifdef VK_EXT_descriptor_heap
        | VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT
#endif
        ;
    enum Exclusion why=flags&native ? NATIVE : (flags&~allowed ? EXTENDED_STATE : ACCEPT);
    if(flags && record) policy_detail(d,"pipeline.flags","flags",flags,
        why==NATIVE ? "native-rate-attachment" : why==ACCEPT ? "preserved-compile-or-binding-flags" :
        flags&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR ? "graphics-library-unresolved" : "unmodeled-flags");
    return why;
}
static enum Exclusion chain_policy(Device *d,const void *chain,enum PolicySite site) {
    enum Exclusion result=ACCEPT;
    unsigned count=0;
    for(const VkBaseInStructure *p=chain;p;p=p->pNext) {
        if(++count>64) {
            policy_detail(d,policy_sites[site],"chain",count,"chain-limit");
            return EXTENDED_STATE;
        }
        enum Exclusion why=EXTENDED_STATE;
        const char *name="unknown-sType", *rule="unknown-or-wrong-site";
#define CASE(type,where) case type: name=#type; if(site!=(where)) break
#define ALLOW(reason) do { why=ACCEPT; rule=(reason); } while(0)
        switch(p->sType) {
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,PS_PIPELINE);
            ALLOW("preserved-rendering-formats"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,PS_PIPELINE);
            ALLOW("preserved-feedback"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,PS_PIPELINE);
            why=NATIVE; rule="application-native-vrs"; break;
        CASE(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,PS_PIPELINE);
            ALLOW("graphics-library-subsets-resolved-separately"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,PS_PIPELINE);
            ALLOW("linked-library-proofs-required"); break;
#ifdef VK_KHR_maintenance5
        CASE(VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,PS_PIPELINE);
            why=flags_policy(d,((const VkPipelineCreateFlags2CreateInfoKHR *)p)->flags,0);
            rule=why==ACCEPT ? "preserved-flags2" : "flags2-conflict"; break;
#endif
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,PS_RASTER);
            ALLOW("preserved-depth-clip"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,PS_RASTER);
            /* Flat fragment interpolation is excluded by the shader policy. */
            ALLOW("preserved-provoking-vertex"); break;
        CASE(VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT,PS_RASTER);
            ALLOW("preserved-depth-bias"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,PS_RASTER);
            if(!((const VkPipelineRasterizationStateStreamCreateInfoEXT *)p)->rasterizationStream)
                ALLOW("default-rasterization-stream");
            else rule="nonzero-rasterization-stream";
            break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,PS_RASTER); {
            const VkPipelineRasterizationLineStateCreateInfoEXT *line=(const void *)p;
            if(line->lineRasterizationMode==VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT && !line->stippledLineEnable)
                ALLOW("default-line-rasterization");
            else rule="nondefault-line-rasterization";
            break;
        }
        CASE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,PS_RASTER); {
            const VkPipelineRasterizationConservativeStateCreateInfoEXT *c=(const void *)p;
            if(c->conservativeRasterizationMode==VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT)
                ALLOW("conservative-disabled");
            else if(d->caps.fragmentShadingRateWithConservativeRasterization)
                ALLOW("device-supports-conservative-vrs");
            else rule="device-no-conservative-vrs";
            break;
        }
        CASE(VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,PS_MULTISAMPLE); {
            const VkPipelineSampleLocationsStateCreateInfoEXT *s=(const void *)p;
            if(!s->sampleLocationsEnable) ALLOW("custom-sample-locations-disabled");
            else if(d->caps.fragmentShadingRateWithCustomSampleLocations)
                ALLOW("device-supports-custom-samples-vrs");
            else { why=MULTISAMPLE; rule="device-no-custom-samples-vrs"; }
            break;
        }
        CASE(VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV,PS_MULTISAMPLE);
            rule="vendor-coverage-to-color-unmodeled"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV,PS_MULTISAMPLE);
            rule="vendor-coverage-modulation-unmodeled"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV,PS_MULTISAMPLE);
            rule="vendor-coverage-reduction-unmodeled"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV,PS_VIEWPORT);
            why=NATIVE; rule="application-native-rate-image"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV,PS_VIEWPORT);
            why=NATIVE; rule="application-native-coarse-sample-order"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV,PS_PIPELINE);
            why=NATIVE; rule="application-native-rate-enum"; break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,PS_BLEND);
            ALLOW("preserved-color-write-enable"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,PS_VIEWPORT);
            ALLOW("preserved-depth-clip-control"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,PS_VERTEX);
            ALLOW("preserved-instance-divisors"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,PS_STAGE);
            ALLOW("preserved-subgroup-size"); break;
        CASE(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT,PS_STAGE);
            ALLOW("shader-module-must-still-be-analyzable"); break;
        CASE(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,PS_STAGE); {
            const VkShaderModuleCreateInfo *m=(const void *)p;
            why=chain_policy(d,m->pNext,PS_MODULE); rule="inline-shader-analyzed-separately"; break;
        }
#ifdef VK_EXT_descriptor_heap
        case VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT:
            name="VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT";
            if(site==PS_STAGE || site==PS_MODULE) ALLOW("preserved-descriptor-heap-resource-mapping");
            break;
#endif
        CASE(VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT,PS_MODULE);
            ALLOW("preserved-validation-cache"); break;
        default: break;
        }
#undef CASE
#undef ALLOW
        policy_detail(d,policy_sites[site],name,(uint32_t)p->sType,rule);
        if(result==ACCEPT && why!=ACCEPT) result=why;
    }
    return result;
}
static enum Exclusion dynamic_policy(Device *d,const VkPipelineDynamicStateCreateInfo *state) {
    enum Exclusion result=ACCEPT;
    for(uint32_t j=0;state && j<state->dynamicStateCount;++j) {
        VkDynamicState value=state->pDynamicStates[j];
        enum Exclusion why=ACCEPT;
        const char *name="unknown-dynamic-state",*rule="preserved-independent-state";
#define SAFE(x) case x: name=#x; break
        switch(value) {
        SAFE(VK_DYNAMIC_STATE_VIEWPORT); SAFE(VK_DYNAMIC_STATE_SCISSOR);
        SAFE(VK_DYNAMIC_STATE_LINE_WIDTH); SAFE(VK_DYNAMIC_STATE_DEPTH_BIAS);
        SAFE(VK_DYNAMIC_STATE_BLEND_CONSTANTS); SAFE(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        SAFE(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK); SAFE(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        SAFE(VK_DYNAMIC_STATE_STENCIL_REFERENCE); SAFE(VK_DYNAMIC_STATE_CULL_MODE);
        SAFE(VK_DYNAMIC_STATE_FRONT_FACE); SAFE(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
        SAFE(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT); SAFE(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
        SAFE(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE); SAFE(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        SAFE(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE); SAFE(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
        SAFE(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE); SAFE(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        SAFE(VK_DYNAMIC_STATE_STENCIL_OP); SAFE(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE);
        SAFE(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE); SAFE(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
        SAFE(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT); SAFE(VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT);
        SAFE(VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT); SAFE(VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT);
        SAFE(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT); SAFE(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
        /* Enable bits are still checked statically, and dynamic enable bits
         * remain blocked: changing unused equations cannot activate blending. */
        SAFE(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT); SAFE(VK_DYNAMIC_STATE_LOGIC_OP_EXT);
        case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:
            name="VK_DYNAMIC_STATE_SAMPLE_MASK_EXT";
            if(!d->caps.fragmentShadingRateWithSampleMask) { why=EXTENDED_STATE; rule="device-no-dynamic-sample-mask-vrs"; }
            else rule="device-supports-sample-mask-vrs";
            break;
        case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
            name="VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR"; why=NATIVE; rule="application-native-vrs"; break;
#define BLOCK(x,reason) case x: name=#x; why=EXTENDED_STATE; rule=reason; break
        BLOCK(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,"dynamic-blend-enable-untracked");
        BLOCK(VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,"dynamic-logic-enable-untracked");
        BLOCK(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,"dynamic-sample-count-untracked");
        BLOCK(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,"dynamic-alpha-coverage-untracked");
        BLOCK(VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,"dynamic-alpha-one-untracked");
        BLOCK(VK_DYNAMIC_STATE_POLYGON_MODE_EXT,"dynamic-polygon-mode-untracked");
        BLOCK(VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT,"dynamic-conservative-mode-untracked");
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT:
            name=value==VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT ? "VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT" : "VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT";
            if(d->caps.fragmentShadingRateWithCustomSampleLocations)
                rule="device-supports-dynamic-custom-samples-vrs";
            else { why=EXTENDED_STATE; rule="device-no-dynamic-custom-samples-vrs"; }
            break;
        BLOCK(VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT,"dynamic-line-mode-untracked");
        BLOCK(VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT,"dynamic-line-stipple-enable-untracked");
        default: why=EXTENDED_STATE; rule="unknown-dynamic-state"; break;
        }
#undef BLOCK
#undef SAFE
        policy_detail(d,"dynamic-state",name,(uint32_t)value,rule);
        if(result==ACCEPT && why!=ACCEPT) result=why;
    }
    return result;
}
/* GPL subset ownership follows Vulkan pipelines-graphics-subsets. Never read
 * ignored members, even when they contain non-null garbage. */
#define GPL_VI VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT
#define GPL_PRE VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT
#define GPL_FS VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT
#define GPL_OUT VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT
#define GPL_ALL (GPL_VI|GPL_PRE|GPL_FS|GPL_OUT)
static uint64_t pipeline_flags(const VkGraphicsPipelineCreateInfo *info) {
    uint64_t flags=info->flags;
#ifdef VK_KHR_maintenance5
    unsigned nodes=0;
    for(const VkBaseInStructure *p=info->pNext;p && nodes++<64;p=p->pNext)
        if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR)
            flags=((const VkPipelineCreateFlags2CreateInfoKHR *)p)->flags;
#endif
    return flags;
}
static int pipeline_is_gpl(const VkGraphicsPipelineCreateInfo *info) {
    if(pipeline_flags(info)&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) return 1;
    unsigned nodes=0;
    for(const VkBaseInStructure *p=info->pNext;p && nodes++<64;p=p->pNext)
        if(p->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT ||
           p->sType==VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR) return 1;
    return 0;
}
static enum Exclusion subset_policy(Device *d,const VkGraphicsPipelineCreateInfo *info,unsigned parts) {
    enum Exclusion result=ACCEPT,why;
#define MERGE(x) do { why=(x); if(result==ACCEPT && why!=ACCEPT) result=why; } while(0)
#define CHAIN(field,site) do { if(info->field) MERGE(chain_policy(d,info->field->pNext,site)); } while(0)
    if(parts) { CHAIN(pDynamicState,PS_DYNAMIC); }
    if(parts&GPL_PRE) { CHAIN(pRasterizationState,PS_RASTER); CHAIN(pViewportState,PS_VIEWPORT); CHAIN(pTessellationState,PS_TESS); }
    if(parts&(GPL_FS|GPL_OUT)) { CHAIN(pMultisampleState,PS_MULTISAMPLE); }
    if(parts&GPL_OUT) { CHAIN(pColorBlendState,PS_BLEND); }
    if(parts&GPL_VI) { CHAIN(pVertexInputState,PS_VERTEX); CHAIN(pInputAssemblyState,PS_ASSEMBLY); }
    if(parts&GPL_FS) { CHAIN(pDepthStencilState,PS_DEPTH); }
#undef CHAIN
    if(parts&(GPL_PRE|GPL_FS))
        for(uint32_t j=0;info->pStages && j<info->stageCount;++j)
            MERGE(chain_policy(d,info->pStages[j].pNext,PS_STAGE));
    if(parts) MERGE(dynamic_policy(d,info->pDynamicState));
    if(result!=ACCEPT) return result;
#undef MERGE
#define REJECT(code,site,name,value,rule) do { policy_detail(d,site,name,(uint64_t)(value),rule); return code; } while(0)
    unsigned vertex=0,fragment=0;
    if(parts&(GPL_PRE|GPL_FS)) for(uint32_t j=0;info->pStages && j<info->stageCount;++j) {
        vertex+=info->pStages[j].stage==VK_SHADER_STAGE_VERTEX_BIT;
        fragment+=info->pStages[j].stage==VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    unsigned expected=((parts&GPL_PRE)!=0)+((parts&GPL_FS)!=0);
    if(expected && (info->stageCount!=expected || vertex!=((parts&GPL_PRE)!=0) || fragment!=((parts&GPL_FS)!=0)))
        REJECT(STAGES,"stages","stageCount",info->stageCount,"requires-vertex-and-fragment");
    unsigned fs=(parts&GPL_FS) ? stage_flags(d,info,VK_SHADER_STAGE_FRAGMENT_BIT) : SHADER_VALID;
    unsigned vs=(parts&GPL_PRE) ? stage_flags(d,info,VK_SHADER_STAGE_VERTEX_BIT) : SHADER_VALID|SHADER_POSITION_COARSE;
    if(!(fs&SHADER_VALID) || (fs&(SHADER_DISCARD|SHADER_FLAT|SHADER_PIXEL_INPUT|SHADER_NATIVE_RATE|SHADER_UNSUPPORTED)))
        REJECT(FRAGMENT,"shader.fragment","policy-flags",fs,"fragment-not-proven-coarse-safe");
    if(!(vs&SHADER_VALID) || !(vs&SHADER_POSITION_COARSE) || (vs&(SHADER_NATIVE_RATE|SHADER_UNSUPPORTED)))
        REJECT(POSITION,"shader.vertex","policy-flags",vs,"position-not-proven-perspective");
    if(parts&(GPL_FS|GPL_OUT)) {
    const VkPipelineMultisampleStateCreateInfo *ms=info->pMultisampleState;
    if(!ms && ((parts&GPL_OUT) || info->renderPass)) REJECT(MULTISAMPLE,"multisample","state",0,"missing-multisample-state");
    if(ms) {
    if(ms->rasterizationSamples!=VK_SAMPLE_COUNT_1_BIT) {
        VkSampleCountFlags supported=d->rate_samples[1]|d->rate_samples[2]|d->rate_samples[3];
        if(!d->experimental_msaa || !(supported&ms->rasterizationSamples) ||
           (ms->rasterizationSamples!=VK_SAMPLE_COUNT_2_BIT && ms->rasterizationSamples!=VK_SAMPLE_COUNT_4_BIT &&
            ms->rasterizationSamples!=VK_SAMPLE_COUNT_8_BIT))
            REJECT(MULTISAMPLE,"multisample","rasterizationSamples",ms->rasterizationSamples,"only-verified-single-sample");
        uint32_t full=(1u<<ms->rasterizationSamples)-1u;
        if(ms->pSampleMask && (ms->pSampleMask[0]&full)!=full)
            REJECT(MULTISAMPLE,"multisample","sampleMask",ms->pSampleMask[0],"experimental-msaa-requires-full-mask");
    }
    if(ms->sampleShadingEnable) REJECT(MULTISAMPLE,"multisample","sampleShadingEnable",1,"per-sample-shading");
    if(ms->alphaToCoverageEnable) REJECT(MULTISAMPLE,"multisample","alphaToCoverageEnable",1,"alpha-to-coverage");
    if(ms->alphaToOneEnable) REJECT(MULTISAMPLE,"multisample","alphaToOneEnable",1,"alpha-to-one");
    /* Bits above rasterizationSamples are ignored. Dynamic sample masks are
     * admitted only if the device explicitly supports that combination. */
    int dynamic_mask=0;
    const VkPipelineDynamicStateCreateInfo *ds=info->pDynamicState;
    for(uint32_t j=0;ds && j<ds->dynamicStateCount;++j)
        if(ds->pDynamicStates[j]==VK_DYNAMIC_STATE_SAMPLE_MASK_EXT) dynamic_mask=1;
    if(!dynamic_mask && ms->pSampleMask && !(ms->pSampleMask[0]&1u) && !d->caps.fragmentShadingRateWithSampleMask)
        REJECT(MULTISAMPLE,"multisample","sampleMask",ms->pSampleMask[0],"device-no-sample-mask-vrs");
    }
    }
    if(parts&GPL_PRE) {
    const VkPipelineRasterizationStateCreateInfo *rs=info->pRasterizationState;
    if(!rs || rs->polygonMode!=VK_POLYGON_MODE_FILL)
        REJECT(EXTENDED_STATE,"raster","polygonMode",rs ? rs->polygonMode : UINT32_MAX,"requires-fill-rasterization");
    }
    if(parts&GPL_OUT) {
    const VkPipelineColorBlendStateCreateInfo *blend=info->pColorBlendState;
    if(blend && blend->logicOpEnable) REJECT(BLENDING,"blend","logicOpEnable",1,"logic-operation-enabled");
    for(uint32_t j=0;blend && blend->pAttachments && j<blend->attachmentCount;++j)
        if(blend->pAttachments[j].blendEnable) REJECT(BLENDING,"blend","attachment",j,"blending-enabled");
    }
#undef REJECT
    return ACCEPT;
}
static enum Exclusion pipeline_policy(Device *d,const VkGraphicsPipelineCreateInfo *info) {
    enum Exclusion why=flags_policy(d,pipeline_flags(info),1);
    enum Exclusion chain=chain_policy(d,info->pNext,PS_PIPELINE);
    if(why!=ACCEPT) return why;
    if(chain!=ACCEPT) return chain;
    if(pipeline_is_gpl(info)) return EXTENDED_STATE; /* handled by gpl_policy.h */
    return subset_policy(d,info,GPL_ALL);
}
#endif
