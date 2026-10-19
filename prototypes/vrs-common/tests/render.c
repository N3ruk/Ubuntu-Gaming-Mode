/* Offscreen differential GPU test: no window, Steam, game or session changes. */
#define _POSIX_C_SOURCE 200809L
#include <vulkan/vulkan.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OK(call) do { VkResult r_=(call); if(r_!=VK_SUCCESS) { fprintf(stderr,"%s -> %d at %d\n",#call,r_,__LINE__); exit(1); } } while(0)
#define WIDTH 64
#define BYTES (WIDTH*WIDTH*4)
static VkSampleCountFlagBits test_samples=VK_SAMPLE_COUNT_1_BIT;
static VkDevice device;
static int native_rate, extended_state, gpl_mode, heap_mode, identifier_mode;
static VkPhysicalDevice physical;
static VkRenderPass pass;
static VkPipelineLayout layout;
static VkPipeline gpl_libraries[32];
static unsigned gpl_library_count;
static uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p; vkGetPhysicalDeviceMemoryProperties(physical,&p);
    for(uint32_t i=0;i<p.memoryTypeCount;++i) if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags) return i;
    fprintf(stderr,"No suitable memory\n"); exit(1);
}
static uint32_t *shader_code(const char *dir,const char *name,size_t *bytes) {
    if(getenv("UGM_TEST_MODERN")) {
        if(!strcmp(name,"perspective.vert")) name="modern-perspective.vert";
        if(!strcmp(name,"ui.vert")) name="modern-ui.vert";
    }
    char path[4096]; snprintf(path,sizeof(path),"%s/%s.spv",dir,name); FILE *f=fopen(path,"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); long size=ftell(f); assert(size>0); rewind(f);
    uint32_t *data=malloc((size_t)size); assert(data); assert(fread(data,1,(size_t)size,f)==(size_t)size); fclose(f);
    *bytes=(size_t)size;return data;
}
static VkShaderModule shader(const char *dir,const char *name) {
    size_t size;uint32_t *data=shader_code(dir,name,&size);
    VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=size,.pCode=data};
    VkShaderModule result; OK(vkCreateShaderModule(device,&ci,NULL,&result)); free(data); return result;
}
static VkResult create_pipeline(VkDevice dev,VkPipelineCache cache,uint32_t count,
        const VkGraphicsPipelineCreateInfo *info,const VkAllocationCallbacks *allocator,VkPipeline *out) {
    assert(count==1);
    VkGraphicsPipelineCreateInfo copy=*info;
    VkPipelineCreateFlags2CreateInfoKHR flags={.sType=VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
        .pNext=info->pNext,.flags=(uint64_t)info->flags|VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT};
    /* Libraries containing identifier-only stages require the same fail-on-
     * compile flag at every later link, including intermediate GPL libraries. */
    if(identifier_mode && !copy.stageCount) {
        for(const VkBaseInStructure *p=copy.pNext;p;p=p->pNext)
            if(p->sType==VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR &&
               ((const VkPipelineLibraryCreateInfoKHR *)p)->libraryCount) {
                copy.flags|=VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
                flags.flags|=VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
            }
    }
    if(heap_mode) { copy.pNext=&flags; copy.layout=VK_NULL_HANDLE; }
    VkResult result=vkCreateGraphicsPipelines(dev,cache,count,&copy,allocator,out);
    if(result==VK_SUCCESS && identifier_mode && copy.stageCount) {
        assert(copy.stageCount<=2);
        VkPipelineShaderStageCreateInfo stages[2];
        VkShaderModuleIdentifierEXT ids[2];
        VkPipelineShaderStageModuleIdentifierCreateInfoEXT id_chain[2];
        PFN_vkGetShaderModuleIdentifierEXT get_id=(PFN_vkGetShaderModuleIdentifierEXT)vkGetDeviceProcAddr(dev,"vkGetShaderModuleIdentifierEXT");
        PFN_vkGetShaderModuleCreateInfoIdentifierEXT get_info=(PFN_vkGetShaderModuleCreateInfoIdentifierEXT)vkGetDeviceProcAddr(dev,"vkGetShaderModuleCreateInfoIdentifierEXT");
        assert(get_id && get_info);
        for(uint32_t j=0;j<copy.stageCount;++j) {
            stages[j]=copy.pStages[j];
            ids[j]=(VkShaderModuleIdentifierEXT){.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT};
            const void *tail=stages[j].pNext;
            if(stages[j].module) get_id(dev,stages[j].module,&ids[j]);
            else {
                const VkShaderModuleCreateInfo *module=stages[j].pNext;
                assert(module && module->sType==VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
                get_info(dev,module,&ids[j]); tail=module->pNext;
            }
            assert(ids[j].identifierSize);
            id_chain[j]=(VkPipelineShaderStageModuleIdentifierCreateInfoEXT){
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT,
                .pNext=tail,.identifierSize=ids[j].identifierSize,.pIdentifier=ids[j].identifier};
            stages[j].module=VK_NULL_HANDLE;stages[j].pNext=&id_chain[j];
        }
        copy.pStages=stages;
        copy.flags|=VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        if(heap_mode) flags.flags|=VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
        VkPipeline by_id=VK_NULL_HANDLE;
        result=vkCreateGraphicsPipelines(dev,cache,count,&copy,allocator,&by_id);
        if(result!=VK_SUCCESS) fprintf(stderr,"Identifier fixture failed: %d\n",result);
        assert(result==VK_SUCCESS && by_id);
        vkDestroyPipeline(dev,*out,allocator);*out=by_id;
    }
    return result;
}
static VkPipeline pipeline(const char *dir,const char *vert,const char *frag) {
    /* An identifier-only link is allowed to return PIPELINE_COMPILE_REQUIRED
     * on a cold cache. Warm the exact complete source pipeline first, rather
     * than depending on a persistent driver cache from earlier test runs. */
    if(identifier_mode) {
        identifier_mode=0;
        VkPipeline warm=pipeline(dir,vert,frag);
        identifier_mode=1;
        vkDestroyPipeline(device,warm,NULL);
    }

    VkShaderModule v=shader(dir,vert), f=shader(dir,frag);
    VkPipelineShaderStageCreateInfo stages[2]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=v,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=f,.pName="main"}};
    VkShaderDescriptorSetAndBindingMappingInfoEXT mapping={.sType=VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
    VkShaderModuleCreateInfo inline_shader[2];uint32_t *inline_code[2]={NULL,NULL};
    if(heap_mode) for(unsigned j=0;j<2;++j) {
        size_t bytes;inline_code[j]=shader_code(dir,j ? frag : vert,&bytes);
        inline_shader[j]=(VkShaderModuleCreateInfo){.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext=&mapping,.codeSize=bytes,.pCode=inline_code[j]};
        stages[j].module=VK_NULL_HANDLE;stages[j].pNext=&inline_shader[j];
    }
    VkPipelineVertexInputStateCreateInfo vi={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport vp={.width=WIDTH,.height=WIDTH,.maxDepth=1}; VkRect2D sc={.extent={WIDTH,WIDTH}};
    VkPipelineViewportStateCreateInfo view={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.pViewports=&vp,.scissorCount=1,.pScissors=&sc};
    VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
    VkPipelineRasterizationDepthClipStateCreateInfoEXT depth_clip={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,.depthClipEnable=VK_TRUE};
    if(extended_state) rs.pNext=&depth_clip;
    VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=test_samples};
    VkPipelineColorBlendAttachmentState att={.colorWriteMask=15};
    VkPipelineColorBlendStateCreateInfo blend={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&att};
    VkPipelineFragmentShadingRateStateCreateInfoKHR native={.sType=VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,
        .fragmentSize={2,2},.combinerOps={VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR}};
    VkGraphicsPipelineCreateInfo ci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,.pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&view,.pRasterizationState=&rs,.pMultisampleState=&ms,.pColorBlendState=&blend,.layout=layout,.renderPass=pass};
    if(native_rate && !strcmp(vert,"perspective.vert") && !strcmp(frag,"checker.frag")) ci.pNext=&native;
    VkPipeline out;
    if(gpl_mode) {
        VkPipeline libs[4];
        for(unsigned part=0;part<4;++part) {
            VkGraphicsPipelineLibraryCreateInfoEXT subset={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,.flags=1u<<part};
            VkGraphicsPipelineCreateInfo piece=ci;
            if((part==1 || part==2) && ci.pNext) subset.pNext=ci.pNext;
            piece.flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
            piece.pNext=&subset;
            piece.stageCount=part==1 || part==2 ? 1 : 0;
            piece.pStages=part==1 ? &stages[0] : part==2 ? &stages[1] : NULL;
            OK(create_pipeline(device,VK_NULL_HANDLE,1,&piece,NULL,&libs[part]));
        }
        /* Libraries must remain analyzable after source modules are destroyed. */
        vkDestroyShaderModule(device,v,NULL); vkDestroyShaderModule(device,f,NULL);
        v=f=VK_NULL_HANDLE;
        VkPipelineLibraryCreateInfoKHR links={.sType=VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,.libraryCount=4,.pLibraries=libs};
        VkGraphicsPipelineCreateInfo linked={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&links,.layout=layout,
            .flags=VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT,.renderPass=pass};
        VkPipeline middle=VK_NULL_HANDLE;
        VkPipeline nested[3];
        if(gpl_mode==2) {
            VkPipelineLibraryCreateInfoKHR shaders={.sType=VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,.libraryCount=2,.pLibraries=&libs[1]};
            VkGraphicsPipelineCreateInfo intermediate={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&shaders,
                .flags=VK_PIPELINE_CREATE_LIBRARY_BIT_KHR|VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT,.layout=layout,.renderPass=pass};
            OK(create_pipeline(device,VK_NULL_HANDLE,1,&intermediate,NULL,&middle));
            nested[0]=libs[0];nested[1]=middle;nested[2]=libs[3];links.libraryCount=3;links.pLibraries=nested;
        }
        OK(create_pipeline(device,VK_NULL_HANDLE,1,&linked,NULL,&out));
        for(unsigned part=0;part<4;++part) gpl_libraries[gpl_library_count++]=libs[part];
        if(middle) gpl_libraries[gpl_library_count++]=middle;
        assert(gpl_library_count<32);
    } else OK(create_pipeline(device,VK_NULL_HANDLE,1,&ci,NULL,&out));
    free(inline_code[0]);free(inline_code[1]);
    vkDestroyShaderModule(device,v,NULL); vkDestroyShaderModule(device,f,NULL); return out;
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(getenv("UGM_TEST_SAMPLES")) test_samples=(VkSampleCountFlagBits)atoi(getenv("UGM_TEST_SAMPLES"));
    assert(test_samples==1 || test_samples==2 || test_samples==4 || test_samples==8);
    printf("Test MSAA samples=%u experimental-rate=%s\n",test_samples,
        getenv("UGM_VRS_EXPERIMENTAL_RATE") ? getenv("UGM_VRS_EXPERIMENTAL_RATE") : "2x2");
    int app_vrs = getenv("UGM_TEST_APP_VRS") != NULL;
    extended_state=getenv("UGM_TEST_EXTENDED_STATE")!=NULL;
    heap_mode=getenv("UGM_TEST_HEAP")!=NULL;
    identifier_mode=getenv("UGM_TEST_IDENTIFIERS")!=NULL;
    gpl_mode=getenv("UGM_TEST_GPL") ? atoi(getenv("UGM_TEST_GPL")) : 0;
    native_rate=app_vrs && atoi(getenv("UGM_TEST_APP_VRS"))==2;
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="UGM VRS offscreen regression",.apiVersion=heap_mode ? VK_API_VERSION_1_3 : VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};
    VkInstance instance; OK(vkCreateInstance(&ici,NULL,&instance));
    uint32_t n=0; OK(vkEnumeratePhysicalDevices(instance,&n,NULL)); assert(n&&n<32); VkPhysicalDevice gpus[32]; OK(vkEnumeratePhysicalDevices(instance,&n,gpus));
    for(uint32_t i=0;i<n;++i) { VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(gpus[i],&p); if(p.vendorID==UGM_GPU_VENDOR) { physical=gpus[i]; printf("GPU: %s\n",p.deviceName); break; } }
    assert(physical);
    /* Independent second instance, destroyed before using the first physical
     * device: catches global-GIPA/last-instance dispatch regressions. */
    VkInstance second; OK(vkCreateInstance(&ici,NULL,&second));
    uint32_t other_count=0; OK(vkEnumeratePhysicalDevices(second,&other_count,NULL)); assert(other_count);
    vkDestroyInstance(second,NULL);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,NULL); assert(n<64); VkQueueFamilyProperties families[64]; vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families);
    uint32_t family=0; while(family<n && !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) ++family; assert(family<n);
    float priority=1; VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR native = {.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,.pipelineFragmentShadingRate=VK_TRUE};
    const char *extensions[16]; unsigned extension_count=0;
    if(app_vrs) { extensions[extension_count++]=VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
        extensions[extension_count++]=VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME; }
    VkPhysicalDeviceDepthClipEnableFeaturesEXT depth_clip={
        .sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,.depthClipEnable=VK_TRUE};
    if(extended_state) {
        VkPhysicalDeviceFeatures2 support={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,.pNext=&depth_clip};
        vkGetPhysicalDeviceFeatures2(physical,&support); assert(depth_clip.depthClipEnable);
        extensions[extension_count++]=VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME;
        depth_clip.pNext=app_vrs ? &native : NULL;
        puts("Extended-state GPU fixture: VkPipelineRasterizationDepthClipStateCreateInfoEXT");
    }
    VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,
        .pNext=extended_state ? (void *)&depth_clip : app_vrs ? (void *)&native : NULL,.enabledExtensionCount=extension_count,.ppEnabledExtensionNames=extensions};
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gpl={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
    if(gpl_mode) {
        VkPhysicalDeviceFeatures2 support={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,.pNext=&gpl};
        vkGetPhysicalDeviceFeatures2(physical,&support); assert(gpl.graphicsPipelineLibrary);
        extensions[extension_count++]=VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME;
        extensions[extension_count++]=VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME;
        gpl.pNext=(void *)dci.pNext; dci.pNext=&gpl;dci.enabledExtensionCount=extension_count;
        printf("GPL fixture: mode=%d\n",gpl_mode);
    }
    VkPhysicalDeviceDescriptorHeapFeaturesEXT heap={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT};
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,.maintenance5=VK_TRUE};
    VkPhysicalDeviceDynamicRenderingFeatures rendering={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,.dynamicRendering=VK_TRUE};
    VkPhysicalDeviceBufferDeviceAddressFeatures address={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,.bufferDeviceAddress=VK_TRUE};
    if(heap_mode) {
        VkPhysicalDeviceFeatures2 support={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,.pNext=&heap};
        vkGetPhysicalDeviceFeatures2(physical,&support); assert(heap.descriptorHeap);
        heap.descriptorHeapCaptureReplay=VK_FALSE;
        extensions[extension_count++]=VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME;
        extensions[extension_count++]=VK_KHR_MAINTENANCE_5_EXTENSION_NAME;
        address.pNext=(void *)dci.pNext;rendering.pNext=&address;maintenance5.pNext=&rendering;heap.pNext=&maintenance5;
        dci.pNext=&heap;dci.enabledExtensionCount=extension_count;
        puts("Descriptor heap fixture: flags2 + inline SPIR-V + resource mappings");
    }
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT identifiers={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT};
    VkPhysicalDevicePipelineCreationCacheControlFeatures cache_control={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES};
    if(identifier_mode) {
        identifiers.pNext=&cache_control;
        VkPhysicalDeviceFeatures2 support={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,.pNext=&identifiers};
        vkGetPhysicalDeviceFeatures2(physical,&support);assert(identifiers.shaderModuleIdentifier && cache_control.pipelineCreationCacheControl);
        extensions[extension_count++]=VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME;
        extensions[extension_count++]=VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME;
        cache_control.pNext=(void *)dci.pNext;dci.pNext=&identifiers;dci.enabledExtensionCount=extension_count;
        puts("Identifier fixture: replace source stages with observed opaque IDs");
    }
    OK(vkCreateDevice(physical,&dci,NULL,&device));
    VkDevice second_device; OK(vkCreateDevice(physical,&dci,NULL,&second_device)); vkDestroyDevice(second_device,NULL);
    VkQueue queue; vkGetDeviceQueue(device,family,0,&queue);
    VkImageCreateInfo imci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,.extent={WIDTH,WIDTH,1},.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
    VkImage image; OK(vkCreateImage(device,&imci,NULL,&image)); VkMemoryRequirements req; vkGetImageMemoryRequirements(device,image,&req);
    VkMemoryAllocateInfo ma={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    VkDeviceMemory imem; OK(vkAllocateMemory(device,&ma,NULL,&imem)); OK(vkBindImageMemory(device,image,imem,0));
    VkImageViewCreateInfo vci={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=image,.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_R8G8B8A8_UNORM,.subresourceRange={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.levelCount=1,.layerCount=1}};
    VkImageView view; OK(vkCreateImageView(device,&vci,NULL,&view));
    VkBufferCreateInfo bci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=BYTES,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VkBuffer buffer; OK(vkCreateBuffer(device,&bci,NULL,&buffer)); vkGetBufferMemoryRequirements(device,buffer,&req);
    ma.allocationSize=req.size; ma.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory bmem; OK(vkAllocateMemory(device,&ma,NULL,&bmem)); OK(vkBindBufferMemory(device,buffer,bmem,0)); void *mapped; OK(vkMapMemory(device,bmem,0,BYTES,0,&mapped));
    VkAttachmentDescription attachment={.format=VK_FORMAT_R8G8B8A8_UNORM,.samples=VK_SAMPLE_COUNT_1_BIT,.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    VkAttachmentReference ref={.attachment=0,.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkImage msimage=VK_NULL_HANDLE; VkImageView msview=VK_NULL_HANDLE; VkDeviceMemory msmem=VK_NULL_HANDLE;
    VkAttachmentDescription attachments[2]={attachment,attachment};
    VkAttachmentReference resolve={.attachment=1,.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkImageView views[2]={view,view};
    if(test_samples!=1) {
        imci.samples=test_samples; imci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        OK(vkCreateImage(device,&imci,NULL,&msimage)); vkGetImageMemoryRequirements(device,msimage,&req);
        ma.allocationSize=req.size; ma.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        OK(vkAllocateMemory(device,&ma,NULL,&msmem)); OK(vkBindImageMemory(device,msimage,msmem,0));
        vci.image=msimage; OK(vkCreateImageView(device,&vci,NULL,&msview)); views[0]=msview;
        attachments[0].samples=test_samples; attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    VkSubpassDescription sub={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&ref,
        .pResolveAttachments=test_samples!=1 ? &resolve : NULL};
    VkSubpassDependency deps[2]={
        {.srcSubpass=VK_SUBPASS_EXTERNAL,.dstSubpass=0,.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
        {.srcSubpass=0,.dstSubpass=VK_SUBPASS_EXTERNAL,.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT,.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT}};
    VkRenderPassCreateInfo rpci={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=test_samples!=1 ? 2 : 1,.pAttachments=attachments,.subpassCount=1,.pSubpasses=&sub,.dependencyCount=2,.pDependencies=deps}; OK(vkCreateRenderPass(device,&rpci,NULL,&pass));
    VkFramebufferCreateInfo fbci={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=pass,.attachmentCount=test_samples!=1 ? 2 : 1,.pAttachments=views,.width=WIDTH,.height=WIDTH,.layers=1};
    VkFramebuffer framebuffer; OK(vkCreateFramebuffer(device,&fbci,NULL,&framebuffer));
    VkPipelineLayoutCreateInfo plci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; OK(vkCreatePipelineLayout(device,&plci,NULL,&layout));
    VkPipeline pipes[3]={pipeline(argv[1],"perspective.vert","checker.frag"),pipeline(argv[1],"ui.vert","checker.frag"),pipeline(argv[1],"perspective.vert","fragcoord.frag")};
    VkCommandPoolCreateInfo cpci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};
    VkCommandPool pool; OK(vkCreateCommandPool(device,&cpci,NULL,&pool));
    VkCommandBufferAllocateInfo cbai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer cmd; OK(vkAllocateCommandBuffers(device,&cbai,&cmd));
    unsigned char reference[BYTES];
    char ratepath[]="/tmp/ugm-vrs-render-XXXXXX"; int fd=mkstemp(ratepath); assert(fd>=0); FILE *ratefile=fdopen(fd,"w"); assert(ratefile); assert(setenv("UGM_VRS_CONFIG_FILE",ratepath,1)==0);
    const char *names[]={"perspective","UI W=1","FragCoord","coarse then UI"};
    for(int scenario=0;scenario<4;++scenario) for(int rate=1;rate<=3;++rate) {
        rewind(ratefile); assert(fprintf(ratefile,"%dx%d\n",rate==2 ? 2 : 1,rate==2 ? 2 : 1)>0); assert(!fflush(ratefile));
        if(rate==3) { OK(vkResetCommandPool(device,pool,0)); } else { OK(vkResetCommandBuffer(cmd,0)); } VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; OK(vkBeginCommandBuffer(cmd,&begin));
        VkClearValue clear={.color={{1,0,1,1}}};
        VkRenderPassBeginInfo rbegin={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,.renderPass=pass,.framebuffer=framebuffer,.renderArea={.extent={WIDTH,WIDTH}},.clearValueCount=1,.pClearValues=&clear};
        vkCmdBeginRenderPass(cmd,&rbegin,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipes[scenario<3 ? scenario : 0]); vkCmdDraw(cmd,3,1,0,0);
        if(scenario==3) { vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipes[1]); vkCmdDraw(cmd,3,1,0,0); }
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy copy={.imageSubresource={.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.layerCount=1},.imageExtent={WIDTH,WIDTH,1}};
        vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
        VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,NULL,0,NULL);
        OK(vkEndCommandBuffer(cmd)); VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cmd}; OK(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)); OK(vkQueueWaitIdle(queue));
        if(rate==1) {
            unsigned black=0,white=0; unsigned char *p=mapped;
            for(unsigned i=0;i<BYTES;i+=4) { black+=p[i]==0&&p[i+1]==0&&p[i+2]==0; white+=p[i]==255&&p[i+1]==255&&p[i+2]==255; }
            assert(native_rate && scenario==0 ? black+white==4096 : black==2048 && white==2048); memcpy(reference,mapped,BYTES);
        } else {
            unsigned changed=0; for(unsigned i=0;i<BYTES;i+=4) changed+=memcmp(reference+i,(unsigned char*)mapped+i,4)!=0;
            printf("%s: %u/4096 pixels changed at %dx%d\n",names[scenario],changed,rate==2 ? 2 : 1,rate==2 ? 2 : 1);
            int audit_blocks=(extended_state || gpl_mode || heap_mode) && getenv("UGM_VRS_AUDIT_ONLY") && !strcmp(getenv("UGM_VRS_AUDIT_ONLY"),"1");
            int msaa_blocks=test_samples!=1 && !getenv("UGM_VRS_EXPERIMENTAL_MSAA");
            const char *exp=getenv("UGM_VRS_EXPERIMENTAL_RATE");
            int unsupported=test_samples==8 && (!exp || strcmp(exp,"1x2"));
            if(scenario==0 && !app_vrs && !audit_blocks && !msaa_blocks && !unsupported && rate==2) assert(changed>=1024); else assert(changed==0);
        }
    }
    fclose(ratefile); assert(!remove(ratepath));
    vkDestroyCommandPool(device,pool,NULL);
    for(int i=0;i<3;++i) vkDestroyPipeline(device,pipes[i],NULL);
    while(gpl_library_count) vkDestroyPipeline(device,gpl_libraries[--gpl_library_count],NULL);
    vkDestroyPipelineLayout(device,layout,NULL); vkDestroyFramebuffer(device,framebuffer,NULL); vkDestroyRenderPass(device,pass,NULL);
    if(msimage) { vkDestroyImageView(device,msview,NULL); vkDestroyImage(device,msimage,NULL); vkFreeMemory(device,msmem,NULL); }
    vkDestroyImageView(device,view,NULL); vkDestroyImage(device,image,NULL); vkFreeMemory(device,imem,NULL);
    vkUnmapMemory(device,bmem); vkDestroyBuffer(device,buffer,NULL); vkFreeMemory(device,bmem,NULL); vkDestroyDevice(device,NULL); vkDestroyInstance(instance,NULL);
    puts("GPU offscreen differential test: OK"); return 0;
}
