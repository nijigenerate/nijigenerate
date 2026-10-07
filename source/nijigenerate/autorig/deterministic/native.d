module nijigenerate.autorig.deterministic.native;

import nijigenerate.autorig.framework : AutoRigTaskContext;
import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.observation;
import nijigenerate.autorig.deterministic.controls;
import nijigenerate.autorig.deterministic.evidence : ngRigMaterialFeature;
import nijigenerate.autorig.deterministic.geometry;
import nijigenerate.autorig.deterministic.registered : ngRigSampleRegisteredDepth;
import nijigenerate.viewport.vertex.automesh.common : getAlphaInput;
import std.digest.sha : sha256Of;
import std.digest : toHexString;
import nijigenerate.commands.base : Context, Command, CommandResult, ExCommandResult, CreateResult, ngRunCommand;
import nijigenerate.commands.puppet.view : CaptureLiveScreenshotCommand;
import nijigenerate.commands.viewport.control : FitViewportToModelCommand;
import nijigenerate.commands.node.node : InsertNodeCommandT, AddNodeCommandT, ConvertToCommandT, MoveNodeCommand;
import nijigenerate.commands.model.set_deform_binding : SetDeformBindingCommand;
import nijigenerate.commands.binding.binding : RemoveBindingCommand;
import nijigenerate.commands.parameter.param : Add2DParameterCommand;
import nijigenerate.commands.inspector.apply_node : NINode, ScaleXCommand, ScaleYCommand,
    TranslationXCommand, TranslationYCommand, TranslationZCommand, RotationZCommand, ZSortCommand, LockToRootCommand;
import nijigenerate : ModelEditSubMode;
import nijigenerate.project : ngModelEditSubMode;
import nijigenerate.commands.node.mask : AddMaskCommand;
import nijigenerate.commands.node.welding : AddWeldingCommand;
import nijigenerate.commands.depth.bone;
import nijigenerate.commands.depth.map : SetDepthsCommand;
import nijigenerate.commands.automesh.dynamic : ApplyAutoMeshPT;
import nijigenerate.viewport.vertex.automesh.optimum : OptimumAutoMeshProcessor;
import nijigenerate.viewport.vertex.automesh.grid : GridAutoMeshProcessor;
import nijigenerate.viewport.vertex.automesh.meta : AMProcessor;
import nijigenerate.project : incActivePuppet, ngRestorePuppetMemory;
import nijigenerate.core.actionstack : incActionPushGroup, incActionPopGroup;
import nijigenerate.ext : ExPart, ExPuppet;
import nijigenerate.ext.nodes.exdepthbone : ExDepthRigRoot;
import nijilive : Node, Puppet, Part, Composite, Deformable, inWriteINPPuppetMemory, inGetCamera;
import nijigenerate.viewport.base : incViewportTargetPosition, incViewportTargetZoom, incViewportZoom;
import nijilive.core.nodes.composite.projectable : Projectable;
import nijilive.math : vec2, vec2u, vec3, vec4, mat4;
import nijilive : MaskingMode, BlendMode;
import nijilive.core.param.binding : DeformationParameterBinding, ValueParameterBinding;
import nijilive.fmt.serialize : inToJson;
import nijigenerate.autorig.json : parseJSON = ngParseAutoRigJson;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import std.exception : enforce;
import std.json : JSONValue, JSONType;
import nijilive : Parameter;
import std.math : isFinite, abs, ceil, sqrt, rint;
import std.algorithm : min, max, canFind;
import std.string : startsWith, endsWith;
import std.base64 : Base64;
import core.thread : Thread;

// Accessed exclusively on the editor thread; distinguishes retries from user edits.
private string[string] liveRigSignatures;

@AMProcessor("autorig-part", "AutoRig Part", 500)
private class RigPartMeshProcessor : OptimumAutoMeshProcessor {
    this(float spacing, float divisions = 12, float[] scales = null, bool shoulder = false) {
        MIN_DISTANCE = spacing;
        MASK_THRESHOLD = 1;
        DIV_PER_PART = divisions;
        LARGE_THRESHOLD = 400; LENGTH_THRESHOLD = 100; RATIO_THRESHOLD = .2;
        SHARP_EXPANSION_FACTOR = .01; NONSHARP_EXPANSION_FACTOR = .05;
        NONSHARP_CONTRACTION_FACTOR = .05; SCALES = [.5,0.];
        if (scales.length) SCALES = scales.dup;
        if (shoulder) { LARGE_THRESHOLD = 50; LENGTH_THRESHOLD = 20; RATIO_THRESHOLD = .01; }
    }
}

private uint uuid(JSONValue value) {
    double number = ngRigNumber(value);
    enforce(number>=0 && number<=uint.max && number==cast(uint)number, "Invalid native rig UUID");
    return cast(uint)number;
}

private float[] nativeNumbers(JSONValue value) {
    float[] result;
    foreach (number; ngRigNumbers(value)) {
        enforce(abs(number)<=float.max, "Rig coordinate exceeds native range"); result ~= cast(float)number;
    }
    return result;
}

private Context editorContext(Node[] nodes = null) {
    enforce(Thread.getThis is null || Thread.getThis.isMainThread, "Rig editor access requires the main thread");
    auto context = new Context(); context.puppet = incActivePuppet();
    if (nodes !is null) context.nodes = nodes;
    return context;
}

private Parameter parameterByName(Puppet puppet, string name) {
    foreach (parameter; puppet.parameters) if (parameter.name == name) return parameter;
    throw new Exception("Rig parameter not found: " ~ name);
}

private CommandResult command(Command action, Context context) {
    auto result = ngRunCommand(action,context);
    enforce(result !is null && result.succeeded, result is null ? "Native rig command returned no result" : result.message);
    return result;
}

private Node create(Command action, Context context) {
    auto result = cast(CreateResult!Node)command(action,context);
    enforce(result !is null && result.created.length == 1, "Native rig creation returned an unexpected result");
    return result.created[0];
}

private Point2 toRoot(Node node, double x, double y) {
    auto matrix = node.puppet.root.transform.matrix.inverse * node.transform.matrix;
    auto p = matrix*vec4(cast(float)x,cast(float)y,0,1);
    return [p.x,p.y];
}

private JSONValue textureMapping(Part part) {
    enforce(Thread.getThis is null || Thread.getThis.isMainThread, "Rig mesh snapshot requires the main thread");
    auto mesh = part.getMesh();
    JSONValue[] coordinates, texture, triangles;
    foreach (v; mesh.vertices) coordinates ~= JSONValue([v.x,v.y]);
    foreach (uv; mesh.uvs) texture ~= JSONValue([uv.x,uv.y]);
    enforce(mesh.indices.length % 3 == 0, "Invalid native triangle index count");
    foreach (i; 0 .. mesh.indices.length/3)
        triangles ~= JSONValue([mesh.indices[i*3],mesh.indices[i*3+1],mesh.indices[i*3+2]]);
    return JSONValue(["vertices":JSONValue(coordinates),"uv":JSONValue(texture),"triangles":JSONValue(triangles)]);
}

private JSONValue rootTextureMapping(Part part) {
    auto result = textureMapping(part);
    Point2[] vertices;
    foreach (p; ngRigPoints(result["vertices"])) vertices ~= toRoot(part,p[0],p[1]);
    result["vertices"] = ngRigPointsJson(vertices);
    return result;
}

private bool sameTextureMapping(JSONValue a, JSONValue b) {
    return ngRigSameTextureMapping(a,b);
}

private JSONValue verifySourceFrames(JSONValue state, AutoRigTaskContext task) {
    JSONValue[] snapshots, checks;
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        foreach (material; state["materials"].array) {
            auto part = cast(Part)puppet.find!Node(uuid(material["uuid"]));
            enforce(part !is null,"Source frame verification Part is missing");
            snapshots ~= rootTextureMapping(part);
        }
    });
    foreach (i,material; state["materials"].array) {
        ngRigCheckpoint(task);
        try {
            checks ~= JSONValue(["part":material["uuid"],"maximum_root_error":JSONValue(
                ngRigVerifySourceUV(material["source_root_mapping"],snapshots[i]))]);
        } catch (Exception error) {
            auto diagnostic = JSONValue(["part":material["uuid"],"path":material["path"],
                "source":material["source_root_mapping"],"actual":snapshots[i],"error":JSONValue(error.msg)]);
            task.previewJson("source-frame-mismatch",diagnostic);
            throw new Exception(material["path"].str ~ ": " ~ error.msg);
        }
    }
    return JSONValue(checks);
}

private JSONValue renderFrame(AutoRigTaskContext task, string name, JSONValue cameraFrame = JSONValue.init) {
    JSONValue frame;
    ubyte[] pixels;
    task.runOnMainThread({
        auto camera = inGetCamera();
        auto position = camera.position, scale = camera.scale; auto rotation = camera.rotation;
        auto targetPosition = incViewportTargetPosition; auto targetZoom = incViewportTargetZoom, zoom = incViewportZoom;
        scope(exit) {
            camera.position = position; camera.scale = scale; camera.rotation = rotation;
            incViewportTargetPosition = targetPosition; incViewportTargetZoom = targetZoom; incViewportZoom = zoom;
        }
        if (cameraFrame.type == JSONType.object) {
            auto p = ngRigNumbers(cameraFrame["position"]), s = ngRigNumbers(cameraFrame["scale"]);
            camera.position = vec2(cast(float)p[0],cast(float)p[1]); camera.scale = vec2(cast(float)s[0],cast(float)s[1]);
            camera.rotation = cast(float)ngRigNumber(cameraFrame["rotation"]);
        } else command(new FitViewportToModelCommand(),editorContext());
        frame = JSONValue(["position":JSONValue([camera.position.x,camera.position.y]),
            "scale":JSONValue([camera.scale.x,camera.scale.y]),"rotation":JSONValue(camera.rotation)]);
        auto result = cast(ExCommandResult!JSONValue)command(new CaptureLiveScreenshotCommand(),editorContext());
        enforce(result !is null, "Could not capture rig preview");
        pixels = Base64.decode(result.result["content"][0]["data"].str);
    });
    task.previewBlob(name,pixels,"image/png");
    return JSONValue(["data":JSONValue(Base64.encode(pixels)),"camera":frame,
        "sha256":JSONValue(sha256Of(pixels).toHexString.idup)]);
}

private JSONValue renderValidation(JSONValue state, AutoRigTaskContext task) {
    auto baseline = "neutral_render" in state.object;
    if (baseline is null) return JSONValue(["applicable":JSONValue(false),"reason":JSONValue("Optional rendering disabled")]);
    import imagefmt : read_image;
    auto neutral = renderFrame(task,"saved-neutral",(*baseline)["camera"]);
    auto sourceBytes = Base64.decode((*baseline)["data"].str);
    enforce(sha256Of(sourceBytes).toHexString.idup == (*baseline)["sha256"].str,"Neutral render ownership mismatch");
    auto source = read_image(sourceBytes,4), saved = read_image(Base64.decode(neutral["data"].str),4);
    scope(exit) { source.free(); saved.free(); }
    enforce(source.e == 0 && saved.e == 0 && source.w == saved.w && source.h == saved.h &&
        source.buf8.length>0 && source.buf8.length == saved.buf8.length,"Neutral preview dimensions changed");
    double mean = 0, maximum = 0;
    foreach (i,pixel; source.buf8) {
        double difference = abs(cast(double)pixel-saved.buf8[i]); mean += difference;
        maximum = max(maximum,difference);
    }
    mean /= source.buf8.length;
    JSONValue[] poses, bodyPoses, controlPoses;
    if (ngRigString(state,"kind","humanoid") == "humanoid") {
        scope(exit) task.runOnMainThread({
            foreach (parameter; incActivePuppet().parameters) parameter.value = parameter.defaults;
            incActivePuppet().update();
        });
        foreach (yawIndex,yaw; [-1.,-.5,0.,.5,1.]) foreach (pitchIndex,pitch; [-1.,0.,1.]) {
            ngRigCheckpoint(task);
            task.runOnMainThread({
                auto puppet = incActivePuppet();
                parameterByName(puppet,"Face::Yaw-Pitch").value = vec2(cast(float)yaw,cast(float)pitch);
                puppet.update();
            });
            import std.conv : to;
            auto pose = renderFrame(task,"head-yaw-" ~ yawIndex.to!string ~ "-pitch-" ~ pitchIndex.to!string,
                (*baseline)["camera"]);
            pose["parameter"] = JSONValue("Face::Yaw-Pitch");
            pose["value"] = JSONValue([yaw,pitch]); poses ~= pose;
        }
        void captureParameter(string name, double[] xs, double[] ys, ref JSONValue[] captures) {
            import std.array : replace;
            import std.conv : to;
            foreach (xIndex,x; xs) foreach (yIndex,y; ys) {
                ngRigCheckpoint(task);
                task.runOnMainThread({
                    auto puppet = incActivePuppet();
                    foreach (parameter; puppet.parameters) parameter.value = parameter.defaults;
                    parameterByName(puppet,name).value = vec2(cast(float)x,cast(float)y);
                    puppet.update();
                });
                auto pose = renderFrame(task,name.replace("::","-") ~ "-x-" ~ xIndex.to!string ~
                    "-y-" ~ yIndex.to!string,(*baseline)["camera"]);
                pose["parameter"] = JSONValue(name); pose["value"] = JSONValue([x,y]); captures ~= pose;
            }
        }
        captureParameter("Body::Yaw-Pitch",[-1.,-.5,0.,.5,1.],[-1.,0.,1.],bodyPoses);
        captureParameter("Body::Roll",[-1.,-.5,0.,.5,1.],[0.],bodyPoses);
        captureParameter("Face::Roll",[-1.,-.5,0.,.5,1.],[0.],poses);
        if (auto controls = "controls" in state.object)
            foreach (mechanism; (*controls)["mechanisms"].array)
                captureParameter(mechanism["name"].str,ngRigNumbers(mechanism["axisX"]),
                    ngRigNumbers(mechanism["axisY"]),controlPoses);
    }
    return JSONValue(["applicable":JSONValue(true),"neutral":neutral,"neutral_mean_absolute_error":JSONValue(mean),
        "neutral_maximum_error":JSONValue(maximum),"neutral_passed":JSONValue(mean<=1),
        "head_support_poses":JSONValue(poses),"body_support_poses":JSONValue(bodyPoses),
        "local_control_poses":JSONValue(controlPoses),"visually_reviewed":JSONValue(false)]);
}

