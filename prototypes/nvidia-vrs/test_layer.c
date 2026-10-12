/* Policy/integration mocks use real validated SPIR-V, not synthetic no-op shaders. */
#define _POSIX_C_SOURCE 200809L
#include "layer.c"
#include <assert.h>
#include <stdint.h>
static uintptr_t next_handle = 1;
static int got_dynamic_vrs, bound_rate, bind_calls;
static VkResult pipeline_result = VK_SUCCESS;
static VkResult fake_shader(VkDevice d, const VkShaderModuleCreateInfo *i,
        const VkAllocationCallbacks *a, VkShaderModule *out) {
    (void)d; (void)i; (void)a; *out = (VkShaderModule)(uintptr_t)next_handle++; return VK_SUCCESS;
}
static void fake_destroy(VkDevice d, VkShaderModule m, const VkAllocationCallbacks *a) {
    (void)d; (void)m; (void)a;
}
static void fake_bind(VkCommandBuffer c, VkPipelineBindPoint p, VkPipeline v) {
    (void)c; (void)p; (void)v; ++bind_calls;
}
static void fake_rate(VkCommandBuffer c, const VkExtent2D *r, const VkFragmentShadingRateCombinerOpKHR *ops) {
    (void)c; assert(ops[0] == VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR); bound_rate = (int)r->width;
}
static VkResult fake_allocate(VkDevice d, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out) {
    (void)d;
    for (uint32_t i = 0; i < info->commandBufferCount; i++) out[i] = (VkCommandBuffer)(uintptr_t)next_handle++;
    return VK_SUCCESS;
}
static VkResult fake_pipeline(VkDevice d, VkPipelineCache c, uint32_t n,
        const VkGraphicsPipelineCreateInfo *infos, const VkAllocationCallbacks *a, VkPipeline *out) {
    (void)d; (void)c; (void)a; assert(n == 1); got_dynamic_vrs = 0;
    const VkPipelineDynamicStateCreateInfo *s = infos[0].pDynamicState;
    if (s) for (uint32_t i = 0; i < s->dynamicStateCount; i++)
        if (s->pDynamicStates[i] == VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR) got_dynamic_vrs = 1;
    *out = (VkPipeline)(uintptr_t)next_handle++; return pipeline_result;
}
static VkShaderModuleCreateInfo load(const char *dir, const char *name) {
    char path[4096]; snprintf(path, sizeof(path), "%s/%s.spv", dir, name);
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long size = ftell(f); assert(size > 0 && !(size % 4)); rewind(f);
    uint32_t *code = malloc((size_t)size); assert(code);
    assert(fread(code, 1, (size_t)size, f) == (size_t)size); fclose(f);
    return (VkShaderModuleCreateInfo){.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=(size_t)size, .pCode=code};
}
static void check(Device *d, const VkShaderModuleCreateInfo *vs, const VkShaderModuleCreateInfo *fs,
                  int expected, int blend_on, int sample, int inline_module, int app_vrs) {
    VkShaderModule v = VK_NULL_HANDLE, f = VK_NULL_HANDLE;
    if (!inline_module) {
        assert(vkCreateShaderModule(d->handle, vs, NULL, &v) == VK_SUCCESS);
        assert(vkCreateShaderModule(d->handle, fs, NULL, &f) == VK_SUCCESS);
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage=VK_SHADER_STAGE_VERTEX_BIT, .module=v, .pName="main", .pNext=inline_module ? vs : NULL},
        {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage=VK_SHADER_STAGE_FRAGMENT_BIT, .module=f, .pName="main", .pNext=inline_module ? fs : NULL}};
    VkPipelineMultisampleStateCreateInfo ms = {.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable=sample};
    VkPipelineColorBlendAttachmentState attachment = {.blendEnable=blend_on};
    VkPipelineColorBlendStateCreateInfo blend = {.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount=1, .pAttachments=&attachment};
    VkPipelineFragmentShadingRateStateCreateInfoKHR native = {.sType=VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR, .fragmentSize={1,1}};
    VkGraphicsPipelineCreateInfo ci = {.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount=2, .pStages=stages, .pMultisampleState=&ms, .pColorBlendState=&blend, .pNext=app_vrs ? &native : NULL};
    VkPipeline pipeline;
    assert(vkCreateGraphicsPipelines(d->handle, VK_NULL_HANDLE, 1, &ci, NULL, &pipeline) == pipeline_result);
    assert(got_dynamic_vrs == expected);
    assert(ci.pDynamicState == NULL && stages[0].module == v); /* app memory untouched */
    assert(unsetenv("UGM_VRS_CONFIG_FILE") == 0);
    for (int rate = 1; rate <= 2; ++rate) {
        assert(setenv("UGM_VRS_RATE", rate == 2 ? "2x2" : "1x1", 1) == 0);
        bound_rate = 0;
        vkCmdBindPipeline(commands->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        assert(bound_rate == (expected ? rate : 0));
    }
    vkDestroyPipeline(d->handle, pipeline, NULL);
    if (!inline_module) { vkDestroyShaderModule(d->handle, v, NULL); vkDestroyShaderModule(d->handle, f, NULL); }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    Device d = {.handle=(VkDevice)(uintptr_t)1, .active=1, .create_shader=fake_shader,
        .destroy_shader=fake_destroy, .create_pipelines=fake_pipeline, .bind_pipeline=fake_bind, .set_rate=fake_rate};
    Command c = {.handle=(VkCommandBuffer)(uintptr_t)2, .device=&d}; devices=&d; commands=&c;
    VkShaderModuleCreateInfo perspective=load(argv[1],"perspective.vert"), checker=load(argv[1],"checker.frag");
    assert(shader_flags(&perspective) & SHADER_POSITION_COARSE);
    check(&d,&perspective,&checker,1,0,0,0,0);
    check(&d,&perspective,&checker,1,0,0,1,0);
    check(&d,&perspective,&checker,0,1,0,0,0);
    check(&d,&perspective,&checker,0,0,1,0,0);
    check(&d,&perspective,&checker,0,0,0,0,1);
    pipeline_result = VK_ERROR_OUT_OF_DEVICE_MEMORY; /* successful handle in a partial batch */
    check(&d,&perspective,&checker,1,0,0,0,0); pipeline_result=VK_SUCCESS;
    const char *protected_vs[]={"ui.vert","partial-ui.vert","local-ui.vert","select-ui.vert"};
    for (unsigned i=0;i<sizeof(protected_vs)/sizeof(*protected_vs);++i) {
        VkShaderModuleCreateInfo v=load(argv[1],protected_vs[i]);
        printf("%-24s flags=%u\n",protected_vs[i],shader_flags(&v));
        check(&d,&v,&checker,0,0,0,0,0); check(&d,&v,&checker,0,0,0,1,0); free((void*)v.pCode);
    }
    VkShaderModuleCreateInfo matrix=load(argv[1],"matrix.vert");
    check(&d,&matrix,&checker,1,0,0,0,0); free((void*)matrix.pCode);
    const char *protected_fs[]={"fragcoord.frag","discard.frag","flat.frag","sample.frag"};
    for (unsigned i=0;i<sizeof(protected_fs)/sizeof(*protected_fs);++i) {
        VkShaderModuleCreateInfo f=load(argv[1],protected_fs[i]);
        check(&d,&perspective,&f,0,0,0,0,0); free((void*)f.pCode);
    }
    assert(parse_rate("2x1").width == 1); assert(parse_rate("2x2extra").width == 1);
    assert(parse_rate("2x2\n").width == 2);
    uint32_t malformed[]={0x07230203,0x10000,0,2,0,0x20000};
    VkShaderModuleCreateInfo bad={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.pCode=malformed,.codeSize=sizeof(malformed)};
    assert(!(shader_flags(&bad)&SHADER_VALID));
    check(&d,&perspective,&bad,0,0,0,0,0);
    assert(!shaders && !pipelines);
    /* Ineligible/native-VRS devices must still reach the downstream driver. */
    d.active = 0; d.allocate_commands = fake_allocate;
    VkCommandBufferAllocateInfo alloc = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandBufferCount=1};
    VkCommandBuffer cb;
    assert(vkAllocateCommandBuffers(d.handle, &alloc, &cb) == VK_SUCCESS);
    bound_rate = 0; int before = bind_calls;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)(uintptr_t)999);
    assert(bind_calls == before + 1 && bound_rate == 0);
    vkFreeCommandBuffers(d.handle, VK_NULL_HANDLE, 1, &cb);
    assert(commands == &c);
    /* Fuzz bounded parser operands/instruction lengths under ASan+UBSan. */
    uint32_t *mutation = malloc(perspective.codeSize); assert(mutation);
    uint32_t random = 0x12345678;
    for (unsigned i=0; i<512; ++i) {
        memcpy(mutation,perspective.pCode,perspective.codeSize);
        random = random * 1664525u + 1013904223u;
        unsigned at = 5 + random % (perspective.codeSize / 4 - 5);
        random = random * 1664525u + 1013904223u; mutation[at] = random;
        VkShaderModuleCreateInfo fuzz = perspective; fuzz.pCode = mutation;
        (void)shader_flags(&fuzz);
    }
    free(mutation);
    free((void*)perspective.pCode); free((void*)checker.pCode);
    puts("VRS real-SPIR-V policy, pipeline passthrough and dynamic toggle: OK");
}
