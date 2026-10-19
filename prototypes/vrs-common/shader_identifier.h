/* Per-device, bounded observations of opaque driver identifiers. No disk cache,
 * no shader retention, no game names; unknown IDs always remain ineligible.
 * Exact bytes+length are compared even after a hash collision. Source shader
 * destruction does not invalidate immutable observations in this device. */
#ifndef UGM_VRS_SHADER_IDENTIFIER_H
#define UGM_VRS_SHADER_IDENTIFIER_H
static unsigned shader_proof_hash(uint32_t size,const uint8_t *id) {
    uint32_t hash=2166136261u;
    for(uint32_t j=0;j<size;++j) hash=(hash^id[j])*16777619u;
    return (hash^size)&(VRS_IDENTIFIER_LIMIT-1u);
}
static int shader_proof_find(Device *d,uint32_t size,const void *identifier,unsigned *flags) {
    if(!size || size>VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT || !identifier) return 0;
    unsigned slot=shader_proof_hash(size,identifier); int found=0;
    pthread_mutex_lock(&d->objects_lock);
    for(unsigned n=0;d->shader_proofs && n<VRS_IDENTIFIER_LIMIT;++n) {
        const ShaderProof *p=&d->shader_proofs[(slot+n)&(VRS_IDENTIFIER_LIMIT-1u)];
        if(!p->size) break;
        if(p->size==size && !memcmp(p->identifier,identifier,size)) { *flags=p->flags; found=1; break; }
    }
    pthread_mutex_unlock(&d->objects_lock); return found;
}
static void shader_proof_store(Device *d,uint32_t size,const void *identifier,unsigned flags) {
    if(!size || size>VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT || !identifier) return;
    unsigned slot=shader_proof_hash(size,identifier); int stored=0;
    pthread_mutex_lock(&d->objects_lock);
    if(!d->shader_proofs) d->shader_proofs=calloc(VRS_IDENTIFIER_LIMIT,sizeof(*d->shader_proofs));
    for(unsigned n=0;d->shader_proofs && n<VRS_IDENTIFIER_LIMIT;++n) {
        ShaderProof *p=&d->shader_proofs[(slot+n)&(VRS_IDENTIFIER_LIMIT-1u)];
        if(!p->size) {
            p->size=size; p->flags=flags; memcpy(p->identifier,identifier,size); stored=1; break;
        }
        if(p->size==size && !memcmp(p->identifier,identifier,size)) {
            /* Any disagreement stays invalid; never promote a cached proof. */
            if(p->flags!=flags) p->flags=0;
            stored=1; break;
        }
    }
    pthread_mutex_unlock(&d->objects_lock);
    if(!stored) policy_detail(d,"shader-identifier","capacity",VRS_IDENTIFIER_LIMIT,"proof-cache-full-or-allocation-failed");
}
#endif
