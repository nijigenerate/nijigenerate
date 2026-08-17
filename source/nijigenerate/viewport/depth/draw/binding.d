module nijigenerate.viewport.depth.draw.binding;

enum DepthMergePolicy {
    Replace,
    Frontmost,
    Backmost,
    Add,
    KeepExistingWhereMissing,
}

struct DepthDrawBinding {
    string layerId;
    ulong targetNodeUuid;
    ulong targetGridUuid;

    int order;
    bool enabled = true;
    bool useNormalLayerAlpha = true;
    float coverageThreshold = 0.01f;
    DepthMergePolicy mergePolicy = DepthMergePolicy.Frontmost;

    bool appliesTo(ulong gridUuid) const {
        return enabled && targetGridUuid == gridUuid && layerId.length > 0;
    }
}