private string editorSignature(Puppet puppet) {
    enforce(Thread.getThis is null || Thread.getThis.isMainThread, "Rig model snapshot requires the main thread");
    auto snapshot = parseJSON(inToJson(puppet));
    void authoredState(ref JSONValue node) {
        // Automatically resized composite meshes are generated during drawing.
        // Their child geometry and authored transforms remain part of the signature.
        if (auto resized = "auto_resized" in node.object)
            if (resized.boolean) node.object.remove("mesh");
        if (auto children = "children" in node.object)
            foreach (ref child; children.array) authoredState(child);
    }
    authoredState(snapshot["nodes"]);
    return ngRigDigest(snapshot);
}

private void settleDepthRefresh(AutoRigTaskContext task) {
    import std.datetime.stopwatch : StopWatch, AutoStart;
    auto elapsed = StopWatch(AutoStart.yes);
    while (true) {
        bool pending;
        task.runOnMainThread({
            ngFlushDepthBoneEffectivePivotDirty();
            ngFlushDepthBoneDirty();
            pending = ngHasPendingDepthBoneEffectivePivotRefresh() || ngHasPendingDepthBoneRefresh();
        });
        if (!pending) return;
        enforce(elapsed.peek.total!"seconds"<300,"Native depth refresh did not settle before checkpoint");
        ngRigCheckpoint(task);
    }
}

/** Main-thread readback produces owned CPU copies, as in AutoMesh's alpha input path. */
private JSONValue observeModel(JSONValue options, AutoRigTaskContext task) {
    uint rootId;
    string signature;
    uint[] ids;
    JSONValue[] groups;
    size_t[uint] sourceOrders;
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        enforce(puppet !is null, "No imported model is open");
        rootId = puppet.root.uuid;
        signature = editorSignature(puppet);
        foreach (parameter; puppet.parameters)
            enforce(parameter.value == parameter.defaults, "Model must be at its default pose before AutoRig");
        void visit(Node node) {
            sourceOrders[node.uuid] = sourceOrders.length;
            enforce(cast(ExDepthRigRoot)node is null, "Model already contains a depth rig");
            if (node.typeId == "Part") ids ~= node.uuid;
            else if (node !is puppet.root && node.children.length) groups ~= JSONValue([
                "uuid":JSONValue(node.uuid),"name":JSONValue(node.name),
                "parent":JSONValue(node.parent.uuid),"source_order":JSONValue(sourceOrders[node.uuid])]);
            foreach (child; node.children) visit(child);
        }
        visit(puppet.root);
    });
    enforce(ids.length>0, "Imported model has no Parts");
    JSONValue[] materials;
    foreach (id; ids) {
        ngRigCheckpoint(task);
        JSONValue record, rootMesh;
        ubyte[] pixels;
        size_t width, height;
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            enforce(puppet !is null && puppet.root.uuid == rootId, "Active model changed during observation");
            auto part = cast(Part)puppet.find!Node(id);
            enforce(part !is null, "Part disappeared during observation");
            auto alpha = getAlphaInput(part);
            enforce(alpha.img !is null && alpha.w>0 && alpha.h>0, "Part has no readable alpha texture");
            width = alpha.w; height = alpha.h;
            pixels = alpha.img.data.dup;
            auto mapping = textureMapping(part);
            Point2[] world;
            foreach (p; ngRigPoints(mapping["vertices"])) world ~= toRoot(part,p[0],p[1]);
            // JSONValue objects share their backing associative array on assignment.
            // Keep the local source mapping independent of the root-space snapshot.
            rootMesh = JSONValue(["vertices":ngRigPointsJson(world),
                "uv":mapping["uv"],"triangles":mapping["triangles"]]);
            double[4] bounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
            foreach (p; world) {
                bounds[0] = min(bounds[0],p[0]); bounds[1] = min(bounds[1],p[1]);
                bounds[2] = max(bounds[2],p[0]); bounds[3] = max(bounds[3],p[1]);
            }
            string path;
            string[] ancestors;
            uint[] ancestorIds;
            for (auto node = cast(Node)part; node !is puppet.root; node = node.parent) {
                path = "/" ~ node.name ~ path;
                if (node !is part) { ancestors = [node.name] ~ ancestors; ancestorIds = [node.uuid] ~ ancestorIds; }
            }
            if (auto imported = cast(ExPart)part) if (imported.layerPath.length) path = imported.layerPath;
            bool active = part.opacity>0;
            for (auto node = cast(Node)part; node !is null; node = node.parent) {
                active = active && node.getEnabled();
                if (auto composite = cast(Projectable)node) active = active && composite.opacity>0;
            }
            record = JSONValue(["name":JSONValue(part.name),"path":JSONValue(path),"uuid":JSONValue(id),
                "source_order":JSONValue(sourceOrders[id]),
                "active":JSONValue(active),"bounds":JSONValue(bounds[]),"source_mapping":mapping,
                "texture_size":JSONValue([width,height]),"texture_sha256":JSONValue(sha256Of(pixels).toHexString.idup)]);
            record["ancestors"] = JSONValue(ancestors);
            record["ancestor_ids"] = JSONValue(ancestorIds);
            record["parent"] = JSONValue(part.parent.uuid);
            auto parentMatrix = puppet.root.transform.matrix.inverse * part.parent.transform.matrix;
            auto px = parentMatrix*vec4(1,0,0,0), py = parentMatrix*vec4(0,1,0,0), po = parentMatrix*vec4(0,0,0,1);
            record["parent_to_root"] = JSONValue([px.x,py.x,po.x,px.y,py.y,po.y]);
            foreach (mask; part.masks) if (mask.mode == MaskingMode.Mask) {
                record["receiver"] = JSONValue(mask.maskSrcUUID); break;
            }
        });
        auto cloud = ngRigTextureSupport(pixels,width,height,rootMesh,40000,task);
        record["source_root_mapping"] = rootMesh;
        record["alpha_runs_32"] = ngRigAlphaRuns(pixels,32);
        record["alpha_runs_128"] = ngRigAlphaRuns(pixels,128);
        auto strongAlpha = pixels.dup;
        foreach (pixel; 0 .. strongAlpha.length/4) if (strongAlpha[pixel*4+3]<=128) strongAlpha[pixel*4+3] = 0;
        record["draw_order_cloud"] = ngRigPointsJson(ngRigTextureSupport(strongAlpha,width,height,rootMesh,10000,task));
        if (ngRigMaterialFeature(record["name"].str) == "sclera") {
            auto mainPatch = ngRigLargestAlphaComponent(pixels,width,height,task);
            record["landmark_cloud"] = ngRigPointsJson(ngRigTextureSupport(mainPatch,width,height,rootMesh,40000,task));
        }
        record["opaque_perimeter_coverage"] = JSONValue(ngRigAlphaPerimeterCoverage(pixels,width,height));
        record["cloud"] = ngRigPointsJson(cloud);
        record["active"] = JSONValue(record["active"].boolean && cloud.length>0);
        materials ~= record;
    }
    auto result = JSONValue(["schema_version":JSONValue("rig-model-observation-d/1"),"rootId":JSONValue(rootId),
        "materials":JSONValue(materials),"groups":JSONValue(groups),"options":options]);
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        enforce(puppet !is null && puppet.root.uuid == rootId && editorSignature(puppet) == signature,
            "Imported model changed while CPU snapshots were being acquired");
    });
    result["source_sha256"] = JSONValue(ngRigDigest(result));
    if (ngRigGet(options,"render",JSONValue(false)).boolean) result["neutral_render"] = renderFrame(task,"source-neutral");
    return result;
}

/** Prepare imported layer groups without retaining editor objects in the worker Fiber. */
private JSONValue prepareSourceGroups(JSONValue state, AutoRigTaskContext task) {
    auto sourceGroups = "groups" in state.object;
    JSONValue[] prepared;
    if (sourceGroups is null) return state;
    foreach (group; sourceGroups.array) {
        ngRigCheckpoint(task);
        bool hasMaterial, onlyEyes = true, onlyMouth = true;
        foreach (material; state["materials"].array) {
            if (!material["active"].boolean) continue;
            auto ancestors = "ancestor_ids" in material.object;
            if (ancestors is null) continue;
            bool belongs;
            foreach (ancestor; ancestors.array) if (ancestor == group["uuid"]) belongs = true;
            if (!belongs) continue;
            hasMaterial = true;
            auto feature = ngRigString(material,"feature","");
            bool eye;
            foreach (candidate; ["sclera","iris","upper","lower","corner","fold","brow"])
                if (candidate == feature) eye = true;
            onlyEyes = onlyEyes && eye; onlyMouth = onlyMouth && feature.startsWith("mouth");
        }
        if (!hasMaterial) continue;
        string desired = onlyEyes || onlyMouth ? "DynamicComposite" : "GridDeformer";
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto node = puppet.find!Node(uuid(group["uuid"]));
            enforce(node !is null && node.parent.uuid == uuid(group["parent"]),"Source group hierarchy changed");
            auto before = node.localTransform;
            uint[] children;
            foreach (child; node.children) children ~= child.uuid;
            if (node.typeId != desired) {
                if (node.typeId != "Node") node = create(new ConvertToCommandT!true("Node"),editorContext([node]));
                node = create(new ConvertToCommandT!true(desired),editorContext([node]));
            }
            enforce(node.uuid == uuid(group["uuid"]) && node.parent.uuid == uuid(group["parent"]) &&
                node.localTransform == before,"Source group conversion changed identity or transform");
            uint[] after; foreach (child; node.children) after ~= child.uuid;
            enforce(children == after,"Source group conversion changed child order");
        });
        prepared ~= JSONValue(["uuid":group["uuid"],"type":JSONValue(desired)]);
    }
    // Children are meshed first so a parent reads their completed geometry.
    foreach_reverse (group; prepared) if (group["type"].str == "GridDeformer") {
        auto processor = new GridAutoMeshProcessor();
        processor.maskThreshold = 1; processor.margin = 0; processor.xSegments = 10; processor.ySegments = 10;
        processor.scaleX = null; processor.scaleY = null;
        foreach (i; 0 .. 11) { processor.scaleX ~= i/10.0f; processor.scaleY ~= i/10.0f; }
        CommandResult result;
        task.runOnMainThread({
            auto node = incActivePuppet().find!Node(uuid(group["uuid"]));
            result = command(new ApplyAutoMeshPT!GridAutoMeshProcessor(processor),editorContext([node]));
        });
        result = result.waitForCompletion(); enforce(result.succeeded,result.message);
    }
    state["prepared_groups"] = JSONValue(prepared);
    return state;
}

