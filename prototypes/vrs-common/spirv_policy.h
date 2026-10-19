/* Read-only, bounded SPIR-V policy analysis. No shader rewriting or runtime tools.
 * RADV reference: radv_shader_info.c, gather_intrinsic_store_output_info and
 * disallow_force_vrs_per_vertex (Mesa main, consulted 2026-10-09).
 * This is deliberately more conservative than optimized NIR: unsupported data
 * flow/stages stay at the application's rate, never guessed to be safe. */
#ifndef UGM_SPIRV_POLICY_H
#define UGM_SPIRV_POLICY_H
#define SHADER_VALID 1u
#define SHADER_DISCARD 2u
#define SHADER_FLAT 4u
#define SHADER_PIXEL_INPUT 8u
#define SHADER_POSITION_COARSE 16u
#define SHADER_NATIVE_RATE 32u
#define SHADER_UNSUPPORTED 64u

typedef struct SpvDefinition { uint32_t offset, position; } SpvDefinition;
typedef struct SpvCall {
    const struct SpvCall *parent;
    uint32_t parameter[16], argument[16];
    unsigned count;
} SpvCall;
typedef struct SpvPolicy {
    const uint32_t *code;
    size_t count;
    uint32_t bound;
    SpvDefinition *defs;
    unsigned budget;
    const SpvCall *call;
} SpvPolicy;
typedef struct SpvPointer {
    uint32_t root, index[8];
    unsigned count;
} SpvPointer;
enum SpvValue { SPV_UNKNOWN, SPV_ONE, SPV_OTHER, SPV_DYNAMIC, SPV_ZERO };

static const uint32_t *spv_def(SpvPolicy *s, uint32_t id) {
    return id && id < s->bound && s->defs[id].offset ? s->code + s->defs[id].offset : NULL;
}
static int spv_uint(SpvPolicy *s, uint32_t id, uint32_t *value) {
    const uint32_t *p = spv_def(s, id);
    if (!p || (p[0] & 65535) != 43 || p[0] >> 16 != 4) return 0;
    const uint32_t *type = spv_def(s, p[1]);
    if (!type || (type[0] & 65535) != 21 || type[2] != 32) return 0;
    *value = p[3]; return 1;
}
static int spv_pointer(SpvPolicy *s, uint32_t id, SpvPointer *out, unsigned depth) {
    const uint32_t *p = spv_def(s, id);
    if (!s->budget || !p || depth > 32) return 0;
    --s->budget;
    unsigned op = p[0] & 65535, n = p[0] >> 16;
    if (op == 59) { out->root = id; out->count = 0; return 1; }
    if (op == 83) return spv_pointer(s, p[3], out, depth + 1);
    if ((op != 65 && op != 66) || n < 5 || !spv_pointer(s, p[3], out, depth + 1)) return 0;
    for (unsigned i = 4; i < n; ++i) {
        if (out->count == 8 || !spv_uint(s, p[i], &out->index[out->count])) return 0;
        ++out->count;
    }
    return 1;
}
/* Root-only provenance for storage checks and external loads. Dynamic array
 * indices do not obscure the storage class; Position selectors still use the
 * stricter constant-index resolver above. Never follow arbitrary pointer casts. */
static uint32_t spv_pointer_root(SpvPolicy *s,uint32_t id,unsigned depth) {
    const uint32_t *p=spv_def(s,id);
    if(!p || !s->budget || depth>32) return 0;
    --s->budget;
    unsigned op=p[0]&65535,n=p[0]>>16;
    if(op==59) return id;
    if((op==83 && n==4) || ((op==65 || op==66) && n>=5))
        return spv_pointer_root(s,p[3],depth+1);
    return 0;
}
/* Only straight-line, SSA, store-free value helpers are admitted. Pointer
 * arguments, branches, calls, loads and unrecognized instructions stay closed.
 * This covers compiler-generated arithmetic helpers without a general IR. */
