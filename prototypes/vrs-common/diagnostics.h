/* Snapshot counts are monotonic but not transactional across recording threads.
 * No GPU feedback is fabricated: vkCmdSetFragmentShadingRateKHR returns void,
 * and a recorded command buffer need not ever be submitted. */
#ifndef UGM_VRS_DIAGNOSTICS_H
#define UGM_VRS_DIAGNOSTICS_H
static void live_report(Device *d,int force) {
    if(!d->debug) return;
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now)) return;
    unsigned long next=atomic_load_explicit(&d->next_report_sec,memory_order_relaxed);
    if(!force) {
        if((unsigned long)now.tv_sec<next) return;
        if(!atomic_compare_exchange_strong_explicit(&d->next_report_sec,&next,
            (unsigned long)now.tv_sec+2,memory_order_relaxed,memory_order_relaxed)) return;
    }
#define COUNT(x) atomic_load_explicit(&d->x,memory_order_relaxed)
    fprintf(stderr,"UGM VRS LIVE pid=%ld device=%p route=%s policy=%s active=%d analyzed=%lu accepted=%lu injected=%lu create-failed=%lu graphics-binds=%lu requested-1x1=%lu requested-2x2=%lu recorded-1x1=%lu recorded-2x2=%lu passthrough-binds=%lu gpl-libraries=%lu gpl-shadows=%lu gpl-linked=%lu gpu-effective=unmeasured excluded-native=%lu excluded-extended=%lu excluded-stages=%lu excluded-fragment=%lu excluded-position=%lu excluded-multisample=%lu excluded-blending=%lu excluded-allocation=%lu\n",
        (long)getpid(),(void *)d->handle,UGM_VRS_VENDOR,d->audit_only ? "legacy-audit" : "generic-3",d->active,
        COUNT(analyzed),COUNT(accepted),COUNT(injected),COUNT(create_failed),COUNT(graphics_binds),
        COUNT(requested_1x1),COUNT(requested_2x2),COUNT(recorded_1x1),COUNT(recorded_2x2),COUNT(passthrough_binds),
        COUNT(gpl_libraries),COUNT(gpl_shadows),COUNT(gpl_linked),
        COUNT(excluded[NATIVE]),COUNT(excluded[EXTENDED_STATE]),COUNT(excluded[STAGES]),COUNT(excluded[FRAGMENT]),
        COUNT(excluded[POSITION]),COUNT(excluded[MULTISAMPLE]),COUNT(excluded[BLENDING]),COUNT(excluded[ALLOCATION]));
    fprintf(stderr,"UGM VRS COVERAGE pid=%ld recorded-2x1=%lu recorded-1x2=%lu unsupported-rate=%lu binds-native=%lu binds-extended=%lu binds-stages=%lu binds-fragment=%lu binds-position=%lu binds-multisample=%lu binds-blending=%lu binds-allocation=%lu binds-unknown=%lu gpu-time=unmeasured\n",
        (long)getpid(),COUNT(recorded_2x1),COUNT(recorded_1x2),COUNT(unsupported_rate),
        COUNT(bind_excluded[NATIVE]),COUNT(bind_excluded[EXTENDED_STATE]),COUNT(bind_excluded[STAGES]),
        COUNT(bind_excluded[FRAGMENT]),COUNT(bind_excluded[POSITION]),COUNT(bind_excluded[MULTISAMPLE]),
        COUNT(bind_excluded[BLENDING]),COUNT(bind_excluded[ALLOCATION]),COUNT(bind_excluded[REASON_COUNT]));
#undef COUNT
    /* Copy under the object lock, print after releasing it. No per-bind object
     * logging, addresses of shaders, application names, or shader source. */
    PolicyDetail snapshot[VRS_DETAIL_LIMIT]; unsigned count=0;
    pthread_mutex_lock(&d->objects_lock);
    for(unsigned j=0;j<d->detail_count;++j) {
        PolicyDetail *e=&d->details[j];
        if(force || e->count!=e->reported) { snapshot[count++]=*e; e->reported=e->count; }
    }
    unsigned long overflow=d->detail_overflow;
    pthread_mutex_unlock(&d->objects_lock);
    for(unsigned j=0;j<count;++j) {
        const PolicyDetail *e=&snapshot[j];
        fprintf(stderr,"UGM VRS DETAIL pid=%ld device=%p site=%s name=%s value=%llu rule=%s count=%lu\n",
            (long)getpid(),(void *)d->handle,e->site,e->name,(unsigned long long)e->value,e->rule,e->count);
    }
    if(overflow) fprintf(stderr,"UGM VRS DETAIL pid=%ld overflow=%lu limit=%u\n",(long)getpid(),overflow,VRS_DETAIL_LIMIT);
}
#endif