private JSONValue meshParts(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    struct MeshGroup { uint[] ids; float spacing, divisions; float[] scales; bool shoulder; }
    MeshGroup[string] groups;
    bool[ulong] shoulders;
    foreach (pair; ngRigGet(state,"shoulder_pairs",JSONValue(cast(JSONValue[])null)).array)
        if (pair["matching"].boolean) shoulders[ngRigUnsigned(pair["target"])] = true;
    JSONValue[uint] beforeFrames;
    task.runOnMainThread({
        foreach (carrier; program["carriers"].array) {
            auto part = cast(Part)incActivePuppet().find!Node(uuid(carrier["part"]));
            enforce(part !is null, "Rig material disappeared before meshing");
            auto texture = part.textures[0]; enforce(texture !is null,"Rig Part texture is missing");
            float spacing = min(10.0f,max(1.0f,cast(float)max(texture.width,texture.height)/12));
            bool shoulder = (part.uuid in shoulders) !is null;
            float divisions = 12; float[] scales = [.5,0.];
            if (shoulder) {
                spacing = 1; divisions = cast(float)min(64.,max(12.,ceil(max(texture.width,texture.height)/4.)));
                double radius = sqrt(cast(double)texture.width*texture.width+cast(double)texture.height*texture.height)/2;
                size_t count = cast(size_t)max(2.,ceil(.2*radius/4)+1);
                scales = null;
                foreach (i; 0 .. count) scales ~= cast(float)(.8+.2*i/(count-1));
                scales ~= [.5,0.];
            }
            auto configuration = JSONValue(["spacing":JSONValue(spacing),"divisions":JSONValue(divisions),
                "scales":JSONValue(scales),"shoulder":JSONValue(shoulder)]);
            auto key = ngRigDigest(configuration);
            if (auto existing = key in groups) existing.ids ~= part.uuid;
            else groups[key] = MeshGroup([part.uuid],spacing,divisions,scales,shoulder);
            auto transform = part.localTransform;
            beforeFrames[part.uuid] = JSONValue([transform.translation.x,transform.translation.y,transform.translation.z,
                transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.scale.x,transform.scale.y]);
        }
    });
    JSONValue[] configurations;
    foreach (key,group; groups) {
        ngRigCheckpoint(task);
        CommandResult result;
        auto processor = new RigPartMeshProcessor(group.spacing,group.divisions,group.scales,group.shoulder);
        task.runOnMainThread({
            Node[] parts;
            foreach (id; group.ids) parts ~= incActivePuppet().find!Node(id);
            result = command(new ApplyAutoMeshPT!OptimumAutoMeshProcessor(processor),editorContext(parts));
        });
        result = result.waitForCompletion(); enforce(result.succeeded,result.message);
        configurations ~= JSONValue(["parts":JSONValue(group.ids),"min_distance":JSONValue(group.spacing),
            "div_per_part":JSONValue(group.divisions),"scales":JSONValue(group.scales),
            "proximal_shoulder_sampling":JSONValue(group.shoulder)]);
    }
    task.runOnMainThread({
        foreach (id,before; beforeFrames) {
            auto part = incActivePuppet().find!Node(id); auto transform = part.localTransform;
            auto actual = JSONValue([transform.translation.x,transform.translation.y,transform.translation.z,
                transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.scale.x,transform.scale.y]);
            enforce(before == actual,"Part AutoMesh changed the source coordinate frame");
        }
    });
    state["part_mesh_generation"] = JSONValue(configurations);
    return state;
}

private JSONValue registerSourceUV(JSONValue state, AutoRigTaskContext task) {
    JSONValue[] readback;
    foreach (material; state["materials"].array) {
        if (material["static"].boolean) continue;
        ngRigCheckpoint(task);
        JSONValue current;
        Point2 oldScale, oldTranslation;
        double rotation;
        task.runOnMainThread({
            auto part = cast(Part)incActivePuppet().find!Node(uuid(material["uuid"]));
            enforce(part !is null,"UV registration Part is missing");
            enforce(ngModelEditSubMode() == ModelEditSubMode.Layout,"UV registration requires layout mode");
            enforce(abs(part.localTransform.rotation.x)<1e-6 && abs(part.localTransform.rotation.y)<1e-6 &&
                part.getMesh().origin == vec2(0,0),"UV registration needs a resolved Part frame");
            current = textureMapping(part);
            oldScale = [part.localTransform.scale.x,part.localTransform.scale.y];
            oldTranslation = [part.localTransform.translation.x,part.localTransform.translation.y];
            rotation = part.localTransform.rotation.z;
        });
        auto operation = ngRigSourceUVRegistration(material["source_mapping"],current);
        auto scale = ngRigNumbers(operation["scale"]), shift = ngRigNumbers(operation["shift"]);
        import std.math : sin, cos;
        Point2 shifted = [shift[0]*oldScale[0],shift[1]*oldScale[1]];
        Point2 translation = [oldTranslation[0]+shifted[0]*cos(rotation)-shifted[1]*sin(rotation),
            oldTranslation[1]+shifted[0]*sin(rotation)+shifted[1]*cos(rotation)];
        task.runOnMainThread({
            auto part = incActivePuppet().find!Node(uuid(material["uuid"]));
            auto ctx = editorContext([part]); ctx.inspectors = [new NINode([part],ModelEditSubMode.Layout)];
            auto sx = new ScaleXCommand(); sx.value = cast(float)(oldScale[0]*scale[0]); command(sx,ctx);
            auto sy = new ScaleYCommand(); sy.value = cast(float)(oldScale[1]*scale[1]); command(sy,ctx);
            auto tx = new TranslationXCommand(); tx.value = cast(float)translation[0]; command(tx,ctx);
            auto ty = new TranslationYCommand(); ty.value = cast(float)translation[1]; command(ty,ctx);
            enforce(textureMapping(cast(Part)part) == current,"UV registration changed native mesh arrays");
        });
        operation["part"] = material["uuid"]; readback ~= operation;
    }
    state["source_uv_registration"] = JSONValue(readback);
    state["source_uv_program_sha256"] = JSONValue(ngRigDigest(JSONValue(readback)));
    state["source_uv_readback"] = verifySourceFrames(state,task);
    return state;
}

private JSONValue prepareFeatureComposites(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    uint[][string] groups;
    string[ulong] sides;
    foreach (carrier; program["carriers"].array) sides[ngRigUnsigned(carrier["part"])] = carrier["side"].str;
    foreach (material; state["materials"].array) {
        if (material["static"].boolean) continue;
        auto feature = ngRigString(material,"feature","");
        if (feature.startsWith("mouth")) groups["Mouth"] ~= uuid(material["uuid"]);
        else if (feature == "sclera" || feature == "iris" || feature == "upper" || feature == "lower" ||
            feature == "corner" || feature == "fold") groups["Eye::" ~ sides[ngRigUnsigned(material["uuid"])]] ~= uuid(material["uuid"]);
    }
    JSONValue[] composites;
    bool[uint] meshed;
    foreach (name,ids; groups) {
        uint[] targets;
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto first = puppet.find!Node(ids[0]);
            Node commonComposite;
            for (auto candidate = first.parent; candidate !is null && candidate !is puppet.root; candidate = candidate.parent) {
                if (candidate.typeId != "DynamicComposite") continue;
                bool common = true;
                foreach (id; ids) {
                    bool contained;
                    for (auto cursor = puppet.find!Node(id); cursor !is null; cursor = cursor.parent)
                        if (cursor is candidate) contained = true;
                    common = common && contained;
                }
                if (common) { commonComposite = candidate; break; }
            }
            if (commonComposite !is null) targets ~= commonComposite.uuid;
            else {
                uint[][uint] partitions;
                foreach (id; ids) partitions[puppet.find!Node(id).parent.uuid] ~= id;
                foreach (parentId,parts; partitions) {
                    auto parent = puppet.find!Node(parentId);
                    auto composite = create(new AddNodeCommandT!true("DynamicComposite","::Mechanism"),editorContext([parent]));
                    composite.name = name ~ "::Composite";
                    Node[] selected; foreach (id; parts) selected ~= puppet.find!Node(id);
                    command(new MoveNodeCommand(composite,0,true),editorContext(selected));
                    targets ~= composite.uuid;
                }
            }
            puppet.root.build(); puppet.update();
        });
        double[4] bounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
        foreach (material; state["materials"].array) if (ids.canFind(uuid(material["uuid"])))
            foreach (p; ngRigPoints(material["cloud"])) {
                bounds[0] = min(bounds[0],p[0]); bounds[1] = min(bounds[1],p[1]);
                bounds[2] = max(bounds[2],p[0]); bounds[3] = max(bounds[3],p[1]);
            }
        double width = bounds[2]-bounds[0], height = max(1.,bounds[3]-bounds[1]);
        // Envelope follows the authored local-control amplitudes, not a borrowed model mesh.
        float margin = cast(float)(name == "Mouth" ? max(.25,.135*width/height) : max(.18,1+.08*width/height));
        foreach (id; targets) {
            if (id in meshed) continue;
            ngRigCheckpoint(task);
            auto processor = new GridAutoMeshProcessor();
            processor.maskThreshold = 1; processor.xSegments = 10; processor.ySegments = 10; processor.margin = margin;
            processor.ngPostParamWrite("margin");
            CommandResult result;
            task.runOnMainThread({ result = command(new ApplyAutoMeshPT!GridAutoMeshProcessor(processor),
                editorContext([incActivePuppet().find!Node(id)])); });
            result = result.waitForCompletion(); enforce(result.succeeded,result.message);
            task.runOnMainThread({
                auto composite = cast(Projectable)incActivePuppet().find!Node(id);
                enforce(composite !is null && !composite.autoResizedMesh,"Feature composite did not retain its native generated mesh");
                composites ~= JSONValue(["uuid":JSONValue(id),"name":JSONValue(name),"parts":JSONValue(ids),
                    "mapping":textureMapping(composite),"margin":JSONValue(margin),"segments":JSONValue(10)]);
            });
            meshed[id] = true;
        }
    }
    state["feature_composites"] = JSONValue(composites);
    state["composite_source_uv_readback"] = verifySourceFrames(state,task);
    return state;
}

private JSONValue registerDomainParents(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    string[uint] charts;
    foreach (carrier; program["carriers"].array) charts[uuid(carrier["part"])] =
        carrier["owner"].str ~ "/" ~ ngRigString(carrier,"chart",carrier["role"].str);
    auto materials = state["materials"].array.dup;
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        JSONValue[] nodes;
        double[uint] sourceOrders;
        foreach (material; state["materials"].array) if (auto order = "source_order" in material.object)
            sourceOrders[uuid(material["uuid"])] = ngRigNumber(*order);
        foreach (group; state["groups"].array) if (auto order = "source_order" in group.object)
            sourceOrders[uuid(group["uuid"])] = ngRigNumber(*order);
        double sourceOrder(Node node) {
            if (auto known = node.uuid in sourceOrders) return *known;
            double result = double.infinity;
            foreach (child; node.children) result = min(result,sourceOrder(child));
            return result;
        }
        size_t order;
        void snapshot(Node node) {
            auto record = JSONValue(["uuid":JSONValue(node.uuid),"type":JSONValue(node.typeId),
                "parent":node is puppet.root || node.parent is null ? JSONValue.init : JSONValue(node.parent.uuid),
                "source_order":JSONValue(min(sourceOrder(node),cast(double)(1000000+order++)))]);
            if (auto drawable = cast(Projectable)node) {
                record["opacity"] = JSONValue(drawable.opacity);
                record["blend_mode"] = JSONValue(drawable.blendingMode == BlendMode.Normal ? "Normal" : "Other");
            }
            nodes ~= record;
            foreach (child; node.children) snapshot(child);
        }
        snapshot(puppet.root); state["hierarchy_nodes"] = JSONValue(nodes);
        bool uniform(Node node, string chart) {
            if (node.typeId == "Part") {
                auto known = node.uuid in charts; if (known is null || *known != chart) return false;
            }
            foreach (child; node.children) if (!uniform(child,chart)) return false;
            return true;
        }
        foreach (ref material; materials) {
            if (material["static"].boolean) continue;
            auto part = puppet.find!Node(uuid(material["uuid"])); auto unit = part;
            while (unit.parent !is null && unit.parent !is puppet.root && uniform(unit.parent,charts[part.uuid])) unit = unit.parent;
            material["carrier_unit"] = JSONValue(unit.uuid); material["parent"] = JSONValue(unit.parent.uuid);
            auto matrix = puppet.root.transform.matrix.inverse * unit.parent.transform.matrix;
            auto x = matrix*vec4(1,0,0,0), y = matrix*vec4(0,1,0,0), origin = matrix*vec4(0,0,0,1);
            material["parent_to_root"] = JSONValue([x.x,y.x,origin.x,x.y,y.y,origin.y]);
        }
    });
    state["materials"] = JSONValue(materials);
    return state;
}