static int spv_pure_function(SpvPolicy *s,uint32_t id) {
    const uint32_t *f=spv_def(s,id);
    if(!f || (f[0]&65535)!=54) return 0;
    unsigned labels=0,returns=0,parameters=0;
    for(size_t i=(size_t)(f-s->code)+(f[0]>>16);i<s->count;i+=s->code[i]>>16) {
        if(!s->budget) return 0;
        --s->budget;
        const uint32_t *p=s->code+i; unsigned op=p[0]&65535,n=p[0]>>16;
        if(op==56) return labels==1 && returns==1;
        if(op==55) {
            const uint32_t *t=spv_def(s,p[1]);
            if(++parameters>16 || !t || (t[0]&65535)==32) return 0;
        } else if(op==248) { if(++labels!=1) return 0; }
        else if(op==254) { if(n!=2 || ++returns!=1) return 0; }
        else if(op==12) {
            const uint32_t *set=spv_def(s,p[3]);
            if(n<5 || !set || (set[0]&65535)!=11 || (set[0]>>16)!=6 ||
               memcmp(set+2,"GLSL.std.450",13)) return 0;
            /* GLSL.std.450 interpolation ops 76..78 are not arithmetic. */
            if(!p[4] || p[4]>81 || (p[4]>=76 && p[4]<=78) || p[4]==35 || p[4]==51) return 0;
        } else if(!((op>=77 && op<=84) || (op>=109 && op<=200) || op==8 || op==317)) return 0;
    }
    return 0;
}
/* BuiltIn Position can decorate a variable or a member of gl_PerVertex. */
static int spv_position_decoration(SpvPolicy *s, uint32_t id, int member) {
    if(!id || id>=s->bound) return 0;
    return s->defs[id].position == (member<0 ? 1u : (uint32_t)member+2u);
}
/* Returns 1 for a store of Position.w, 0 for an unrelated store, -1 if unclear.
 * The selector describes w inside the stored value (including whole blocks). */
