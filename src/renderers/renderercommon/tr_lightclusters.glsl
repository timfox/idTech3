// Shared clustered lighting buffer definitions
// Bindings must match CPU upload (headers: binding 6, indices: binding 7)

struct ClusterHeader {
    int lightOffset;
    int lightCount;
};

layout(std430, binding = 6) readonly buffer ClusterHeaders {
    ClusterHeader headers[];
};

layout(std430, binding = 7) readonly buffer ClusterIndices {
    int lightIndices[];
};

// Helper: compute cluster index from screen coords (pixel) and slice
int LC_ClusterIndex(int tilesX, int tilesY, int slice, int tileX, int tileY) {
    return (slice * tilesY + tileY) * tilesX + tileX;
}

// Iterate lights for a cluster; caller supplies tilesX, tilesY, slice, tileX, tileY.
// Returns [offset, count] into lightIndices.
ivec2 LC_GetClusterRange(int tilesX, int tilesY, int slice, int tileX, int tileY) {
    int idx = LC_ClusterIndex(tilesX, tilesY, slice, tileX, tileY);
    ClusterHeader h = headers[idx];
    return ivec2(h.lightOffset, h.lightCount);
}


