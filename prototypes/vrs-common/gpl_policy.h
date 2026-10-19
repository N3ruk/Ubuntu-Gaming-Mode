/* GPL uses private variants, never mutates application library handles.
 * VRS belongs to BOTH pre-raster and fragment subsets. Injecting only at link
 * time is ignored; modifying every library in place breaks mixed safe/UI links.
 * Keep originals static and select matching dynamic variants only after ALL
 * four subsets pass policy. Any allocation/driver failure leaves the original.
 * No retained application pointers or shader handles. */
#ifndef UGM_VRS_GPL_POLICY_H
#define UGM_VRS_GPL_POLICY_H

typedef union GplChain {
    VkBaseOutStructure base;
    VkGraphicsPipelineLibraryCreateInfoEXT subset;
    VkPipelineLibraryCreateInfoKHR link;
    VkPipelineRenderingCreateInfo rendering;
#ifdef VK_KHR_maintenance5
    VkPipelineCreateFlags2CreateInfoKHR flags2;
#endif
} GplChain;

static void gpl_reject(Device *d,VkPipeline handle,enum Exclusion why,const char *rule,uint64_t value) {
    if(d->debug) {
        Pipeline *p=calloc(1,sizeof(*p));
        if(p) {
            p->node.key=HANDLE_KEY(handle); p->rejection=why;
            pthread_mutex_lock(&d->objects_lock);
            if(!table_find(&d->pipelines,p->node.key)) { table_insert(&d->pipelines,&p->node); p=NULL; }
            pthread_mutex_unlock(&d->objects_lock); free(p);
        }
    }
    policy_detail(d,"graphics-library","resolution",value,rule);
    if(d->debug) atomic_fetch_add_explicit(&d->excluded[why],1,memory_order_relaxed);
}
static void gpl_finish(Device *d,VkPipelineCache cache,const VkGraphicsPipelineCreateInfo *info,
        const VkAllocationCallbacks *allocator,VkPipeline *out) {
    uint64_t flags=pipeline_flags(info);
    int library=(flags&VK_PIPELINE_CREATE_LIBRARY_BIT_KHR)!=0;
    if(d->debug && library) atomic_fetch_add_explicit(&d->gpl_libraries,1,memory_order_relaxed);
    enum Exclusion why=flags_policy(d,flags,1);
    if(why!=ACCEPT) { gpl_reject(d,*out,why,"unsupported-flags",flags); return; }
    /* Private re-creation must not change derivative handle/index semantics. */
    if(flags&(VK_PIPELINE_CREATE_DERIVATIVE_BIT|VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)) {
        gpl_reject(d,*out,EXTENDED_STATE,"gpl-derivatives-unmodeled",flags); return;
    }
    why=chain_policy(d,info->pNext,PS_PIPELINE);
    if(why!=ACCEPT) { gpl_reject(d,*out,why,"unsupported-top-level-chain",0); return; }
    const VkGraphicsPipelineLibraryCreateInfoEXT *subset=NULL;
    const VkPipelineLibraryCreateInfoKHR *link=NULL;
    for(const VkBaseInStructure *p=info->pNext;p;p=p->pNext) {
        if(p->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT) subset=(const void *)p;
        if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR) link=(const void *)p;
    }
    unsigned own=subset ? subset->flags : link && link->libraryCount ? 0 : GPL_ALL;
    if(own&~GPL_ALL || (link && (link->libraryCount>4 || (link->libraryCount && !link->pLibraries)))) {
        gpl_reject(d,*out,EXTENDED_STATE,"invalid-or-unmodeled-subsets",own); return;
    }
    why=subset_policy(d,info,own);
    if(why!=ACCEPT) { gpl_reject(d,*out,why,"local-subset-policy",own); return; }
    unsigned parts=own;
    VkSampleCountFlags samples=(own&(GPL_FS|GPL_OUT)) && info->pMultisampleState ?
        info->pMultisampleState->rasterizationSamples : 0;
    VkPipeline targets[4]={0};
    int resolved=1;
    enum Exclusion linked_reason=EXTENDED_STATE;
    pthread_mutex_lock(&d->objects_lock);
    for(uint32_t j=0;link && j<link->libraryCount;++j) {
        Pipeline *p=(Pipeline *)table_find(&d->pipelines,HANDLE_KEY(link->pLibraries[j]));
        if(!p || !p->library || (parts&p->parts)) {
            if(p && p->rejection) linked_reason=(enum Exclusion)p->rejection;
            resolved=0; break;
        }
        parts|=p->parts; samples|=p->samples;
        if((p->parts&(GPL_PRE|GPL_FS)) && !p->shadow) { resolved=0; break; }
        targets[j]=p->shadow ? p->shadow : link->pLibraries[j];
    }
    pthread_mutex_unlock(&d->objects_lock);
    if(!resolved || (!library && parts!=GPL_ALL) || !parts) {
        gpl_reject(d,*out,linked_reason,"missing-unsafe-or-overlapping-library",parts); return;
    }
    Pipeline *tracked=calloc(1,sizeof(*tracked));
    if(!tracked) { gpl_reject(d,*out,ALLOCATION,"tracking-allocation-failed",parts); return; }
    tracked->parts=parts; tracked->library=library; tracked->samples=samples;
    if(!library && d->debug) atomic_fetch_add_explicit(&d->accepted,1,memory_order_relaxed);
    /* VI/output-only libraries do not own shading rate; their original handle
     * is sufficient. Shader-bearing libraries need a consistent private copy. */
    if(parts&(GPL_PRE|GPL_FS)) {
        VkGraphicsPipelineCreateInfo copy=*info;
        GplChain chain[64]; unsigned nodes=0;
        VkBaseOutStructure *tail=NULL;
        copy.pNext=NULL;
        for(const VkBaseInStructure *p=info->pNext;p;p=p->pNext) {
            GplChain *node=&chain[nodes];
            switch(p->sType) {
            case VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO:
                continue; /* original call already populated application output */
            case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:
                node->subset=*(const VkGraphicsPipelineLibraryCreateInfoEXT *)p; break;
            case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:
                node->link=*(const VkPipelineLibraryCreateInfoKHR *)p;
                node->link.pLibraries=targets; break;
            case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:
                node->rendering=*(const VkPipelineRenderingCreateInfo *)p; break;
#ifdef VK_KHR_maintenance5
            case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR:
                node->flags2=*(const VkPipelineCreateFlags2CreateInfoKHR *)p; break;
#endif
            default:
                free(tracked); gpl_reject(d,*out,EXTENDED_STATE,"unclonable-top-level-chain",p->sType); return;
            }
            node->base.pNext=NULL;
            if(tail) tail->pNext=&node->base; else copy.pNext=&node->base;
            tail=&node->base; ++nodes;
        }
        VkDynamicState *values=NULL;
        VkPipelineDynamicStateCreateInfo dynamic;
        if(own&(GPL_PRE|GPL_FS)) {
            const VkPipelineDynamicStateCreateInfo *old=info->pDynamicState;
            uint32_t n=old ? old->dynamicStateCount : 0;
            if(n==UINT32_MAX) { free(tracked); gpl_reject(d,*out,ALLOCATION,"dynamic-state-size",n); return; }
            values=calloc((size_t)n+1,sizeof(*values));
            if(!values) { free(tracked); gpl_reject(d,*out,ALLOCATION,"dynamic-state-allocation-failed",n); return; }
            if(n) memcpy(values,old->pDynamicStates,(size_t)n*sizeof(*values));
            values[n]=VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
            dynamic=old ? *old : (VkPipelineDynamicStateCreateInfo){.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount=n+1; dynamic.pDynamicStates=values; copy.pDynamicState=&dynamic;
        }
        VkPipeline shadow=VK_NULL_HANDLE;
        /* Private libraries use our allocator, final executables use the app's
         * allocator so ordinary vkDestroyPipeline remains correctly paired. */
        VkResult result=d->create_pipelines(d->handle,cache,1,&copy,library ? NULL : allocator,&shadow);
        free(values);
        if(result!=VK_SUCCESS || !shadow) {
            if(shadow) d->destroy_pipeline(d->handle,shadow,library ? NULL : allocator);
            free(tracked);
            if(d->debug) atomic_fetch_add_explicit(&d->create_failed,1,memory_order_relaxed);
            policy_detail(d,"graphics-library","VkResult",(uint64_t)(int64_t)result,"shadow-create-failed-original-preserved");
            return;
        }
        if(library) {
            tracked->shadow=shadow;
            if(d->debug) atomic_fetch_add_explicit(&d->gpl_shadows,1,memory_order_relaxed);
        } else {
            d->destroy_pipeline(d->handle,*out,allocator); *out=shadow; tracked->injected=1;
            if(d->debug) {
                atomic_fetch_add_explicit(&d->injected,1,memory_order_relaxed);
                atomic_fetch_add_explicit(&d->gpl_linked,1,memory_order_relaxed);
            }
        }
    }
    tracked->node.key=HANDLE_KEY(*out);
    pthread_mutex_lock(&d->objects_lock); table_insert(&d->pipelines,&tracked->node);
    pthread_mutex_unlock(&d->objects_lock);
    policy_detail(d,"graphics-library","subsets",parts,library ? "library-proof-retained" : "complete-link-instrumented");
}
#endif
