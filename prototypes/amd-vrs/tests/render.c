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
static VkDevice device;
static VkPhysicalDevice physical;
static VkRenderPass pass;
static VkPipelineLayout layout;
static uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p; vkGetPhysicalDeviceMemoryProperties(physical,&p);
    for(uint32_t i=0;i<p.memoryTypeCount;++i) if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags) return i;
    fprintf(stderr,"No suitable memory\n"); exit(1);
}
static VkShaderModule shader(const char *dir,const char *name) {
    char path[4096]; snprintf(path,sizeof(path),"%s/%s.spv",dir,name); FILE *f=fopen(path,"rb"); assert(f);
    assert(!fseek(f,0,SEEK_END)); long size=ftell(f); assert(size>0); rewind(f);
    uint32_t *data=malloc((size_t)size); assert(data); assert(fread(data,1,(size_t)size,f)==(size_t)size); fclose(f);
    VkShaderModuleCreateInfo ci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=(size_t)size,.pCode=data};
    VkShaderModule result; OK(vkCreateShaderModule(device,&ci,NULL,&result)); free(data); return result;
}
static VkPipeline pipeline(const char *dir,const char *vert,const char *frag) {
    VkShaderModule v=shader(dir,vert), f=shader(dir,frag);
    VkPipelineShaderStageCreateInfo stages[2]={
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=v,.pName="main"},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=f,.pName="main"}};
    VkPipelineVertexInputStateCreateInfo vi={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport vp={.width=WIDTH,.height=WIDTH,.maxDepth=1}; VkRect2D sc={.extent={WIDTH,WIDTH}};
    VkPipelineViewportStateCreateInfo view={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.pViewports=&vp,.scissorCount=1,.pScissors=&sc};
    VkPipelineRasterizationStateCreateInfo rs={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
    VkPipelineMultisampleStateCreateInfo ms={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState att={.colorWriteMask=15};
    VkPipelineColorBlendStateCreateInfo blend={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&att};
    VkGraphicsPipelineCreateInfo ci={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,.pVertexInputState=&vi,.pInputAssemblyState=&ia,.pViewportState=&view,.pRasterizationState=&rs,.pMultisampleState=&ms,.pColorBlendState=&blend,.layout=layout,.renderPass=pass};
    VkPipeline out; OK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,NULL,&out));
    vkDestroyShaderModule(device,v,NULL); vkDestroyShaderModule(device,f,NULL); return out;
}
int main(int argc,char **argv) {
    assert(argc==2);
    int app_vrs = getenv("UGM_TEST_APP_VRS") != NULL;
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="UGM VRS offscreen regression",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};
    VkInstance instance; OK(vkCreateInstance(&ici,NULL,&instance));
    uint32_t n=0; OK(vkEnumeratePhysicalDevices(instance,&n,NULL)); assert(n&&n<32); VkPhysicalDevice gpus[32]; OK(vkEnumeratePhysicalDevices(instance,&n,gpus));
    for(uint32_t i=0;i<n;++i) {
        VkPhysicalDeviceDriverProperties driver={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 props={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,.pNext=&driver};
        vkGetPhysicalDeviceProperties2(gpus[i],&props);
        if(props.properties.vendorID==0x1002 && driver.driverID==VK_DRIVER_ID_MESA_RADV) {
            physical=gpus[i]; printf("GPU RADV: %s\n",props.properties.deviceName); break;
        }
    }
    assert(physical);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,NULL); assert(n<64); VkQueueFamilyProperties families[64]; vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families);
    uint32_t family=0; while(family<n && !(families[family].queueFlags&VK_QUEUE_GRAPHICS_BIT)) ++family; assert(family<n);
    float priority=1; VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR native = {.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
    const char *extension = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
    VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,
        .pNext=app_vrs ? &native : NULL,.enabledExtensionCount=app_vrs ? 1u : 0u,.ppEnabledExtensionNames=app_vrs ? &extension : NULL};
    OK(vkCreateDevice(physical,&dci,NULL,&device));
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
    VkSubpassDescription sub={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&ref};
    VkSubpassDependency deps[2]={
        {.srcSubpass=VK_SUBPASS_EXTERNAL,.dstSubpass=0,.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
        {.srcSubpass=0,.dstSubpass=VK_SUBPASS_EXTERNAL,.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT,.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT}};
    VkRenderPassCreateInfo rpci={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=1,.pAttachments=&attachment,.subpassCount=1,.pSubpasses=&sub,.dependencyCount=2,.pDependencies=deps}; OK(vkCreateRenderPass(device,&rpci,NULL,&pass));
    VkFramebufferCreateInfo fbci={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=pass,.attachmentCount=1,.pAttachments=&view,.width=WIDTH,.height=WIDTH,.layers=1};
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
    for(int scenario=0;scenario<4;++scenario) for(int rate=1;rate<=2;++rate) {
        rewind(ratefile); assert(fprintf(ratefile,"%dx%d\n",rate,rate)>0); assert(!fflush(ratefile));
        OK(vkResetCommandBuffer(cmd,0)); VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; OK(vkBeginCommandBuffer(cmd,&begin));
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
            assert(black==2048 && white==2048); memcpy(reference,mapped,BYTES);
        } else {
            unsigned changed=0; for(unsigned i=0;i<BYTES;i+=4) changed+=memcmp(reference+i,(unsigned char*)mapped+i,4)!=0;
            printf("%s: %u/4096 pixels changed by 2x2\n",names[scenario],changed);
            if(scenario==0 && !app_vrs) assert(changed>=1024); else assert(changed==0);
        }
    }
    fclose(ratefile); assert(!remove(ratepath));
    vkDestroyCommandPool(device,pool,NULL);
    for(int i=0;i<3;++i) vkDestroyPipeline(device,pipes[i],NULL);
    vkDestroyPipelineLayout(device,layout,NULL); vkDestroyFramebuffer(device,framebuffer,NULL); vkDestroyRenderPass(device,pass,NULL);
    vkDestroyImageView(device,view,NULL); vkDestroyImage(device,image,NULL); vkFreeMemory(device,imem,NULL);
    vkUnmapMemory(device,bmem); vkDestroyBuffer(device,buffer,NULL); vkFreeMemory(device,bmem,NULL); vkDestroyDevice(device,NULL); vkDestroyInstance(instance,NULL);
    puts("GPU offscreen differential test: OK"); return 0;
}
