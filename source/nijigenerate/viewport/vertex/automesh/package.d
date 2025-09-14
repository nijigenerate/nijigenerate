module nijigenerate.viewport.vertex.automesh;

public import nijigenerate.viewport.vertex.automesh.automesh;
public import nijigenerate.viewport.vertex.automesh.contours;
public import nijigenerate.viewport.vertex.automesh.grid;
public import nijigenerate.viewport.vertex.automesh.skeletonize;
public import nijigenerate.viewport.vertex.automesh.optimum;
public import nijigenerate.viewport.vertex.automesh.alpha_provider;
// Compile-time list of AutoMesh processor types
import std.meta : AliasSeq;
public alias AutoMeshProcessorTypes = AliasSeq!(
    OptimumAutoMeshProcessor,
    ContourAutoMeshProcessor,
    GridAutoMeshProcessor,
    SkeletonExtractor
);
