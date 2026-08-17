module nijigenerate.viewport.depth.draw.persistence;

import nijigenerate.ext : ExPuppet;
import nijigenerate.viewport.depth.draw.manifest;
import nijigenerate.viewport.depth.draw.session;
import nijilive;
import std.json : parseJSON;

string ngDepthDrawSessionToPersistentJson(DepthDrawSession session) {
    return ngDepthDrawSessionToManifest(session).toString();
}

DepthDrawSession ngDepthDrawSessionFromPersistentJson(string value) {
    if (value.length == 0) return null;
    return ngDepthDrawSessionFromManifest(parseJSON(value));
}

bool ngSetPuppetDepthDrawSession(Puppet puppet, DepthDrawSession session) {
    auto exPuppet = cast(ExPuppet)puppet;
    if (exPuppet is null) return false;
    exPuppet.depthDrawSessionManifestJson = session is null ? null : ngDepthDrawSessionToPersistentJson(session);
    return true;
}

DepthDrawSession ngGetPuppetDepthDrawSession(Puppet puppet) {
    auto exPuppet = cast(ExPuppet)puppet;
    if (exPuppet is null || exPuppet.depthDrawSessionManifestJson.length == 0) return null;
    return ngDepthDrawSessionFromPersistentJson(exPuppet.depthDrawSessionManifestJson);
}

bool ngClearPuppetDepthDrawSession(Puppet puppet) {
    auto exPuppet = cast(ExPuppet)puppet;
    if (exPuppet is null) return false;
    exPuppet.depthDrawSessionManifestJson = null;
    return true;
}