private JSONValue buildRig(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    JSONValue result = state;
    bool humanoid = ngRigString(program,"kind","humanoid") == "humanoid";
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        auto root = create(new AddNodeCommandT!(true)("DepthRigRoot","::AutoRig"),editorContext([puppet.root]));
        root.name = "AutoRig::DepthRig";
        if (humanoid) {
            // Match InitialTree.prepare_units: anatomy hosted inside a rendering
            // unit needs a propagating Composite rather than a deformation endpoint.
            uint[] hosted;
            foreach (definition; program["hierarchy"]["groups"].array)
                if (auto node = "node" in definition["parent"].object) hosted ~= uuid(*node);
            foreach (id, parent; program["hierarchy"]["surface_parents"].object)
                if (auto node = "node" in parent.object) hosted ~= uuid(*node);
            if (program["hierarchy"]["face_origin"].type != JSONType.null_)
                hosted ~= uuid(program["hierarchy"]["face_origin"]);
            JSONValue[] converted;
            foreach (id, units; program["hierarchy"]["render_units"].object) foreach (unitId; units.array) {
                auto unit = puppet.find!Node(uuid(unitId));
                if (unit is null || unit.typeId != "DynamicComposite") continue;
                bool contains;
                foreach (origin; hosted)
                    for (auto cursor = puppet.find!Node(origin); cursor !is null; cursor = cursor.parent)
                        if (cursor is unit) contains = true;
                if (!contains) continue;
                auto before = parseJSON(inToJson(unit));
                auto composite = cast(Composite)create(new ConvertToCommandT!true("Composite"),editorContext([unit]));
                enforce(composite !is null && composite.uuid == uuid(unitId) && composite.propagateMeshGroup,
                    "Origin composite did not preserve identity and propagate deformation");
                auto after = parseJSON(inToJson(composite));
                foreach (key; ["blend_mode", "opacity", "masks"])
                    enforce(ngRigGet(before,key) == ngRigGet(after,key),
                        "Origin composite conversion changed " ~ key);
                converted ~= unitId;
            }
            state["origin_composites"] = JSONValue(converted);
        }
        Node[string] bones;
        double[string] poseOrigins;
        if (humanoid) foreach (bone; program["scaffold"]["bones"].array) {
            auto add = new AddDepthBoneCommand();
            add.parent = bone["parent"].type == JSONType.null_ ? root : bones[bone["parent"].str];
            add.boneId = bone["id"].str; add.restHead = nativeNumbers(bone["head"]);
            add.restTail = nativeNumbers(bone["tail"]); add.restRoll = cast(float)ngRigNumber(bone["rest_roll"]);
            auto created = create(add,editorContext()); bones[bone["id"].str] = created;
            auto inheritance = new SetDepthBoneConstraintCommand(); inheritance.bone = created;
            inheritance.constraint = JSONValue(["allowParentToTargets":bone["allow_parent_to_targets"]]).toString();
            command(inheritance,editorContext());
            if (bone["lock_to_root"].boolean) {
                auto context = editorContext([created]);
                context.inspectors = [new NINode([created],ModelEditSubMode.Layout)];
                auto lock = new LockToRootCommand(); lock.value = true; command(lock,context);
            }
            if (auto pose = "pose_origin_z" in bone.object) {
                auto absolute = ngRigNumber(*pose);
                auto parentZ = bone["parent"].type == JSONType.null_ || bone["lock_to_root"].boolean ? 0. :
                    poseOrigins[bone["parent"].str];
                auto context = editorContext([created]);
                context.inspectors = [new NINode([created],ModelEditSubMode.Layout)];
                auto translation = new TranslationZCommand(); translation.value = cast(float)(absolute-parentZ);
                command(translation,context); poseOrigins[bone["id"].str] = absolute;
            }
        }
        if (humanoid) {
            auto parameters = new AddStandardDepthParametersCommand(program["native_drivers"]); parameters.root = root;
            command(parameters,editorContext());
        }
        // Material origins and shared surface parents follow the semantic plan.
        Node[string] origins;
        void setFrame(Node node, mat4 matrix) {
            import std.math : atan2;
            auto parent = node.parent.transform.matrix;
            auto local = parent.inverse*matrix;
            auto x = local*vec4(1,0,0,0), y = local*vec4(0,1,0,0), p = local*vec4(0,0,0,1);
            float sx = sqrt(x.x*x.x+x.y*x.y), sy = sqrt(y.x*y.x+y.y*y.y);
            enforce(sx>0 && sy>0 && abs(x.x*y.x+x.y*y.y)<=1e-4*sx*sy,"Material hierarchy requires an unsupported shear");
            if (x.x*y.y-x.y*y.x<0) sy = -sy;
            auto context = editorContext([node]); context.inspectors = [new NINode([node],ModelEditSubMode.Layout)];
            auto scaleX = new ScaleXCommand(); scaleX.value = sx; command(scaleX,context);
            auto scaleY = new ScaleYCommand(); scaleY.value = sy; command(scaleY,context);
            auto rotate = new RotationZCommand(); rotate.value = atan2(x.y,x.x); command(rotate,context);
            auto translateX = new TranslationXCommand(); translateX.value = p.x; command(translateX,context);
            auto translateY = new TranslationYCommand(); translateY.value = p.y; command(translateY,context);
            auto translateZ = new TranslationZCommand(); translateZ.value = p.z; command(translateZ,context);
            node.transformChanged();
        }
        void movePreservingFrame(Node unit, Node parent) {
            auto world = unit.transform.matrix;
            float absoluteSort = unit.zSortNoOffset;
            command(new MoveNodeCommand(parent,parent.children.length,true),editorContext([unit]));
            setFrame(unit,world);
            float parentSort = parent.zSortNoOffset;
            auto context = editorContext([unit]); context.inspectors = [new NINode([unit],ModelEditSubMode.Layout)];
            auto sort = new ZSortCommand(); sort.value = absoluteSort-parentSort; command(sort,context);
        }
        if (humanoid) foreach (definition; program["hierarchy"]["groups"].array) {
            auto parent = puppet.find!Node(uuid(definition["parent"]["node"]));
            enforce(parent !is null,"Material origin parent disappeared");
            auto origin = create(new AddNodeCommandT!true("Node","::AutoRig"),editorContext([parent]));
            origin.name = definition["id"].str;
            auto point = ngRigNumbers(definition["origin"]);
            auto desired = puppet.root.transform.matrix*mat4.translation(cast(float)point[0],cast(float)point[1],0);
            setFrame(origin,desired); origins[definition["id"].str] = origin;
        }
        JSONValue[] targets;
        Node[string] domainGrids;
        foreach (carrier; program["carriers"].array) {
            auto part = puppet.find!Node(uuid(carrier["part"])); enforce(part !is null, "Rig part disappeared");
            auto target = JSONValue(carrier.object.dup);
            if (humanoid) {
                auto id = carrier["domain_id"].str;
                Node grid;
                if (auto existing = id in domainGrids) grid = *existing;
                else {
                    auto selector = program["hierarchy"]["surface_parents"][id];
                    auto parent = "group" in selector.object ? origins[selector["group"].str] :
                        puppet.find!Node(uuid(selector["node"]));
                    enforce(parent !is null,"Semantic surface parent disappeared");
                    grid = create(new AddNodeCommandT!true("GridDeformer","::AutoRig"),editorContext([parent]));
                    grid.name = "AutoRig::" ~ id;
                    auto frame = ngRigNumbers(carrier["parent_to_root"]);
                    setFrame(grid,puppet.root.transform.matrix*mat4.translation(cast(float)frame[2],cast(float)frame[5],0));
                    foreach (bone; carrier["bones"].array) {
                        auto add = new AddDepthBoneSourceCommand(); add.root = root; add.target = grid; add.bone = bones[bone.str];
                        command(add,editorContext());
                    }
                    if (auto rule = "bone_influence_rule" in carrier.object) {
                        auto apply = new SetDepthBoneInfluenceRuleCommand(); apply.root = root; apply.target = grid;
                        apply.rule = (*rule).toString(); command(apply,editorContext());
                    }
                    domainGrids[id] = grid;
                    foreach (unitId; program["hierarchy"]["render_units"][id].array) {
                        auto unit = puppet.find!Node(uuid(unitId));
                        enforce(unit !is null,"Semantic render unit disappeared");
                        movePreservingFrame(unit,grid);
                    }
                }
                target["grid"] = JSONValue(grid.uuid);
            }
            target["mapping"] = textureMapping(cast(Part)part);
            Point2[] world;
            foreach (p; ngRigPoints(target["mapping"]["vertices"])) world ~= toRoot(part,p[0],p[1]);
            target["world"] = ngRigPointsJson(world);
            target["origin"] = JSONValue(toRoot(part,0,0)[]);
            auto direction = part.transform.matrix.inverse * puppet.root.transform.matrix;
            auto dx = direction*vec4(1,0,0,0), dy = direction*vec4(0,1,0,0);
            target["root_to_local_direction"] = JSONValue([dx.x,dy.x,dx.y,dy.y]);
            double absoluteZ = part.zSortNoOffset;
            uint[] compositeScope;
            for (auto node = part; node !is null; node = node.parent) {
                if (node !is part && node.typeId.canFind("Composite")) compositeScope = [node.uuid] ~ compositeScope;
            }
            target["absolute_zsort"] = JSONValue(absoluteZ); target["relative_zsort"] = JSONValue(part.relZSort);
            target["composite_scope"] = JSONValue(compositeScope);
            targets ~= target;
        }
        if (humanoid && program["hierarchy"]["face_origin"].type != JSONType.null_) {
            auto origin = puppet.find!Node(uuid(program["hierarchy"]["face_origin"]));
            bool[uint] faceParts;
            Node grid;
            foreach (carrier; program["carriers"].array) if (carrier["owner"].str == "head" && carrier["chart"].str == "face") {
                faceParts[uuid(carrier["part"])] = true; grid = domainGrids[carrier["domain_id"].str];
            }
            bool movable(Node node) {
                if (node is origin || node is grid) return false;
                if (node.typeId == "Part" && (node.uuid in faceParts) is null) return false;
                foreach (child; node.children) if (!movable(child)) return false;
                return true;
            }
            Node[] units;
            foreach (id,unused; faceParts) {
                auto part = puppet.find!Node(id); if (part is origin) continue;
                auto unit = part;
                while (unit.parent !is null && unit.parent !is origin && movable(unit.parent)) unit = unit.parent;
                if (unit.parent !is origin && !units.canFind(unit)) units ~= unit;
            }
            foreach (unit; units) movePreservingFrame(unit,origin);
        }
        foreach (ref target; targets) {
            auto part = puppet.find!Node(uuid(target["part"]));
            uint[] scopeIds;
            for (auto cursor = part.parent; cursor !is null; cursor = cursor.parent)
                if (cursor.typeId.canFind("Composite")) scopeIds = [cursor.uuid] ~ scopeIds;
            target["relative_zsort"] = JSONValue(part.relZSort);
            target["composite_scope"] = JSONValue(scopeIds);
        }
        if (humanoid) {
            // Imported group grids remain between a Part and its semantic
            // surface. They must use that surface's sources so the native
            // ancestor influence subtraction applies the deformation once.
            string[uint] assignedGroups;
            JSONValue[] sourceGrids;
            foreach (target; targets) {
                auto surface = puppet.find!Node(uuid(target["grid"]));
                auto part = puppet.find!Node(uuid(target["part"]));
                bool reachedSurface;
                for (auto node = part.parent; node !is null; node = node.parent) {
                    if (node is surface) { reachedSurface = true; break; }
                    if (node.typeId != "GridDeformer") continue;
                    auto domain = target["domain_id"].str;
                    if (auto previous = node.uuid in assignedGroups) {
                        enforce(*previous == domain,"Imported group grid spans incompatible semantic surfaces");
                        continue;
                    }
                    Point2[] depthField, samplePoints;
                    foreach (depth; ngRigNumbers(target["depth"])) {
                        Point2 point = [depth,0.]; depthField ~= point;
                    }
                    foreach (vertex; (cast(Deformable)node).vertices)
                        samplePoints ~= toRoot(node,vertex.x,vertex.y);
                    auto xs = ngRigNumbers(target["xs"]), ys = ngRigNumbers(target["ys"]);
                    double x0 = xs[0], y0 = ys[0], width = xs[$-1]-x0, height = ys[$-1]-y0;
                    enforce(width>0 && height>0,"Semantic surface has degenerate bounds");
                    foreach (ref x; xs) x = (x-x0)/width;
                    foreach (ref y; ys) y = (y-y0)/height;
                    foreach (ref point; samplePoints) {
                        auto frame = ngRigNumbers(target["parent_to_root"]);
                        point[0] = (point[0]-frame[2]-x0)/width; point[1] = (point[1]-frame[5]-y0)/height;
                    }
                    auto sampled = ngRigSampleGrid(depthField,xs,ys,samplePoints);
                    float[] depths;
                    foreach (value; sampled) {
                        enforce(isFinite(value[0]),"Imported group surface depth is nonfinite");
                        depths ~= cast(float)value[0];
                    }
                    auto setDepth = new SetDepthsCommand(); setDepth.target = node; setDepth.depths = depths;
                    command(setDepth,editorContext());
                    foreach (bone; target["bones"].array) {
                        auto add = new AddDepthBoneSourceCommand();
                        add.root = root; add.target = node; add.bone = bones[bone.str];
                        command(add,editorContext());
                    }
                    assignedGroups[node.uuid] = domain;
                    sourceGrids ~= JSONValue(["grid":JSONValue(node.uuid),"surface":target["grid"]]);
                }
                enforce(reachedSurface,"Material escaped its semantic surface");
            }
            state["source_group_grids"] = JSONValue(sourceGrids);
        }
        puppet.root.build(); puppet.rescanNodes();
        result["kind"] = JSONValue(ngRigString(program,"kind","humanoid"));
        JSONValue[string] originIds;
        foreach (name,node; origins) originIds[name] = JSONValue(node.uuid);
        result["material_origins"] = JSONValue(originIds);
        result["rigRoot"] = JSONValue(root.uuid); result["targets"] = JSONValue(targets);
        result["program_sha256"] = program["content_sha256"];
    });
    if (humanoid) {
        bool[uint] processed;
        foreach (target; result["targets"].array) {
            auto gridId=uuid(target["grid"]); if (gridId in processed) continue;
            processed[gridId]=true;
            auto processor=new GridAutoMeshProcessor();
            processor.maskThreshold=15; processor.margin=0; processor.xSegments=1; processor.ySegments=1;
            processor.scaleX=[0.,1.]; processor.scaleY=[0.,1.];
            void applyGrid() {
                CommandResult applied;
                task.runOnMainThread({
                    auto grid=incActivePuppet().find!Node(gridId);
                    applied=command(new ApplyAutoMeshPT!GridAutoMeshProcessor(processor),editorContext([grid]));
                });
                applied=applied.waitForCompletion(); enforce(applied.succeeded,applied.message);
            }
            applyGrid();
            double[] boundsX,boundsY;
            task.runOnMainThread({
                import nijilive.core.nodes.deformer.grid : GridDeformer;
                auto grid=cast(GridDeformer)incActivePuppet().find!Node(gridId);
                auto data=parseJSON(inToJson(grid));
                boundsX=ngRigNumbers(data["grid_axis_x"]); boundsY=ngRigNumbers(data["grid_axis_y"]);
            });
            enforce(boundsX.length==2 && boundsY.length==2 && boundsX[1]>boundsX[0] && boundsY[1]>boundsY[0],
                "Grid AutoMesh returned degenerate alpha bounds");
            float[] advanced(double[] desired,double[] bounds) {
                double largest=max(1.,max(abs(bounds[0]),abs(bounds[1])));
                foreach (value; desired) largest=max(largest,abs(value));
                auto rounding=8*float.epsilon*largest; double[] native; float[] scales;
                foreach (value; desired) {
                    if (native.length && value-native[$-1]<=max(1e-4,1e-4*max(abs(value),abs(native[$-1])))+rounding)
                        continue;
                    native~=value; scales~=cast(float)((value-bounds[0])/(bounds[1]-bounds[0]));
                }
                return scales;
            }
            processor.scaleX=advanced(ngRigNumbers(target["xs"]),boundsX);
            processor.scaleY=advanced(ngRigNumbers(target["ys"]),boundsY);
            applyGrid();
            JSONValue generated;
            task.runOnMainThread({
                import nijilive.core.nodes.deformer.grid : GridDeformer;
                auto grid=cast(GridDeformer)incActivePuppet().find!Node(gridId);
                Point2[] local,world; double[] xs,ys;
                auto data=parseJSON(inToJson(grid)); xs=ngRigNumbers(data["grid_axis_x"]); ys=ngRigNumbers(data["grid_axis_y"]);
                foreach (p; grid.vertices) { local~=[cast(double)p.x,cast(double)p.y]; world~=toRoot(grid,p.x,p.y); }
                auto values=ngRigSampleRegisteredDepth(ngRigNumbers(target["xs"]),ngRigNumbers(target["ys"]),
                    ngRigNumbers(target["depth_model_units"]),local);
                foreach (ref value; values) value=rint(value/ngRigNumber(program["native_depth_scale"])*1e6)/1e6;
                auto depth=new SetDepthsCommand(); depth.target=grid;
                foreach (v; values) depth.depths~=cast(float)v; command(depth,editorContext());
                generated=JSONValue(["xs":JSONValue(xs),"ys":JSONValue(ys),"depth":JSONValue(values),
                    "points":ngRigPointsJson(world),"processor":JSONValue("grid"),"direct_mesh_definition":JSONValue(false)]);
            });
            foreach (ref updated; result["targets"].array) if (uuid(updated["grid"])==gridId)
                foreach (key; ["xs","ys","depth","points"]) updated[key]=generated[key];
            task.previewJson("grid-automesh-"~ngRigDigest(target["domain_id"])[0..16],generated);
        }
        task.runOnMainThread({
            auto puppet=incActivePuppet(); auto root=puppet.find!Node(uuid(result["rigRoot"]));
            foreach (source; ngRigGet(state,"source_group_grids",JSONValue(cast(JSONValue[])null)).array) {
                auto node=cast(Deformable)puppet.find!Node(uuid(source["grid"]));
                foreach (target; result["targets"].array) if (uuid(target["grid"])==uuid(source["surface"])) {
                    auto frame=ngRigNumbers(target["parent_to_root"]); Point2[] points;
                    foreach (vertex; node.vertices) {
                        auto p=toRoot(node,vertex.x,vertex.y); points~=[p[0]-frame[2],p[1]-frame[5]];
                    }
                    auto values=ngRigSampleRegisteredDepth(ngRigNumbers(target["xs"]),ngRigNumbers(target["ys"]),
                        ngRigNumbers(target["depth"]),points);
                    auto depth=new SetDepthsCommand(); depth.target=node;
                    foreach (value; values) depth.depths~=cast(float)value; command(depth,editorContext());
                    if (auto rule="bone_influence_rule" in target.object) {
                        auto influence=new SetDepthBoneInfluenceRuleCommand(); influence.root=root; influence.target=node;
                        influence.rule=(*rule).toString(); command(influence,editorContext());
                    }
                    break;
                }
            }
            // Registered bone Z origins are authored template inputs. The
            // original workflow never replaces them with a surface Fit Z.
        });
    }
    return result;
}

