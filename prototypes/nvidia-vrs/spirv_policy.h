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

typedef struct SpvPolicy {
    const uint32_t *code;
    size_t count;
    uint32_t bound;
    size_t *defs;
    unsigned budget;
} SpvPolicy;
typedef struct SpvPointer {
    uint32_t root, index[8];
    unsigned count;
} SpvPointer;
enum SpvValue { SPV_UNKNOWN, SPV_ONE, SPV_OTHER, SPV_DYNAMIC };

static const uint32_t *spv_def(SpvPolicy *s, uint32_t id) {
    return id && id < s->bound && s->defs[id] ? s->code + s->defs[id] : NULL;
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
/* BuiltIn Position can decorate a variable or a member of gl_PerVertex. */
static int spv_position_decoration(SpvPolicy *s, uint32_t id, int member) {
    for (size_t i = 5; i < s->count; i += s->code[i] >> 16) {
        if (!s->budget) return 0;
        --s->budget;
        const uint32_t *p = s->code + i;
        unsigned op = p[0] & 65535, n = p[0] >> 16;
        if (member < 0 && op == 71 && n == 4 && p[1] == id && p[2] == 11 && p[3] == 0) return 1;
        if (member >= 0 && op == 72 && n == 5 && p[1] == id && p[2] == (uint32_t)member && p[3] == 11 && p[4] == 0) return 1;
    }
    return 0;
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
    return a == b ? a : SPV_DYNAMIC;
}
static enum SpvValue spv_value(SpvPolicy *s, uint32_t id, const uint32_t *sel, unsigned ns, unsigned depth) {
    const uint32_t *p = spv_def(s, id);
    if (!s->budget || !p || depth > 32 || ns > 8) return SPV_UNKNOWN;
    --s->budget;
    unsigned op = p[0] & 65535, n = p[0] >> 16;
    if (op == 43 && !ns && n == 4) { /* ordinary float32 constant, not specialization */
        const uint32_t *t = spv_def(s, p[1]);
        if (!t || (t[0] & 65535) != 22 || t[2] != 32) return SPV_UNKNOWN;
        return p[3] == 0x3f800000u ? SPV_ONE : SPV_OTHER;
    }
    if (op == 46) return SPV_OTHER; /* OpConstantNull */
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
        SpvPointer ptr;
        if (!spv_pointer(s, p[3], &ptr, 0)) return SPV_UNKNOWN;
        const uint32_t *v = spv_def(s, ptr.root);
        if (v[3] == 1 || v[3] == 2 || v[3] == 9 || v[3] == 12) return SPV_DYNAMIC;
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
    /* Data-dependent arithmetic is the same unknown-W case RADV accepts.
     * We do not pretend to run NIR's constant folding here. */
    if ((op >= 126 && op <= 152) || (op >= 109 && op <= 124)) return SPV_DYNAMIC;
    return SPV_UNKNOWN;
}
static unsigned shader_flags(const VkShaderModuleCreateInfo *info) {
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
        if (op >= 19 && op <= 39) { result = 1; min = 2; }
        if ((op >= 41 && op <= 52) || op == 59 || op == 61 || op == 65 || op == 66 ||
            (op >= 77 && op <= 84) || (op >= 109 && op <= 200) || op == 245) { result = 2; min = 4; }
        if (op == 41 || op == 42 || op == 46 || op == 48 || op == 49) min = 3;
        if (op == 21 || op == 23 || op == 32) min = 4;
        if (op == 22) min = 3;
        if (op == 62) min = 3;
        if (op == 71) min = 3;
        if (op == 72) min = 4;
        if (op == 15) min = 4;
        if (n < min) { flags = 0; break; }
        if (result) {
            if (!p[result] || p[result] >= s.bound || s.defs[p[result]]) { flags = 0; break; }
            s.defs[p[result]] = i;
        }
        if (op == 15) { entries++; model = p[1]; }
        if (op == 71 || op == 72) {
            unsigned at = op == 71 ? 2 : 3;
            if (p[at] == 14) flags |= SHADER_FLAT;
            if (p[at] == 11) {
                if (n <= at + 1) { flags = 0; break; }
                unsigned builtin = p[at + 1];
                if (builtin == 15 || builtin == 18 || builtin == 19 || builtin == 20) flags |= SHADER_PIXEL_INPUT;
                if (builtin == 4432 || builtin == 4444) flags |= SHADER_NATIVE_RATE;
            }
        }
        if (op == 252 || op == 4416 || op == 5380) flags |= SHADER_DISCARD;
        /* Decoration groups, alias/copy memory and pointer calls need a full IR.
         * A graphics library/mesh pipeline is not guessed from incomplete data. */
        if (op == 73 || op == 74 || op == 75 || op == 63 || op == 64 || op == 57 || op == 67) flags |= SHADER_UNSUPPORTED;
        i += n;
    }
    if (entries != 1 || (model != 0 && model != 4)) flags |= SHADER_UNSUPPORTED;
    if ((flags & SHADER_VALID) && !(flags & SHADER_UNSUPPORTED) && model == 0) {
        int saw_w = 0, coarse = 0, unclear = 0;
        for (size_t i = 5; i < s.count; i += s.code[i] >> 16) {
            const uint32_t *p = s.code + i;
            if ((p[0] & 65535) != 62) continue;
            SpvPointer ptr; uint32_t sel[8]; unsigned ns;
            if (!spv_pointer(&s, p[1], &ptr, 0)) { unclear = 1; continue; }
            int position = spv_position_store(&s, &ptr, sel, &ns);
            if (position < 0) unclear = 1;
            if (position != 1) continue;
            saw_w = 1;
            enum SpvValue value = spv_value(&s, p[2], sel, ns, 0);
            if (value == SPV_UNKNOWN) unclear = 1;
            if (value == SPV_OTHER || value == SPV_DYNAMIC) coarse = 1;
        }
        if (saw_w && coarse && !unclear && s.budget) flags |= SHADER_POSITION_COARSE;
    }
    free(s.defs);
    return flags;
}
#endif