static int spv_position_store(SpvPolicy *s, const SpvPointer *ptr, uint32_t *sel, unsigned *ns) {
    const uint32_t *var = spv_def(s, ptr->root);
    if (!var || (var[0] & 65535) != 59) return -1;
    if (var[3] != 3) return 0; /* Output */
    const uint32_t *type = spv_def(s, var[1]);
    if (!type || (type[0] & 65535) != 32) return -1;
    uint32_t tid = type[3];
    int position = spv_position_decoration(s, ptr->root, -1);
    for (unsigned i = 0; i < ptr->count; ++i) {
        type = spv_def(s, tid);
        if (!type) return -1;
        unsigned op = type[0] & 65535;
        if (op == 30) {
            if (ptr->index[i] + 2 >= type[0] >> 16) return -1;
            position = spv_position_decoration(s, tid, (int)ptr->index[i]);
            tid = type[2 + ptr->index[i]];
        } else if (op == 23) {
            if (ptr->index[i] >= type[3]) return -1;
            if (position) {
                if (i + 1 != ptr->count) return -1;
                *ns = 0; return ptr->index[i] == 3;
            }
            tid = type[2];
        } else return -1;
    }
    type = spv_def(s, tid);
    if (!type) return -1;
    *ns = 0;
    if ((type[0] & 65535) == 30) {
        for (unsigned m = 0; m + 2 < type[0] >> 16; ++m) {
            if (spv_position_decoration(s, tid, (int)m)) {
                sel[(*ns)++] = m; type = spv_def(s, type[m + 2]); position = 1; break;
            }
        }
    }
    if (!position) return 0;
    if (!type || (type[0] & 65535) != 23 || type[3] != 4) return -1;
    sel[(*ns)++] = 3; return 1;
}
static enum SpvValue spv_merge(enum SpvValue a, enum SpvValue b) {
    if (a == SPV_UNKNOWN || b == SPV_UNKNOWN) return SPV_UNKNOWN;
    if(a==b) return a;
    /* A path with W=1 may be UI even if another path has perspective. */
    return a==SPV_ONE || b==SPV_ONE ? SPV_UNKNOWN : SPV_DYNAMIC;
}
static enum SpvValue spv_value(SpvPolicy *s, uint32_t id, const uint32_t *sel, unsigned ns, unsigned depth) {
    const uint32_t *p = spv_def(s, id);
    if (!s->budget || !p || depth > 32 || ns > 8) return SPV_UNKNOWN;
    --s->budget;
    unsigned op = p[0] & 65535, n = p[0] >> 16;
    if (op == 43 && !ns && n == 4) { /* ordinary float32 constant, not specialization */
        const uint32_t *t = spv_def(s, p[1]);
        if (!t || (t[0] & 65535) != 22 || t[2] != 32) return SPV_UNKNOWN;
        return p[3] == 0x3f800000u ? SPV_ONE : !(p[3]&0x7fffffffu) ? SPV_ZERO : SPV_OTHER;
    }
    if (op == 46) return SPV_ZERO; /* OpConstantNull */
    if(op==55 && s->call) {
        const SpvCall *frame=s->call;
        for(unsigned j=0;j<frame->count;++j) if(frame->parameter[j]==id) {
            s->call=frame->parent;
            enum SpvValue value=spv_value(s,frame->argument[j],sel,ns,depth+1);
            s->call=frame; return value;
        }
        return SPV_UNKNOWN;
    }
    if(op==57 && n>=4 && spv_pure_function(s,p[3])) {
        const uint32_t *f=spv_def(s,p[3]);
        SpvCall frame={.parent=s->call}; uint32_t returned=0;
        for(size_t i=(size_t)(f-s->code)+(f[0]>>16);i<s->count;i+=s->code[i]>>16) {
            if(!s->budget) return SPV_UNKNOWN;
            --s->budget;
            const uint32_t *q=s->code+i; unsigned opcode=q[0]&65535;
            if(opcode==56) break;
            if(opcode==55) {
                if(frame.count>=16 || frame.count+4>=n) return SPV_UNKNOWN;
                frame.parameter[frame.count]=q[2]; frame.argument[frame.count]=p[4+frame.count]; ++frame.count;
            }
            if(opcode==254) returned=q[1];
        }
        if(frame.count+4!=n || !returned) return SPV_UNKNOWN;
        s->call=&frame; enum SpvValue value=spv_value(s,returned,sel,ns,depth+1);
        s->call=frame.parent; return value;
    }
    if (op == 83 && n == 4) return spv_value(s, p[3], sel, ns, depth + 1);
    if ((op == 44 || op == 80) && ns) { /* composite, including vec3 + scalar */
        const uint32_t *t = spv_def(s, p[1]);
        if (!t) return SPV_UNKNOWN;
        uint32_t component = sel[0];
        if ((t[0] & 65535) == 23) {
            for (unsigned i = 3; i < n; ++i) {
                const uint32_t *v = spv_def(s, p[i]);
                const uint32_t *vt = v ? spv_def(s, v[1]) : NULL;
                if (!vt) return SPV_UNKNOWN;
                unsigned width = (vt[0] & 65535) == 23 ? vt[3] : 1;
                if (component < width) {
                    if (width == 1) return spv_value(s, p[i], sel + 1, ns - 1, depth + 1);
                    return spv_value(s, p[i], &component, 1, depth + 1);
                }
                component -= width;
            }
        } else if (component < n - 3) return spv_value(s, p[3 + component], sel + 1, ns - 1, depth + 1);
        return SPV_UNKNOWN;
    }
    if (op == 81 && n >= 5) { /* CompositeExtract */
        uint32_t path[8]; unsigned k = n - 4;
        if (k + ns > 8) return SPV_UNKNOWN;
        memcpy(path, p + 4, k * sizeof(*path));
        if (ns) memcpy(path + k, sel, ns * sizeof(*path));
        return spv_value(s, p[3], path, k + ns, depth + 1);
    }
    if (op == 82 && n >= 6) { /* CompositeInsert */
        unsigned k = n - 5;
        if (ns >= k && !memcmp(sel, p + 5, k * sizeof(*sel))) return spv_value(s, p[3], sel + k, ns - k, depth + 1);
        if (ns < k) return SPV_UNKNOWN;
        return spv_value(s, p[4], sel, ns, depth + 1);
    }
    if (op == 79 && n >= 6 && ns == 1 && sel[0] < n - 5) { /* VectorShuffle */
        const uint32_t *v = spv_def(s, p[3]);
        const uint32_t *t = v ? spv_def(s, v[1]) : NULL;
        if (!t || (t[0] & 65535) != 23) return SPV_UNKNOWN;
        uint32_t k = p[5 + sel[0]];
        if (k == UINT32_MAX) return SPV_UNKNOWN;
        uint32_t source = p[3];
        if (k >= t[3]) { k -= t[3]; source = p[4]; }
        return spv_value(s, source, &k, 1, depth + 1);
    }
    if (op == 169 && n == 6) return spv_merge(spv_value(s, p[4], sel, ns, depth + 1), spv_value(s, p[5], sel, ns, depth + 1));
    if (op == 245 && n >= 5 && n % 2 == 1) {
        enum SpvValue result = spv_value(s, p[3], sel, ns, depth + 1);
        for (unsigned i = 5; i < n; i += 2) result = spv_merge(result, spv_value(s, p[i], sel, ns, depth + 1));
        return result;
    }
    if (op == 61 && n >= 4) { /* Load: follow direct local stores, not arbitrary aliasing. */
        uint32_t root=spv_pointer_root(s,p[3],0);
        const uint32_t *v=spv_def(s,root);
        if(!v) return SPV_UNKNOWN;
        if (v[3] == 1 || v[3] == 2 || v[3] == 9 || v[3] == 12) return SPV_DYNAMIC;
        SpvPointer ptr;
        if (!spv_pointer(s, p[3], &ptr, 0)) return SPV_UNKNOWN;
        if (v[3] != 7) return SPV_UNKNOWN;
        enum SpvValue result = SPV_UNKNOWN; int found = 0;
        for (size_t i = 5; i < s->count; i += s->code[i] >> 16) {
            if (!s->budget) return SPV_UNKNOWN;
            --s->budget;
            const uint32_t *store = s->code + i; SpvPointer dst;
            if ((store[0] & 65535) != 62 || !spv_pointer(s, store[1], &dst, 0) || dst.root != ptr.root) continue;
            if (dst.count > ptr.count || memcmp(dst.index, ptr.index, dst.count * sizeof(uint32_t))) return SPV_UNKNOWN;
            uint32_t path[8]; unsigned k = ptr.count - dst.count;
            if (k + ns > 8) return SPV_UNKNOWN;
            memcpy(path, ptr.index + dst.count, k * sizeof(uint32_t));
            if (ns) memcpy(path + k, sel, ns * sizeof(uint32_t));
            enum SpvValue value = spv_value(s, store[2], path, k + ns, depth + 1);
            result = found ? spv_merge(result, value) : value; found = 1;
        }
        return result;
    }
    /* Common perspective transform. Require an actual uniform/input matrix,
     * not just an arithmetic opcode (constant arithmetic can produce W=1).
     * A uniform orthographic matrix remains indistinguishable from perspective
     * without runtime data: this is still the documented RADV-inspired heuristic. */
    if(op==145 && n==5 && ns==1 && sel[0]<4) {
        const uint32_t *matrix=spv_def(s,p[3]);
        SpvPointer ptr;
        if(matrix && (matrix[0]&65535)==61 && spv_pointer(s,matrix[3],&ptr,0)) {
            const uint32_t *root=spv_def(s,ptr.root);
            if(root && (root[3]==1 || root[3]==2 || root[3]==9 || root[3]==12)) return SPV_DYNAMIC;
        }
    }
    if(op==148 && n==5 && !ns) { /* Dot-product perspective form (e.g. HLSL). */
        int dependent[2]={0,0};
        for(unsigned j=0;j<2;++j) {
            const uint32_t *v=spv_def(s,p[3+j]);
            const uint32_t *type=v ? spv_def(s,v[1]) : NULL;
            if(!type || (type[0]&65535)!=23 || type[3]>4) return SPV_UNKNOWN;
            for(uint32_t k=0;k<type[3];++k) {
                enum SpvValue value=spv_value(s,p[3+j],&k,1,depth+1);
                if(value==SPV_UNKNOWN) return SPV_UNKNOWN;
                dependent[j]|=value==SPV_DYNAMIC;
            }
        }
        if(dependent[0] && dependent[1]) return SPV_DYNAMIC;
    }
    /* Scalarized transforms emitted by HLSL compilers (FAdd/FMul/Fma).
     * This is the same dynamic-W heuristic as MatrixTimesVector, not proof
     * that runtime uniforms cannot encode an orthographic/UI transform.
     * Unknown/constant arithmetic, zero products and direct cancellation are
     * not promoted to perspective. No arbitrary ExtInst is considered safe. */
    if((op==129 || op==131 || op==133) && n==5) {
        if(op==131 && p[3]==p[4]) return SPV_ZERO;
        enum SpvValue a=spv_value(s,p[3],sel,ns,depth+1);
        enum SpvValue b=spv_value(s,p[4],sel,ns,depth+1);
        if(a==SPV_UNKNOWN || b==SPV_UNKNOWN) return SPV_UNKNOWN;
        if(op==133 && (a==SPV_ZERO || b==SPV_ZERO)) return SPV_ZERO;
        if(b==SPV_ZERO && (op==129 || op==131)) return a;
        if(a==SPV_ZERO && op==129) return b;
        if(op==133 && b==SPV_ONE) return a;
        if(op==133 && a==SPV_ONE) return b;
        if(a==SPV_DYNAMIC || b==SPV_DYNAMIC) return SPV_DYNAMIC;
        return SPV_UNKNOWN;
    }
    if(op==12 && n==8 && p[4]==50) { /* GLSL.std.450 Fma */
        const uint32_t *set=spv_def(s,p[3]);
        if(!set || (set[0]&65535)!=11 || (set[0]>>16)!=6 || memcmp(set+2,"GLSL.std.450",13)) return SPV_UNKNOWN;
        enum SpvValue a=spv_value(s,p[5],sel,ns,depth+1);
        enum SpvValue b=spv_value(s,p[6],sel,ns,depth+1);
        enum SpvValue c=spv_value(s,p[7],sel,ns,depth+1);
        if(a==SPV_UNKNOWN || b==SPV_UNKNOWN || c==SPV_UNKNOWN) return SPV_UNKNOWN;
        if(a==SPV_ZERO || b==SPV_ZERO) return c;
        if(a==SPV_DYNAMIC || b==SPV_DYNAMIC || c==SPV_DYNAMIC) return SPV_DYNAMIC;
        return SPV_UNKNOWN;
    }
    /* Identity arithmetic recognized without floating-point folding. */
    if((op==133 || op==136) && n==5) {
        enum SpvValue a=spv_value(s,p[3],sel,ns,depth+1);
        enum SpvValue b=spv_value(s,p[4],sel,ns,depth+1);
        if(b==SPV_ONE) return a;
        if(op==133 && a==SPV_ONE) return b;
    }
    return SPV_UNKNOWN;
}
typedef void (*SpvReport)(void *,const char *,uint32_t);
static unsigned shader_flags_report(const VkShaderModuleCreateInfo *info,SpvReport report,void *context) {
#define REPORT(reason,value) do { if(report) report(context,reason,value); } while(0)
    if (!info || !info->pCode || info->codeSize < 20 || info->codeSize % 4 || info->codeSize > 16 * 1024 * 1024) return 0;
    SpvPolicy s = {.code = info->pCode, .count = info->codeSize / 4, .bound = info->pCode[3], .budget = 200000};
    if (s.code[0] != 0x07230203 || s.code[4] || !s.bound || s.bound > 1048576) return 0;
    s.defs = calloc(s.bound, sizeof(*s.defs));
    if (!s.defs) return 0;
    unsigned flags = SHADER_VALID, entries = 0, model = UINT32_MAX;
    for (size_t i = 5; i < s.count;) {
        const uint32_t *p = s.code + i; unsigned n = p[0] >> 16, op = p[0] & 65535;
        if (!n || n > s.count - i) { flags = 0; break; }
        unsigned result = 0, min = 1;
        if ((op >= 19 && op <= 39) || op==11) { result = 1; min = 2; }
        if ((op >= 41 && op <= 52) || op == 59 || op == 61 || op == 65 || op == 66 ||
            (op >= 77 && op <= 84) || (op >= 109 && op <= 200) || op == 245 || op==12 || op==54 || op==55 || op==57) { result = 2; min = 4; }
        if (op == 41 || op == 42 || op == 46 || op == 48 || op == 49 || op==55) min = 3;
        if(op==54 || op==12) min=5;
        if (op == 21 || op == 23 || op == 32) min = 4;
        if (op == 22) min = 3;
        if (op == 62) min = 3;
        if (op == 71) min = 3;
        if (op == 72) min = 4;
        if (op == 15) min = 4;
        if (n < min) { flags = 0; break; }
        if (result) {
            if (!p[result] || p[result] >= s.bound || s.defs[p[result]].offset) { flags = 0; break; }
            s.defs[p[result]].offset = (uint32_t)i;
        }
        if (op == 15) { entries++; model = p[1]; }
        if (op == 71 || op == 72) {
            unsigned at = op == 71 ? 2 : 3;
            if (p[at] == 14) flags |= SHADER_FLAT;
            if (p[at] == 11) {
                if (n <= at + 1) { flags = 0; break; }
                unsigned builtin = p[at + 1];
                if(builtin==0) {
                    if(!p[1] || p[1]>=s.bound || (op==72 && p[2]>INT32_MAX-2u)) { flags=0; break; }
                    s.defs[p[1]].position=op==71 ? 1u : p[2]+2u;
                }
                if (builtin == 15 || builtin == 16 || builtin == 18 || builtin == 19 || builtin == 20) flags |= SHADER_PIXEL_INPUT;
                if (builtin == 4432 || builtin == 4444) flags |= SHADER_NATIVE_RATE;
            }
        }
        if (op == 252 || op == 4416 || op == 5380) flags |= SHADER_DISCARD;
        /* Decoration groups, alias/copy memory and pointer calls need a full IR.
         * A graphics library/mesh pipeline is not guessed from incomplete data. */
        if (op == 73 || op == 74 || op == 75 || op == 63 || op == 64 || op == 67) { flags |= SHADER_UNSUPPORTED; REPORT("unmodeled-memory-or-decoration-opcode",op); }
        i += n;
    }
    /* Side effects, pixel-sensitive operations, and subgroup/interlock work
     * cannot safely be reduced to one invocation for a 2x2 area. */
    if(flags & SHADER_VALID) for(size_t i=5;i<s.count;i+=s.code[i]>>16) {
        const uint32_t *p=s.code+i; unsigned op=p[0]&65535,n=p[0]>>16;
        if(op==17) { /* Capability whitelist: future shader semantics fail closed. */
            if(n!=2) { flags=0; break; }
            switch(p[1]) {
            case 0: case 1: case 9: case 10: case 11: case 22: case 25:
            case 28: case 29: case 30: case 31: case 32: case 33: case 34:
            case 36: case 37: case 39: case 42: case 43: case 44: case 45:
            case 46: case 47: case 48: case 49: case 50: case 57: case 69: case 70: case 71:
            case 4433: case 4434: case 4435: case 4436: case 4437: case 4439:
            /* Capabilities declare available semantics, not their use. Vulkan's
             * memory model, address representation and float preservation do
             * not by themselves make a shader pixel-sensitive. Actual stores,
             * atomics, barriers and demote instructions remain checked below. */
            case 4427: /* DrawParameters (vertex inputs) */
            case 4466: /* SignedZeroInfNanPreserve */
            case 5345: /* VulkanMemoryModel */
            case 5347: /* PhysicalStorageBufferAddresses */
            case 5379: /* DemoteToHelperInvocation; actual demote stays excluded */
            case 4448: case 4449: case 4450: case 5301: case 5302: case 5304:
            case 5305: case 5306: case 5307: case 5308: case 5309: case 5311: case 5312:
                break;
            default: flags|=SHADER_UNSUPPORTED; REPORT("unsupported-capability",p[1]);
            }
        }
        if(op==57 && model==0 && !spv_pure_function(&s,p[3])) { flags|=SHADER_UNSUPPORTED; REPORT("unmodeled-vertex-function-call",op); }
        if(model!=4) continue;
        if(op==99 || op==95 || op==98 || (op>=207 && op<=215) ||
           op==224 || op==225 || (op>=227 && op<=242) || op==318 || op==319 ||
           (op>=333 && op<=366) || op==4431 || op==5110 || op==5111 || op==5296 ||
           op==5364 || op==5365 || op==5614 || op==5615 || op==6035)
            { flags|=SHADER_UNSUPPORTED; REPORT("pixel-sensitive-or-side-effect-opcode",op); }
        if(op==62) {
            uint32_t id=spv_pointer_root(&s,p[1],0);
            if(!id) { flags|=SHADER_UNSUPPORTED; REPORT("unresolved-store-pointer",op); }
            else {
                const uint32_t *root=spv_def(&s,id);
                if(!root || (root[3]!=3 && root[3]!=6 && root[3]!=7)) { flags|=SHADER_UNSUPPORTED; REPORT("external-store-storage-class",root ? root[3] : UINT32_MAX); }
            }
        }
        if(op==71 || op==72) {
            unsigned at=op==71 ? 2 : 3;
            /* NoPerspective/Centroid/Sample interpolation is pixel sensitive. */
            if(p[at]==13 || p[at]==16 || p[at]==17) flags|=SHADER_PIXEL_INPUT;
            if(p[at]==11 && n>at+1 && (p[at+1]==22 || p[at+1]==23 ||
               p[at+1]==5014 || (p[at+1]>=4416 && p[at+1]<=4420))) flags|=SHADER_PIXEL_INPUT;
        }
    }
    if (entries != 1 || (model != 0 && model != 4)) flags |= SHADER_UNSUPPORTED;
    if ((flags & SHADER_VALID) && !(flags & SHADER_UNSUPPORTED) && model == 0) {
        int saw_w = 0, coarse = 0, unclear = 0, saw_one = 0;
        for (size_t i = 5; i < s.count; i += s.code[i] >> 16) {
            const uint32_t *p = s.code + i;
            if ((p[0] & 65535) != 62) continue;
            SpvPointer ptr; uint32_t sel[8]; unsigned ns;
            if (!spv_pointer(&s, p[1], &ptr, 0)) {
                const uint32_t *root=spv_def(&s,spv_pointer_root(&s,p[1],0));
                if(!root || root[3]==3) unclear=1;
                continue;
            }
            int position = spv_position_store(&s, &ptr, sel, &ns);
            if (position < 0) unclear = 1;
            if (position != 1) continue;
            saw_w = 1;
            enum SpvValue value = spv_value(&s, p[2], sel, ns, 0);
            if (value == SPV_UNKNOWN) unclear = 1;
            if (value == SPV_ONE) saw_one = 1;
            if (value == SPV_OTHER || value == SPV_DYNAMIC) coarse = 1;
        }
        if (saw_w && coarse && !unclear && !saw_one && s.budget) flags |= SHADER_POSITION_COARSE;
    }
    if(model==0 && !(flags&SHADER_POSITION_COARSE)) REPORT("position-w-not-proven",flags);
    free(s.defs);
#undef REPORT
    return flags;
}
static unsigned shader_flags(const VkShaderModuleCreateInfo *info) {
    return shader_flags_report(info,NULL,NULL);
}
#endif