private JSONValue applyControls(JSONValue state, AutoRigTaskContext task) {
    auto controls = ngRigCompileControls(state,task);
    foreach (mechanism; controls["mechanisms"].array) {
        uint parameterId;
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            foreach (parameter; puppet.parameters) enforce(parameter.name != mechanism["name"].str,
                "Local control parameter already exists: " ~ mechanism["name"].str);
            auto created = cast(CreateResult!Parameter)command(new Add2DParameterCommand(-1,1),editorContext());
            enforce(created !is null && created.created.length == 1, "Could not create local control parameter");
            auto parameter = created.created[0]; parameter.name = mechanism["name"].str;
            auto xs = nativeNumbers(mechanism["axisX"]), ys = nativeNumbers(mechanism["axisY"]);
            parameter.min = vec2(xs[0],ys[0]); parameter.max = vec2(xs[$-1],ys[$-1]);
            parameter.axisPoints[0] = null; parameter.axisPoints[1] = null;
            foreach (x; xs) parameter.axisPoints[0] ~= (x-xs[0])/(xs[$-1]-xs[0]);
            foreach (y; ys) parameter.axisPoints[1] ~= (y-ys[0])/(ys[$-1]-ys[0]);
            parameter.defaults = vec2(0,0); parameter.value = parameter.defaults; parameterId = parameter.uuid;
        });
        foreach (operation; mechanism["operations"].array) foreach (key; operation["keys"].array) {
            ngRigCheckpoint(task);
            auto offsets = nativeNumbers(key["offsets"]);
            auto index = ngRigNumbers(key["key"]);
            task.runOnMainThread({
                auto puppet = incActivePuppet();
                auto node = puppet.find!Node(uuid(operation["part"]));
                auto parameter = puppet.findParameter(parameterId);
                enforce(node !is null && parameter !is null, "Local control target disappeared");
                auto context = editorContext([node]); context.parameters = [parameter];
                context.keyPoint = vec2u(cast(uint)index[0],cast(uint)index[1]); context.hasExplicitKeyPoint = true;
                command(new SetDeformBindingCommand("deform",offsets,true),context);
            });
        }
    }
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        foreach (mask; controls["masks"].array) {
            auto target = cast(Part)puppet.find!Node(uuid(mask["part"]));
            auto source = cast(Part)puppet.find!Node(uuid(mask["source"]));
            enforce(target !is null && source !is null, "Iris clipping target disappeared");
            bool exists;
            foreach (binding; target.masks) if (binding.maskSrc is source) exists = true;
            if (!exists) command(new AddMaskCommand(source,MaskingMode.Mask),editorContext([target]));
        }
        foreach (operation; controls["draw_order"].array) {
            auto node = puppet.find!Node(uuid(operation["part"]));
            enforce(node !is null,"Facial draw-order target disappeared");
            auto context = editorContext([node]); context.inspectors = [new NINode([node],ModelEditSubMode.Layout)];
            auto apply = new ZSortCommand(); apply.value = cast(float)ngRigNumber(operation["relative_zsort"]);
            command(apply,context);
            enforce(abs(node.relZSort-apply.value)<1e-6,"Facial draw-order readback failed");
        }
        foreach (parameter; puppet.parameters) parameter.value = parameter.defaults;
        puppet.root.build(); puppet.update();
    });
    state["controls"] = controls;
    return state;
}

private JSONValue weldShoulders(JSONValue state, AutoRigTaskContext task) {
    auto pairs = ngRigGet(state,"shoulder_pairs",JSONValue(cast(JSONValue[])null)).array.dup;
    foreach (ref pair; pairs) {
        if (!pair["matching"].boolean) { pair["status"] = JSONValue("not_matching"); continue; }
        ngRigCheckpoint(task);
        Point2[] sourcePoints, targetPoints;
        JSONValue beforeSource, beforeTarget;
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto source = cast(Part)puppet.find!Node(uuid(pair["source"]));
            auto target = cast(Part)puppet.find!Node(uuid(pair["target"]));
            enforce(source !is null && target !is null,"Shoulder welding Part disappeared");
            beforeSource = textureMapping(source); beforeTarget = textureMapping(target);
            foreach (p; ngRigPoints(beforeSource["vertices"])) {
                auto point = source.transform.matrix*vec4(cast(float)p[0],cast(float)p[1],0,1);
                Point2 pWorld = [point.x,point.y]; sourcePoints ~= pWorld;
            }
            foreach (p; ngRigPoints(beforeTarget["vertices"])) {
                auto point = target.transform.matrix*vec4(cast(float)p[0],cast(float)p[1],0,1);
                Point2 pWorld = [point.x,point.y]; targetPoints ~= pWorld;
            }
        });
        ptrdiff_t[] indices; size_t count;
        foreach (i,p; sourcePoints) {
            if (i%256 == 0) ngRigCheckpoint(task);
            double nearest = double.infinity; ptrdiff_t index = -1;
            foreach (j,q; targetPoints) {
                double distance = (p[0]-q[0])^^2+(p[1]-q[1])^^2;
                if (distance<nearest) { nearest = distance; index = cast(ptrdiff_t)j; }
            }
            indices ~= nearest<16 ? index : -1;
            if (nearest<16) ++count;
        }
        if (!count) { pair["status"] = JSONValue("no_native_vertex_pairs"); continue; }
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto source = cast(Part)puppet.find!Node(uuid(pair["source"]));
            auto target = cast(Part)puppet.find!Node(uuid(pair["target"]));
            command(new AddWeldingCommand(target,0),editorContext([source]));
            bool found;
            foreach (link; source.welded) if (link.target is target) {
                enforce(link.indices == indices && link.weight == 0,"Native shoulder correspondence differs from prediction");
                found = true;
            }
            enforce(found && textureMapping(source) == beforeSource && textureMapping(target) == beforeTarget,
                "Shoulder welding changed native mesh arrays or failed to create its link");
        });
        pair["indices"] = JSONValue(indices); pair["weight"] = JSONValue(0.);
        pair["paired_vertices"] = JSONValue(count); pair["mesh_arrays_unchanged"] = JSONValue(true);
        pair["status"] = JSONValue("applied_readback_verified");
    }
    state["shoulder_pairs"] = JSONValue(pairs);
    return state;
}

private JSONValue applyCheekCorrections(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    // The original registered-template workflow does not run regularize_fixed_feet.py.
    // Its authored native curves remain the leg deformation authority.
    auto feet = "reference_template" in program.object ? JSONValue([
        "operations":JSONValue(cast(JSONValue[])null),"applicable":JSONValue(false),
        "reason":JSONValue("Registered native curves; original workflow has no fixed-foot override")]) :
        ngRigCompileFixedFootCorrections(state,program,task);
    foreach (operation; feet["operations"].array) {
        ngRigCheckpoint(task);
        auto offsets = nativeNumbers(operation["values"]), indices = ngRigNumbers(operation["key"]);
        task.runOnMainThread({
            auto puppet = incActivePuppet(); auto node = puppet.find!Node(uuid(operation["grid"]));
            auto parameter = parameterByName(puppet,operation["parameter"].str);
            auto context = editorContext([node]); context.parameters = [parameter];
            context.keyPoint = vec2u(cast(uint)indices[0],cast(uint)indices[1]); context.hasExplicitKeyPoint = true;
            // This is a computed post-bake field, like native GPU writeback.
            // Invalidating the same bone bake would replace the correction.
            command(new SetDeformBindingCommand("deform",offsets,true,false),context);
        });
    }
    settleDepthRefresh(task);
    state["fixed_foot_corrections"] = feet;
    auto report = ngRigCompileCheekCorrections(state,program,task);
    if (!report["applicable"].boolean) { state["shape_corrections"] = report; return state; }
    bool[ulong] bound;
    foreach (operation; report["operations"].array) foreach (value; ngRigNumbers(operation["values"]))
        if (value != 0) bound[ngRigUnsigned(operation["part"])] = true;
    string rootSignature;
    task.runOnMainThread({
        auto puppet = incActivePuppet(); auto parameter = parameterByName(puppet,"Face::Yaw-Pitch");
        enforce(parameter !is null,"Cheek correction parameter is missing");
        foreach (id,unused; bound) {
            auto part = puppet.find!Node(cast(uint)id);
            enforce(part !is null && parameter.getBinding(part,"deform") is null,
                "Cheek correction would overwrite an existing Part angle binding");
        }
        rootSignature = ngRigDigest(parseJSON(inToJson(puppet.find!Node(uuid(state["rigRoot"])))));
    });
    foreach (operation; report["operations"].array) {
        if (!(ngRigUnsigned(operation["part"]) in bound)) continue;
        ngRigCheckpoint(task);
        auto offsets = nativeNumbers(operation["values"]), indices = ngRigNumbers(operation["key"]);
        task.runOnMainThread({
            auto puppet = incActivePuppet(); auto node = puppet.find!Node(uuid(operation["part"]));
            auto parameter = parameterByName(puppet,"Face::Yaw-Pitch");
            auto context = editorContext([node]); context.parameters = [parameter];
            context.keyPoint = vec2u(cast(uint)indices[0],cast(uint)indices[1]); context.hasExplicitKeyPoint = true;
            command(new SetDeformBindingCommand("deform",offsets,true),context);
            auto binding = cast(DeformationParameterBinding)parameter.getBinding(node,"deform");
            enforce(binding !is null && binding.isSet(context.keyPoint),"Cheek correction key readback is missing");
            auto actual = binding.getValue(context.keyPoint).vertexOffsets;
            enforce(actual.length*2 == offsets.length,"Cheek correction vertex count changed");
            foreach (i,p; actual) enforce(abs(p.x-offsets[i*2])<.0003 && abs(p.y-offsets[i*2+1])<.0003,
                "Cheek correction key differs from the compiled residual");
        });
    }
    task.runOnMainThread({
        auto puppet = incActivePuppet(); auto parameter = parameterByName(puppet,"Face::Yaw-Pitch");
        parameter.value = parameter.defaults; puppet.update();
        enforce(ngRigDigest(parseJSON(inToJson(puppet.find!Node(uuid(state["rigRoot"]))))) == rootSignature,
            "Cheek correction changed the native skeleton or depth authority");
        foreach (key; state["native_head_keys"].array) {
            auto grid = puppet.find!Node(uuid(key["grid"]));
            auto binding = cast(DeformationParameterBinding)parameter.getBinding(grid,"deform");
            auto index = ngRigNumbers(key["key"]), expected = ngRigNumbers(key["offsets"]);
            auto actual = binding.getValue(vec2u(cast(uint)index[0],cast(uint)index[1])).vertexOffsets;
            enforce(actual.length*2 == expected.length,"Cheek correction changed a native grid binding");
            foreach (i,p; actual) enforce(p.x == expected[i*2] && p.y == expected[i*2+1],
                "Cheek correction changed native depth-generated offsets");
        }
    });
    report["bound_targets"] = JSONValue(bound.keys);
    report["native_grid_depth_bones_unchanged"] = JSONValue(true);
    auto unsignedReport = report.object.dup; unsignedReport.remove("content_sha256");
    report["content_sha256"] = JSONValue(ngRigDigest(JSONValue(unsignedReport)));
    state["shape_corrections"] = report;
    state["shape_corrections_sha256"] = report["content_sha256"];
    return state;
}

private JSONValue validateDepthInputs(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    JSONValue[] surfaces;
    if (ngRigString(state,"kind","humanoid") != "humanoid") {
        state["depth_validation"] = JSONValue(["applicable":JSONValue(false),"reason":JSONValue("Local rig has no depth surfaces")]);
        return state;
    }
    double[4] bounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        auto root = cast(ExDepthRigRoot)puppet.find!Node(uuid(state["rigRoot"]));
        enforce(root !is null,"Depth input root is missing");
        foreach (target; state["targets"].array) {
            auto grid = cast(Deformable)puppet.find!Node(uuid(target["grid"]));
            auto mapped = cast(DepthMappedNode)grid;
            enforce(grid !is null && mapped !is null,"Depth input grid is missing");
            auto raw = mapped.copyDepths(); auto expected = ngRigNumbers(target["depth"]);
            enforce(raw.length == expected.length,"Stored depth length differs from the compiled program");
            foreach (i,depth; raw) enforce(isFinite(depth) && abs(depth-expected[i])<=1e-7,
                "Stored grid depth differs from the compiled program");
            auto transform = root.transform.matrix.inverse * grid.transform.matrix;
            double[] zs;
            foreach (vertex; grid.vertices) {
                auto point = transform*vec4(vertex.x,vertex.y,0,1);
                bounds[0] = min(bounds[0],point.x); bounds[1] = min(bounds[1],point.y);
                bounds[2] = max(bounds[2],point.x); bounds[3] = max(bounds[3],point.y);
                zs ~= point.z;
            }
            JSONValue[] sources;
            bool bound;
            foreach (binding; root.bindings) if (binding.targetUuid == grid.uuid) {
                bound = true;
                foreach (bone; binding.sourceBoneUuids) {
                    auto settings = binding.sourceSetting(bone);
                    enforce(isFinite(settings.depthScale) && isFinite(settings.depthOffset) &&
                        isFinite(settings.rotation) && isFinite(settings.weight),"Nonfinite depth source settings");
                    sources ~= JSONValue(["bone":JSONValue(bone),"depth_scale":JSONValue(settings.depthScale),
                        "depth_offset":JSONValue(settings.depthOffset),"rotation":JSONValue(settings.rotation),
                        "weight":JSONValue(settings.weight)]);
                }
            }
            enforce(bound && sources.length>0,"Depth grid has no bone sources");
            surfaces ~= JSONValue(["grid":target["grid"],"raw_depth":JSONValue(raw),
                "vertex_z":JSONValue(zs),"sources":JSONValue(sources)]);
        }
    });
    double observedScale = max(1.,max(bounds[2]-bounds[0],bounds[3]-bounds[1])*.42/2.9);
    double expectedScale = ngRigNumber(program["native_depth_scale"]);
    enforce(abs(observedScale-expectedScale)<=max(.001,observedScale*1e-5),
        "Observed native depth unit differs from the compiled scale");
    foreach (ref surface; surfaces) {
        auto raw = ngRigNumbers(surface["raw_depth"]), zs = ngRigNumbers(surface["vertex_z"]);
        foreach (ref source; surface["sources"].array) {
            double low = double.infinity, high = -double.infinity;
            foreach (i,depth; raw) {
                double effective = (depth*ngRigNumber(source["depth_scale"])+
                    ngRigNumber(source["depth_offset"]))*observedScale+zs[i];
                enforce(isFinite(effective),"Nonfinite effective depth");
                low = min(low,effective); high = max(high,effective);
            }
            source["effective_z_range_model"] = JSONValue([low,high]);
            source["effective_relief_model"] = JSONValue(high-low);
        }
    }
    state["depth_validation"] = JSONValue(["applicable":JSONValue(true),"depth_units_verified":JSONValue(true),
        "observed_scale":JSONValue(observedScale),"compiled_scale":JSONValue(expectedScale),
        "surfaces":JSONValue(surfaces),"visual_volume_accepted":JSONValue(false)]);
    return state;
}

private JSONValue bakeAngles(JSONValue state, AutoRigTaskContext task) {
    if (ngRigString(state,"kind","humanoid") != "humanoid") return state;
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        bool[uint] grids;
        foreach (target; state["targets"].array) grids[uuid(target["grid"])] = true;
        foreach (source; ngRigGet(state,"source_group_grids",JSONValue(cast(JSONValue[])null)).array)
            grids[uuid(source["grid"])] = true;
        // Replace automatically refreshed keys before the explicit native bake,
        // matching the original workflow's ownership and fresh-write checks.
        foreach (name; ["Face::Yaw-Pitch","Face::Roll","Body::Yaw-Pitch","Body::Roll"]) {
            auto parameter = parameterByName(puppet,name);
            enforce(parameter !is null,"Standard rig parameter missing: " ~ name);
            auto context = editorContext(); context.parameters = [parameter];
            foreach (binding; parameter.bindings)
                if (binding.getName() == "deform" && cast(uint)binding.getTarget().target.uuid in grids)
                    context.activeBindings = context.activeBindings ~ binding;
            if (context.activeBindings.length) command(new RemoveBindingCommand(),context);
        }
        foreach (parameter; puppet.parameters) parameter.value = parameter.defaults;
        puppet.update();
    });
    JSONValue[] headKeys;
    foreach (name; ["Face::Yaw-Pitch","Face::Roll","Body::Yaw-Pitch","Body::Roll"]) {
        size_t nx, ny;
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto parameter = parameterByName(puppet,name);
            enforce(parameter !is null, "Standard rig parameter missing: " ~ name);
            nx = parameter.axisPoints[0].length; ny = parameter.axisPoints[1].length;
        });
        foreach (x; 0 .. nx) foreach (y; 0 .. ny) {
            ngRigCheckpoint(task);
            CommandResult result;
            task.runOnMainThread({
                auto puppet = incActivePuppet(); auto parameter = parameterByName(puppet,name);
                foreach (other; puppet.parameters) other.value = other.defaults;
                parameter.value = parameter.unmapValue(vec2(parameter.axisPoints[0][x],parameter.axisPoints[1][y]));
                puppet.update();
                auto apply = new ApplyDepthBoneDeformCommand(); apply.root = puppet.find!Node(uuid(state["rigRoot"]));
                // An empty target list lets the native command include every
                // registered target in one parent/descendant hierarchy batch.
                auto context = editorContext(); context.armedParameters = [parameter];
                result = command(apply,context);
            });
            result = result.waitForCompletion(); enforce(result.succeeded,result.message);
            settleDepthRefresh(task);
            if (name == "Face::Yaw-Pitch") task.runOnMainThread({
                auto puppet = incActivePuppet(); auto parameter = parameterByName(puppet,name);
                auto root = cast(ExDepthRigRoot)puppet.find!Node(uuid(state["rigRoot"]));
                ulong head; foreach (bone; root.depthBones()) if (bone.boneId == "Head") head = bone.uuid;
                auto index = vec2u(cast(uint)x,cast(uint)y);
                auto projection = ngDepthBoneKeyProjection(root,head,parameter,index);
                bool[uint] seen;
                foreach (target; state["targets"].array) if (target["owner"].str == "head" && !(uuid(target["grid"]) in seen)) {
                    auto grid = puppet.find!Node(uuid(target["grid"]));
                    auto binding = cast(DeformationParameterBinding)parameter.getBinding(grid,"deform");
                    import std.format : format;
                    enforce(binding !is null && binding.isSet(index),
                        "Face bake readback is missing: grid=%s key=(%s,%s)".format(grid.uuid,x,y));
                    double[] offsets;
                    foreach (offset; binding.getValue(index).vertexOffsets) { offsets ~= offset.x; offsets ~= offset.y; }
                    auto value = parameter.unmapValue(vec2(parameter.axisPoints[0][x],parameter.axisPoints[1][y]));
                    headKeys ~= JSONValue(["grid":target["grid"],"key":JSONValue([x,y]),
                        "value":JSONValue([value.x,value.y]),"offsets":JSONValue(offsets),"projection":JSONValue(projection)]);
                    seen[uuid(target["grid"])] = true;
                }
            });
        }
        task.runOnMainThread({
            auto puppet = incActivePuppet(); parameterByName(puppet,name).value = vec2(0,0); puppet.update();
        });
    }
    state["native_head_keys"] = JSONValue(headKeys);
    state["depth_angle_program_sha256"] = JSONValue(ngRigDigest(JSONValue(headKeys)));
    return state;
}

/** Read all live state on the editor thread, then validate only copied numbers in the Fiber. */
private JSONValue verifyRig(JSONValue state, JSONValue program, AutoRigTaskContext task) {
    enforce(state["program_sha256"].str == program["content_sha256"].str, "Rig program ownership mismatch");
    JSONValue[] grids, localPoses, findings;
    size_t boneCount, keyCount, intermediateCount;
    bool humanoid = ngRigString(state,"kind","humanoid") == "humanoid";
    string snapshotSignature;
    auto sourceFrames = verifySourceFrames(state,task);
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        snapshotSignature = editorSignature(puppet);
        auto root = cast(ExDepthRigRoot)puppet.find!Node(uuid(state["rigRoot"]));
        enforce(root !is null, "Saved DepthRigRoot is missing"); boneCount = root.depthBones().length;
        enforce(boneCount == (humanoid ? program["scaffold"]["bones"].array.length : 0),
            "Saved skeleton differs from the compiled rig kind");
        if (humanoid) {
            foreach (source; ngRigGet(state,"source_group_grids",JSONValue(cast(JSONValue[])null)).array) {
                auto innerIndex = root.findBindingIndex(uuid(source["grid"]));
                auto surfaceIndex = root.findBindingIndex(uuid(source["surface"]));
                enforce(innerIndex>=0 && surfaceIndex>=0,"Saved imported group grid has no bone binding");
                auto inner = root.bindings[cast(size_t)innerIndex];
                auto surface = root.bindings[cast(size_t)surfaceIndex];
                enforce(inner.sourceBoneUuids == surface.sourceBoneUuids && inner.influenceRule == surface.influenceRule,
                    "Saved imported group grid differs from its semantic surface influence");
            }
            auto originIds = state["material_origins"];
            foreach (definition; program["hierarchy"]["groups"].array) {
                auto origin = puppet.find!Node(uuid(originIds[definition["id"].str]));
                enforce(origin !is null && origin.parent !is null &&
                    origin.parent.uuid == uuid(definition["parent"]["node"]),"Saved material origin parent differs from the semantic plan");
                auto point = toRoot(origin,0,0), expected = ngRigNumbers(definition["origin"]);
                enforce(abs(point[0]-expected[0])<.001 && abs(point[1]-expected[1])<.001,
                    "Saved material origin differs from its anatomical joint");
            }
            bool[string] checked;
            foreach (target; state["targets"].array) {
                auto domain = target["domain_id"].str;
                auto grid = puppet.find!Node(uuid(target["grid"]));
                if ((domain in checked) is null) {
                    auto parent = program["hierarchy"]["surface_parents"][domain];
                    uint expectedParent = "group" in parent.object ? uuid(originIds[parent["group"].str]) : uuid(parent["node"]);
                    enforce(grid !is null && grid.parent !is null && grid.parent.uuid == expectedParent,
                        "Saved shared surface parent differs from the semantic plan");
                    checked[domain] = true;
                }
                if (target["owner"].str == "head" && target["chart"].str == "face") {
                    bool inherited;
                    for (auto cursor = puppet.find!Node(uuid(target["part"])); cursor !is null; cursor = cursor.parent)
                        if (cursor.uuid == uuid(program["hierarchy"]["face_origin"])) inherited = true;
                    enforce(inherited,"Facial mechanism escaped the face material origin");
                }
            }
            foreach (definition; program["scaffold"]["bones"].array) {
                auto bone = root.depthBones()[0]; bool found;
                foreach (candidate; root.depthBones()) if (candidate.boneId == definition["id"].str) {
                    bone = candidate; found = true; break;
                }
                enforce(found,"Saved scaffold bone is missing");
                auto head = ngRigNumbers(definition["head"]), tail = ngRigNumbers(definition["tail"]);
                double[3] actualHead = [bone.restHead.x,bone.restHead.y,bone.restHead.z];
                double[3] actualTail = [bone.restTail.x,bone.restTail.y,bone.restTail.z];
                foreach (axis; 0 .. 3) enforce(abs(actualHead[axis]-head[axis])<.001 &&
                    abs(actualTail[axis]-tail[axis])<.001,"Saved scaffold rest pose changed");
                enforce(abs(bone.restRoll-ngRigNumber(definition["rest_roll"]))<1e-6,"Saved scaffold rest roll changed");
                enforce(bone.allowParentToTargets == definition["allow_parent_to_targets"].boolean,
                    "Saved bone support inheritance changed");
                enforce(bone.lockToRoot == definition["lock_to_root"].boolean,
                    "Saved bone root attachment changed");
                auto data=parseJSON(inToJson(bone));
                auto expectedZ=ngRigNumber(definition["pose_origin_z"]);
                if (!definition["lock_to_root"].boolean && definition["parent"].type!=JSONType.null_)
                    foreach (parent; program["scaffold"]["bones"].array) if (parent["id"]==definition["parent"])
                        expectedZ-=ngRigNumber(parent["pose_origin_z"]);
                enforce(abs(ngRigNumber(data["transform"]["trans"][2])-expectedZ)<.001,
                    "Saved bone Z origin differs from the registered template");
            }
            foreach (driver; program["native_drivers"].array) {
                auto parameter = parameterByName(puppet,driver["parameter"].str);
                Node bone;
                foreach (candidate; root.depthBones()) if (candidate.boneId == driver["bone"].str) bone = candidate;
                auto binding = cast(ValueParameterBinding)parameter.getBinding(bone,driver["binding"].str);
                enforce(binding !is null,"Saved native driver is missing");
                foreach (value; driver["values"].array) {
                    auto point = ngRigNumbers(value["key"]); bool matched;
                    foreach (y; 0 .. parameter.axisPoints[1].length) foreach (x; 0 .. parameter.axisPoints[0].length) {
                        auto actualPoint = parameter.unmapValue(vec2(parameter.axisPoints[0][x],parameter.axisPoints[1][y]));
                        if (abs(actualPoint.x-point[0])>1e-6 || abs(actualPoint.y-point[1])>1e-6) continue;
                        auto index = vec2u(cast(uint)x,cast(uint)y);
                        // The command stores native float values. Compare to the
                        // same conversion, not an unrepresentable double target.
                        auto expected = cast(float)ngRigNumber(value["value"]);
                        enforce(binding.isSet(index) && abs(binding.getValue(index)-expected)<1e-6,
                            "Saved native driver value changed"); matched = true;
                    }
                    enforce(matched,"Saved native driver axis changed");
                }
            }
        }
        foreach (composite; ngRigGet(state,"feature_composites",JSONValue(cast(JSONValue[])null)).array) {
            auto node = cast(Projectable)puppet.find!Node(uuid(composite["uuid"]));
            if (node !is null && !sameTextureMapping(textureMapping(node),composite["mapping"]))
                task.previewJson("composite-mesh-mismatch",JSONValue(["uuid":composite["uuid"],
                    "expected":composite["mapping"],"actual":textureMapping(node)]));
            enforce(node !is null && !node.autoResizedMesh && sameTextureMapping(textureMapping(node),composite["mapping"]),
                "Saved feature composite mesh changed");
        }
    });
    bool[uint] inspectedGrids;
    foreach (target; state["targets"].array) {
        ngRigCheckpoint(task);
        task.runOnMainThread({
            auto puppet = incActivePuppet();
            auto part = cast(Part)puppet.find!Node(uuid(target["part"]));
            enforce(part !is null, "Saved material is missing");
            enforce(sameTextureMapping(textureMapping(part),target["mapping"]), "Saved Part geometry or UV mapping changed after meshing");
            if (!humanoid) return;
            if (uuid(target["grid"]) in inspectedGrids) return;
            inspectedGrids[uuid(target["grid"])] = true;
            auto grid = cast(Deformable)puppet.find!Node(uuid(target["grid"]));
            auto mapped = cast(DepthMappedNode)grid;
            enforce(part !is null && grid !is null && mapped !is null, "Saved material or carrier is missing");
            enforce(sameTextureMapping(textureMapping(part),target["mapping"]), "Saved Part geometry or UV mapping changed after meshing");
            enforce(mapped.copyDepths().length == grid.vertices.length, "Saved grid depth count mismatch");
            Point2[] rest;
            foreach (p; grid.vertices) rest ~= Point2.init;
            foreach (i; 0 .. grid.vertices.length) rest[i] = [grid.vertices[i].x,grid.vertices[i].y];
            JSONValue[] keys;
            foreach (name; ["Face::Yaw-Pitch","Face::Roll","Body::Yaw-Pitch","Body::Roll"]) {
                auto parameter = parameterByName(puppet,name);
                auto binding = cast(DeformationParameterBinding)parameter.getBinding(grid,"deform");
                enforce(binding !is null, "Saved angle carrier binding missing: " ~ name);
                foreach (y; 0 .. parameter.axisPoints[1].length) foreach (x; 0 .. parameter.axisPoints[0].length) {
                    auto index = vec2u(cast(uint)x,cast(uint)y);
                    enforce(binding.isSet(index), "Saved angle key was not baked: " ~ name);
                    auto offsets = binding.getValue(index).vertexOffsets;
                    enforce(offsets.length == rest.length, "Saved angle key vertex count mismatch");
                    Point2[] posed;
                    foreach (i, p; rest) {
                        Point2 point = [p[0]+offsets[i].x,p[1]+offsets[i].y]; posed ~= point;
                    }
                    keys ~= JSONValue(["parameter":JSONValue(name),"key":JSONValue([x,y]),"points":ngRigPointsJson(posed)]);
                    ++keyCount;
                }
                foreach (y; 0 .. max(1,parameter.axisPoints[1].length-1))
                    foreach (x; 0 .. parameter.axisPoints[0].length-1)
                        foreach (fx; [.25f,.5f,.75f]) foreach (fy; parameter.isVec2 ? [.25f,.5f,.75f] : [0.0f]) {
                            auto offsets = binding.interpolate(vec2u(cast(uint)x,cast(uint)y),vec2(fx,fy)).vertexOffsets;
                            enforce(offsets.length == rest.length,"Saved intermediate angle vertex count mismatch");
                            Point2[] posed;
                            foreach (i,p; rest) { Point2 q = [p[0]+offsets[i].x,p[1]+offsets[i].y]; posed ~= q; }
                            keys ~= JSONValue(["parameter":JSONValue(name),"key":JSONValue([x+fx,y+fy]),
                                "points":ngRigPointsJson(posed),"intermediate":JSONValue(true)]);
                            ++intermediateCount;
                        }
            }
            foreach (pair; [["Face::Yaw-Pitch","Body::Yaw-Pitch"],
                ["Face::Roll","Body::Roll"]]) foreach (a; [-1.0f,0.0f,1.0f]) foreach (b; [-1.0f,0.0f,1.0f]) {
                Point2[] posed = rest.dup;
                foreach (j,name; pair) {
                    auto parameter = parameterByName(puppet,name);
                    auto binding = cast(DeformationParameterBinding)parameter.getBinding(grid,"deform");
                    float value = j == 0 ? a : b;
                    auto coordinate = parameter.mapAxis(0,value);
                    size_t index;
                    while (index+2<parameter.axisPoints[0].length && coordinate>parameter.axisPoints[0][index+1]) ++index;
                    float fraction = (coordinate-parameter.axisPoints[0][index])/
                        (parameter.axisPoints[0][index+1]-parameter.axisPoints[0][index]);
                    uint row = parameter.isVec2 ? cast(uint)parameter.getClosestAxisPointIndex(1,parameter.mapAxis(1,0)) : 0;
                    auto offsets = binding.interpolate(vec2u(cast(uint)index,row),vec2(fraction,0)).vertexOffsets;
                    enforce(offsets.length == posed.length,"Combined angle sample vertex count mismatch");
                    foreach (i,p; offsets) { posed[i][0] += p.x; posed[i][1] += p.y; }
                }
                keys ~= JSONValue(["parameter":JSONValue(pair[0] ~ " + " ~ pair[1]),
                    "key":JSONValue([a,b]),"points":ngRigPointsJson(posed),"combined":JSONValue(true)]);
                ++intermediateCount;
            }
            grids ~= JSONValue(["part":target["part"],"columns":JSONValue(target["xs"].array.length),
                "rest":ngRigPointsJson(rest),"keys":JSONValue(keys),"depth":JSONValue(mapped.copyDepths())]);
        });
    }
    foreach (mechanism; state["controls"]["mechanisms"].array) {
        foreach (operation; mechanism["operations"].array) {
            ngRigCheckpoint(task);
            task.runOnMainThread({
                auto puppet = incActivePuppet();
                auto parameter = parameterByName(puppet,mechanism["name"].str);
                auto node = puppet.find!Node(uuid(operation["part"]));
                auto binding = cast(DeformationParameterBinding)parameter.getBinding(node,"deform");
                enforce(binding !is null, "Saved local control binding is missing");
                foreach (key; operation["keys"].array) {
                    auto index = ngRigNumbers(key["key"]); auto kp = vec2u(cast(uint)index[0],cast(uint)index[1]);
                    enforce(binding.isSet(kp), "Saved local control key is missing");
                    auto expected = ngRigNumbers(key["offsets"]), actual = binding.getValue(kp).vertexOffsets;
                    enforce(actual.length*2 == expected.length, "Saved local control offsets have changed");
                    foreach (i; 0 .. actual.length) enforce(abs(actual[i].x-expected[i*2])<1e-4*(1+abs(expected[i*2])) &&
                        abs(actual[i].y-expected[i*2+1])<1e-4*(1+abs(expected[i*2+1])), "Saved local control value mismatch");
                }
                auto part = cast(Part)node;
                enforce(part !is null,"Local control target is not a Part");
                JSONValue[] samples;
                foreach (y; 0 .. max(1,parameter.axisPoints[1].length-1))
                    foreach (x; 0 .. parameter.axisPoints[0].length-1)
                        foreach (fx; [.25f,.5f,.75f]) foreach (fy; parameter.isVec2 ? [.25f,.5f,.75f] : [0.0f]) {
                            auto value = binding.interpolate(vec2u(cast(uint)x,cast(uint)y),vec2(fx,fy)).vertexOffsets;
                            double[] offsets; foreach (p; value) { offsets ~= p.x; offsets ~= p.y; }
                            samples ~= JSONValue(["key":JSONValue([x+fx,y+fy]),"offsets":JSONValue(offsets)]);
                            ++intermediateCount;
                        }
                localPoses ~= JSONValue(["part":operation["part"],"parameter":mechanism["name"],
                    "mapping":textureMapping(part),"samples":JSONValue(samples)]);
            });
        }
    }
    task.runOnMainThread({
        auto puppet = incActivePuppet();
        foreach (key; ngRigGet(state,"native_head_keys",JSONValue(cast(JSONValue[])null)).array) {
            auto parameter = parameterByName(puppet,"Face::Yaw-Pitch");
            auto grid = puppet.find!Node(uuid(key["grid"]));
            auto binding = cast(DeformationParameterBinding)parameter.getBinding(grid,"deform");
            auto indices = ngRigNumbers(key["key"]), expected = ngRigNumbers(key["offsets"]);
            auto actual = binding.getValue(vec2u(cast(uint)indices[0],cast(uint)indices[1])).vertexOffsets;
            enforce(actual.length*2 == expected.length,"Saved native head bake vertex count changed");
            foreach (i,p; actual) enforce(abs(p.x-expected[i*2])<.0003 && abs(p.y-expected[i*2+1])<.0003,
                "Saved native head bake changed after shape correction");
        }
        if (auto feet = "fixed_foot_corrections" in state.object) foreach (operation; (*feet)["operations"].array) {
            auto node = puppet.find!Node(uuid(operation["grid"]));
            auto parameter = parameterByName(puppet,operation["parameter"].str);
            auto binding = cast(DeformationParameterBinding)parameter.getBinding(node,"deform");
            auto indices = ngRigNumbers(operation["key"]), expected = ngRigNumbers(operation["values"]);
            auto kp = vec2u(cast(uint)indices[0],cast(uint)indices[1]);
            enforce(binding !is null && binding.isSet(kp),"Saved fixed-foot key is missing");
            auto actual = binding.getValue(kp).vertexOffsets;
            enforce(actual.length*2 == expected.length,"Saved fixed-foot vertex count changed");
            foreach (i,p; actual) enforce(abs(p.x-expected[i*2])<.0003 && abs(p.y-expected[i*2+1])<.0003,
                "Saved fixed-foot field differs from the compiled correction");
        }
        auto corrections = ngRigGet(state,"shape_corrections",JSONValue(["applicable":JSONValue(false)]));
        if (corrections["applicable"].boolean) {
            auto unsignedReport = corrections.object.dup; unsignedReport.remove("content_sha256");
            enforce(ngRigDigest(JSONValue(unsignedReport)) == state["shape_corrections_sha256"].str,
                "Saved shape correction report ownership mismatch");
            auto parameter = parameterByName(puppet,"Face::Yaw-Pitch");
            foreach (operation; corrections["operations"].array) {
                bool bound;
                foreach (id; corrections["bound_targets"].array)
                    if (ngRigUnsigned(id) == ngRigUnsigned(operation["part"])) bound = true;
                if (!bound) continue;
                auto part = puppet.find!Node(uuid(operation["part"]));
                auto binding = cast(DeformationParameterBinding)parameter.getBinding(part,"deform");
                auto index = ngRigNumbers(operation["key"]), expected = ngRigNumbers(operation["values"]);
                auto kp = vec2u(cast(uint)index[0],cast(uint)index[1]);
                enforce(binding !is null && binding.isSet(kp),"Saved cheek residual key is missing");
                auto actual = binding.getValue(kp).vertexOffsets;
                enforce(actual.length*2 == expected.length,"Saved cheek residual vertex count changed");
                foreach (i,p; actual) enforce(abs(p.x-expected[i*2])<.0003 && abs(p.y-expected[i*2+1])<.0003,
                    "Saved cheek residual value changed");
            }
        }
        foreach (pair; ngRigGet(state,"shoulder_pairs",JSONValue(cast(JSONValue[])null)).array) {
            if (ngRigString(pair,"status","") != "applied_readback_verified") continue;
            auto source = cast(Part)puppet.find!Node(uuid(pair["source"]));
            auto target = cast(Part)puppet.find!Node(uuid(pair["target"]));
            enforce(source !is null && target !is null,"Saved shoulder link Part is missing");
            bool found;
            foreach (link; source.welded) if (link.target is target) {
                auto expected = ngRigNumbers(pair["indices"]);
                enforce(link.indices.length == expected.length && link.weight == 0,"Saved shoulder link has changed");
                foreach (i,value; link.indices) enforce(value == expected[i],"Saved shoulder correspondence has changed");
                found = true;
            }
            enforce(found,"Saved shoulder welding link is missing");
        }
        enforce(editorSignature(puppet) == snapshotSignature,"Model changed during saved rig snapshots");
    });
    foreach (grid; grids) {
        auto depth = ngRigNumbers(grid["depth"]);
        auto rest=ngRigPoints(grid["rest"]);
        ngRigValidateGrid(rest,cast(size_t)ngRigNumber(grid["columns"]),0);
        foreach (key; grid["keys"].array) {
            ngRigCheckpoint(task);
            try { ngRigValidateGrid(ngRigPoints(key["points"]),cast(size_t)ngRigNumber(grid["columns"]),.02,rest); }
            catch (Exception error) findings ~= JSONValue(["part":grid["part"],"parameter":key["parameter"],
                "key":key["key"],"message":JSONValue(error.msg)]);
        }
    }
    foreach (local; localPoses) foreach (sample; local["samples"].array) {
        ngRigCheckpoint(task);
        try {
            double ratio = ngRigTriangleMinimumRatio(ngRigPoints(local["mapping"]["vertices"]),
                local["mapping"]["triangles"],ngRigNumbers(sample["offsets"]));
            if (!local["parameter"].str.endsWith("::Blink")) enforce(ratio>=.002,"Intermediate local control folded a triangle");
        } catch (Exception error) findings ~= JSONValue(["part":local["part"],"parameter":local["parameter"],
            "key":sample["key"],"message":JSONValue(error.msg)]);
    }
    auto report = JSONValue(["schema_version":JSONValue("rig-saved-validation-d/1"),
        "passed":JSONValue(findings.length == 0),"bones":JSONValue(boneCount),"baked_keys":JSONValue(keyCount),
        "intermediate_samples":JSONValue(intermediateCount),
        "numerical_findings":JSONValue(findings),"source_sha256":state["source_sha256"],
        "source_uv_readback":sourceFrames,
        "readback_verified":JSONValue(true),
        "program_sha256":program["content_sha256"],"model_sha256":state["model_sha256"],
        "visual_review_required":JSONValue(true)]);
    auto renders = renderValidation(state,task); report["renders"] = renders;
    if (renders["applicable"].boolean && !renders["neutral_passed"].boolean)
        findings ~= JSONValue(["message":JSONValue("Neutral render differs from the source")]);
    report["passed"] = JSONValue(findings.length == 0); report["numerical_findings"] = JSONValue(findings);
    if (findings.length) task.previewJson("validation-findings",report);
    // The original finishing workflow records numerical findings and exports
    // every pose. Structural/readback failures still throw above.
    return report;
}

/** Model snapshots are retained in memory and restored only for retries or rollback. */
JSONValue ngRigNativeStage(string stage, JSONValue state, JSONValue program, ubyte[] model,
    AutoRigTaskContext task) {
    ngRigCheckpoint(task);
    if (stage == "observe-model") state = observeModel(state,task);
    else {
        enforce(model.length > 0, "Missing committed model snapshot");
        enforce(sha256Of(model).toHexString.idup == state["model_sha256"].str, "Model snapshot hash mismatch");
        task.runOnMainThread({
            auto current = incActivePuppet();
            enforce(current !is null && current.root.uuid == uuid(state["rootId"]),
                "Active model changed since the previous AutoRig stage");
            auto signature = editorSignature(current);
            if (signature != state["editor_signature"].str) {
                auto expected = task.sessionId() in liveRigSignatures;
                enforce(expected !is null && signature == *expected,
                    "Editor model changed since the previous AutoRig stage; regenerate its observation");
                ngRestorePuppetMemory(model);
            }
        });
    }
    if (stage == "verify-saved-rig") return verifyRig(state,program,task);
    bool grouped;
    task.runOnMainThread({ incActionPushGroup(); grouped = true; });
    scope(exit) if (grouped) task.runOnMainThread({ incActionPopGroup(); });
    scope(failure) if (model.length) task.runOnMainThread({
        ngRestorePuppetMemory(model);
        liveRigSignatures[task.sessionId()] = editorSignature(incActivePuppet());
    });
    if (stage == "mesh-parts") state = meshParts(state,program,task);
    else if (stage == "prepare-shoulders") state["shoulder_pairs"] = ngRigPrepareShoulders(state,program,task);
    else if (stage == "prepare-source-groups") state = prepareSourceGroups(state,task);
    else if (stage == "register-source-uv") state = registerSourceUV(state,task);
    else if (stage == "prepare-feature-composites") state = prepareFeatureComposites(state,program,task);
    else if (stage == "compile-domain-layout") state = registerDomainParents(state,program,task);
    else if (stage == "build-native-rig") state = buildRig(state,program,task);
    else if (stage == "weld-shoulders") state = weldShoulders(state,task);
    else if (stage == "validate-depth-inputs") state = validateDepthInputs(state,program,task);
    else if (stage == "bake-depth-angles") state = bakeAngles(state,task);
    else if (stage == "apply-rig-controls") state = applyControls(state,task);
    else if (stage == "apply-shape-corrections") state = applyCheekCorrections(state,program,task);
    else enforce(stage == "observe-model", "Unknown native rig stage: " ~ stage);
    ngRigCheckpoint(task);
    settleDepthRefresh(task);
    ubyte[] snapshot;
    task.runOnMainThread({
        snapshot = inWriteINPPuppetMemory(incActivePuppet());
        state["editor_signature"] = JSONValue(editorSignature(incActivePuppet()));
        liveRigSignatures[task.sessionId()] = state["editor_signature"].str;
    });
    state["model_sha256"] = JSONValue(sha256Of(snapshot).toHexString.idup);
    state["completed_stage"] = JSONValue(stage);
    task.publishBlob("model",snapshot);
    return state;
}
