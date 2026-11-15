#include "core.hpp"
#include "detection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace nijigenerate::autorig::patterns {
namespace {
using Object = boost::json::object;
using Array = boost::json::array;
using Id = std::uint32_t;
void require(bool ok, std::string_view message) {
    if (!ok) throw std::invalid_argument(std::string(message));
}
void check(const Checkpoint& checkpoint) { if (checkpoint) checkpoint(); }
const Json& get(const Json& value, std::string_view key) {
    require(value.is_object(), "Expected an object");
    const auto* result = value.as_object().if_contains(key);
    require(result != nullptr, "Required field is missing: " + std::string(key));
    return *result;
}
Json optional(const Json& value, std::string_view key, Json fallback = {}) {
    if (value.is_object()) if (auto* found = value.as_object().if_contains(key)) return *found;
    return fallback;
}
const Array& array(const Json& value, std::string_view key) {
    static const Array empty;
    if (value.is_object()) if (const auto* found = value.as_object().if_contains(key))
        if (!found->is_null()) return found->as_array();
    return empty;
}
std::string string(const Json& value) {
    require(value.is_string(), "Expected a string");
    return std::string(value.as_string());
}
double number(const Json& value) {
    require(value.is_number(), "Expected a number");
    const double result = value.to_number<double>();
    require(std::isfinite(result), "Nonfinite number");
    return result;
}
Id id(const Json& value) {
    const double result = number(value);
    require(result > 0 && result < std::numeric_limits<Id>::max() && std::floor(result) == result,
        "Invalid UUID");
    return static_cast<Id>(result);
}
void finite(const Json& value) {
    if (value.is_number()) (void)number(value);
    if (value.is_array()) for (const auto& item : value.as_array()) finite(item);
    if (value.is_object()) for (const auto& item : value.as_object()) finite(item.value());
}
struct Index {
    std::map<Id, const Json*> nodes, parameters;
    std::map<Id, Id> parents;
    explicit Index(const Json& document) {
        visit(get(document, "nodes"), 0);
        for (const auto& parameter : array(document, "param")) visitParameter(parameter);
        for (const auto& [uuid, parameter] : parameters)
            require(!nodes.contains(uuid), "Parameter and Node UUID collision");
    }
    void visit(const Json& node, Id parent) {
        const auto uuid = id(get(node, "uuid"));
        require(nodes.emplace(uuid, &node).second, "Duplicate Node UUID");
        parents[uuid] = parent;
        for (const auto& child : array(node, "children")) visit(child, uuid);
    }
    void visitParameter(const Json& value) {
        if (value.as_object().contains("groupUUID")) {
            for (const auto& child : array(value, "children")) visitParameter(child);
        } else require(parameters.emplace(id(get(value, "uuid")), &value).second, "Duplicate Parameter UUID");
    }
    const Json& node(Id uuid) const {
        require(nodes.contains(uuid), "Selected Node does not exist");
        return *nodes.at(uuid);
    }
    bool part(Id uuid) const { return string(get(node(uuid), "type")) == "Part"; }
};
Json* findNode(Json& node, Id uuid) {
    if (id(get(node, "uuid")) == uuid) return &node;
    if (auto* children = node.as_object().if_contains("children"))
        for (auto& child : children->as_array()) if (auto* found = findNode(child, uuid)) return found;
    return nullptr;
}
Json* findParameter(Array& parameters, Id uuid) {
    for (auto& parameter : parameters) {
        if (parameter.as_object().contains("groupUUID")) {
            if (auto* result = findParameter(parameter.as_object().at("children").as_array(), uuid)) return result;
        } else if (id(get(parameter, "uuid")) == uuid) return &parameter;
    }
    return nullptr;
}
// Fingerprints bind detection output to an exact document. Labels are not detection features.
std::string digest(const Json& value) {
    const auto bytes = boost::json::serialize(value);
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : bytes) { hash ^= c; hash *= 1099511628211ull; }
    return std::to_string(hash);
}
std::vector<Id> roleIds(const Json& match, std::string_view role) {
    std::vector<Id> result;
    for (const auto& item : array(get(match, "roles"), role)) result.push_back(id(item));
    return result;
}
void validateMatch(const PatternSpec& spec, const Index& index, const Json& match) {
    const auto& roles = get(match, "roles").as_object();
    for (const auto& required : spec.roles) {
        require(roles.contains(required) && !roles.at(required).as_array().empty(),
            "Missing structural role: " + required);
    }
    for (const auto& role : roles) {
        require(std::find(spec.roles.begin(), spec.roles.end(), role.key()) != spec.roles.end(),
            "Unknown structural role");
        std::set<Id> unique;
        for (const auto& item : role.value().as_array()) {
            auto uuid = id(item);
            require(unique.insert(uuid).second, "Duplicate target within a role");
            const auto& node = index.node(uuid);
            (void)node;
            if (role.key() == "surface" || role.key() == "mask" || role.key() == "seam")
                require(index.part(uuid), "Surface, mask and seam roles require Part nodes");
        }
    }
    if (roles.contains("seam")) for (auto target : roleIds(match, "target"))
        for (auto seam : roleIds(match, "seam")) require(target != seam, "Self welding is not allowed");
    if (roles.contains("mask")) for (auto target : roleIds(match, "target"))
        for (auto mask : roleIds(match, "mask")) require(target != mask, "Self masking is not allowed");
}
bool hasBinding(const Index& index, Id uuid, std::string_view property) {
    for (const auto& [_, p] : index.parameters)
        for (const auto& b : array(*p, "bindings"))
            if (id(get(b, "node")) == uuid && get(b, "param_name").is_string() &&
                    string(get(b, "param_name")) == property) return true;
    return false;
}
std::vector<std::array<double, 2>> vertices(const Json& node) {
    const auto& values = get(get(node, "mesh"), "verts").as_array();
    require(values.size() >= 6 && values.size() % 2 == 0, "A deformable Part mesh is required");
    std::vector<std::array<double, 2>> points;
    for (std::size_t i = 0; i < values.size(); i += 2)
        points.push_back({number(values[i]), number(values[i + 1])});
    return points;
}
Json matchFor(const PatternSpec& spec, Id target, const Index& index) {
    Object roles{{"target", Array{target}}};
    const auto& node = index.node(target);
    for (const auto& role : spec.roles) {
        if (role == "target") continue;
        Array ids;
        if (role == "support" && index.parents.at(target)) ids.push_back(index.parents.at(target));
        if (role == "mask" || role == "surface")
            for (const auto& mask : array(node, "masks")) {
                auto source = id(get(mask, "source"));
                if (index.nodes.contains(source) && index.part(source) && source != target) ids.push_back(source);
            }
        if (role == "seam")
            for (const auto& weld : array(node, "weldedLinks")) {
                auto source = id(get(weld, "targetUUID"));
                if (index.nodes.contains(source) && index.part(source) && source != target) ids.push_back(source);
            }
        if (ids.empty()) return {};
        // Different mask edges can refer to the same source.
        std::set<Id> seen;
        Array unique;
        for (const auto& value : ids) if (seen.insert(id(value)).second) unique.push_back(value);
        roles[role] = std::move(unique);
    }
    return Object{{"roles", roles}};
}
std::vector<Id> selectedTargets(const Json& selection) {
    std::set<Id> seen;
    std::vector<Id> result;
    for (const auto& match : get(selection, "matches").as_array())
        for (auto uuid : roleIds(match, "target")) if (seen.insert(uuid).second) result.push_back(uuid);
    return result;
}
Json validateSelection(const PatternSpec& spec, const Json& document, const Json& selection) {
    require(string(get(selection, "schema")) == "autorig.pattern-targets/1", "Unknown selection schema");
    require(string(get(selection, "pattern")) == spec.id, "Selection belongs to another pattern");
    require(string(get(selection, "source")) == digest(document), "Stale target selection: rerun detection");
    Index index(document);
    for (const auto& match : get(selection, "matches").as_array()) validateMatch(spec, index, match);
    return selection;
}
Id unusedId(const Index& index, Id seed) {
    while (!seed || index.nodes.contains(seed) || index.parameters.contains(seed)) {
        require(seed < std::numeric_limits<Id>::max() - 1, "UUID space exhausted");
        ++seed;
    }
    return seed;
}
}

const std::vector<PatternSpec>& ngPatternSpecs() {
    static const std::vector<PatternSpec> specs{
        {"p01", "Transfer authored bindings", "transfer", {"target"}},
        {"p02", "Link parameter axes", "link", {"target"}},
        {"p03", "Cross-input residual", "residual", {"target"}},
        {"p04", "Surface transport", "surface", {"target", "surface"}},
        {"p05", "Local transform composition", "transform", {"target", "support"}},
        {"p06", "Bend and twist", "bend", {"target"}},
        {"p07", "Supported covering", "covering", {"target", "support"}},
        {"p08", "Moving silhouette mask", "mask", {"target", "mask"}},
        {"p09", "Excess surface clipping", "dodge", {"target", "mask"}},
        {"p10", "Mask network", "mask-network", {"target", "mask"}},
        {"p11", "Lower-alpha composition", "clip", {"target"}},
        {"p12", "Pose draw order", "zsort", {"target"}},
        {"p13", "Opacity response", "opacity", {"target"}},
        {"p14", "Surface lighting displacement", "lighting", {"target", "surface"}},
        {"p15", "Material state", "material", {"target"}},
        {"p16", "Boundary welding", "weld", {"target", "seam"}},
        {"p17", "Opening boundaries", "opening", {"target"}},
        {"p18", "Aperture follower", "follower", {"target", "support"}},
        {"p19", "Nested material oscillation", "oscillation", {"target"}},
        {"p20", "Opening width and position", "width", {"target"}},
        {"p21", "Expression state", "state", {"target"}},
        {"p22", "Independent articulation", "articulation", {"target"}},
        {"p23", "Branch articulation", "branches", {"target", "support"}},
        {"p24", "Supported expansion", "expansion", {"target"}},
        {"p25", "Supported soft volume", "soft-volume", {"target", "support"}},
        {"p26", "Suspended rigid region", "pendulum", {"target", "support"}},
        {"p27", "Ribbon network", "ribbon", {"target", "support"}},
        {"p28", "Multi-anchor curve", "multi-anchor", {"target", "support"}},
        {"p29", "Compose secondary modes", "modes", {"target"}},
        {"p30", "Moving physics frame", "physics-frame", {"target", "support"}},
        {"p31", "Physics output response", "gain", {"target"}},
        {"p32", "Local shape adjustment", "adjustment", {"target"}},
        {"p33", "Deformation scope", "scope", {"target"}},
        {"p34", "Protected and free regions", "region", {"target"}},
        {"p35", "Independent group translation", "translation", {"target"}}
    };
    return specs;
}
const PatternSpec& ngPatternSpec(std::string_view pattern) {
    for (const auto& spec : ngPatternSpecs()) if (spec.id == pattern) return spec;
    throw std::invalid_argument("Unknown structural pattern");
}

Json ngDetectPattern(std::string_view pattern, const Json& document, const Json& request, Checkpoint checkpoint) {
    const auto& spec = ngPatternSpec(pattern);
    Index index(document);
    Array matches, unresolved, candidates;
    const auto mode = string(optional(request, "mode", "auto"));
    require(mode == "auto" || mode == "manual", "Target mode must be auto or manual");
    if (mode == "manual") {
        // Empty explicit selection means no targets. Never fall back to auto here.
        matches = get(request, "matches").as_array();
        for (auto& match : matches) {
            check(checkpoint);
            validateMatch(spec, index, match);
            match.as_object()["provenance"] = "manual";
            match.as_object()["requires_review"] = false;
        }
    } else {
        if (!request.as_object().contains("condition")) {
            auto detected = ngDetectStructuralPatterns(document, checkpoint);
            const auto& entry = detected.as_object().at(pattern).as_object();
            matches = entry.at("matches").as_array();
            candidates = entry.at("candidates").as_array();
            for (const auto& match : matches) validateMatch(spec, index, match);
        }
        // Explicit structural annotations are evidence, not part-name aliases.
        for (const auto& [uuid, node] : index.nodes) {
            check(checkpoint);
            bool candidate = false;
            if (auto* query = request.as_object().if_contains("condition")) {
                require(query->is_object() && !query->as_object().empty(), "Empty structural condition");
                candidate = true;
                for (const auto& field : query->as_object()) {
                    if (field.key() == "type") candidate &= get(*node, "type") == field.value();
                    else if (field.key() == "parent_type") {
                        auto parent = index.parents.at(uuid);
                        candidate &= parent && get(index.node(parent), "type") == field.value();
                    } else if (field.key() == "binding") candidate &= hasBinding(index, uuid, string(field.value()));
                    else if (field.key() == "min_children") candidate &= array(*node, "children").size() >= number(field.value());
                    else if (field.key() == "min_masks") candidate &= array(*node, "masks").size() >= number(field.value());
                    else if (field.key() == "min_aspect") {
                        if (!index.part(uuid)) { candidate = false; continue; }
                        const auto points = vertices(*node);
                        auto low = points.front(), high = low;
                        for (auto p : points) for (int i = 0; i < 2; ++i) {
                            low[i] = std::min(low[i], p[i]); high[i] = std::max(high[i], p[i]);
                        }
                        auto w = high[0] - low[0], h = high[1] - low[1];
                        candidate &= std::min(w, h) > 1e-8 && std::max(w, h) / std::min(w, h) >= number(field.value());
                    } else require(false, "Unknown structural condition field");
                }
            }
            const bool conditionMatched = candidate;
            const auto structure = optional(*node, "autorig_structure", Object{});
            for (const auto& item : array(structure, "patterns"))
                candidate = candidate || string(item) == pattern;
            if (request.as_object().contains("condition")) candidate = conditionMatched;
            if (!candidate) continue;
            auto match = matchFor(spec, uuid, index);
            if (structure.as_object().contains("roles")) match = Object{{"roles", get(structure, "roles")}};
            if (match.is_null()) {
                unresolved.push_back(Object{{"uuid", uuid}, {"reason", "Missing structural relationship"}});
                continue;
            }
            validateMatch(spec, index, match);
            match.as_object()["provenance"] = request.as_object().contains("condition") ? "condition_or_structure" : "structure";
            bool duplicate = false;
            for (const auto& existing : matches) duplicate |= get(existing, "roles") == get(match, "roles");
            if (!duplicate) matches.push_back(std::move(match));
        }
    }
    Array parts, nodes;
    std::set<Id> seen;
    for (const auto& match : matches) for (const auto& role : get(match, "roles").as_object())
        for (const auto& item : role.value().as_array()) if (seen.insert(id(item)).second)
            (index.part(id(item)) ? parts : nodes).push_back(item);
    Json output = Object{{"schema", "autorig.pattern-targets/1"}, {"pattern", spec.id},
        {"source", digest(document)}, {"mode", mode}, {"parts", parts}, {"nodes", nodes},
        {"matches", matches}, {"unresolved", unresolved}, {"candidates", candidates},
        {"status", matches.empty() ? (candidates.empty() ? "no_match" : "needs_review") : "matched"},
        {"recognition_scope", "structural_graph_sampled_behavior_and_geometry"}, {"psd_semantics_inferred", false}};
    if (auto* intent = request.as_object().if_contains("intent")) output.as_object()["intent"] = *intent;
    output.as_object()["restricted_selection"] = request.as_object().contains("condition");
    output.as_object()["plan"] = ngPlanPattern(pattern, document, output, checkpoint);
    const auto plan = output.as_object().at("plan");
    if (get(plan, "status") == "ready") {
        output.as_object()["observed_matches"] = matches;
        output.as_object()["matches"] = get(plan, "matches");
        Array plannedParts, plannedNodes;
        std::set<Id> plannedIds;
        for (const auto& match : array(plan, "matches")) {
            validateMatch(spec, index, match);
            for (const auto& role : get(match, "roles").as_object())
                for (const auto& value : role.value().as_array()) if (plannedIds.insert(id(value)).second)
                    (index.part(id(value)) ? plannedParts : plannedNodes).push_back(value);
        }
        output.as_object()["parts"] = plannedParts;
        output.as_object()["nodes"] = plannedNodes;
        output.as_object()["status"] = "planned";
    }
    return output;
}

namespace {
bool contains(std::initializer_list<std::string_view> items, std::string_view value) {
    return std::find(items.begin(), items.end(), value) != items.end();
}
std::array<double, 2> point(const Json& value) {
    require(value.is_array() && value.as_array().size() == 2, "Expected a point");
    return {number(value.as_array()[0]), number(value.as_array()[1])};
}
Array axis(const Json& control, std::string_view name) {
    auto values = get(control, name).as_array();
    require(values.size() >= 2 && values.size() <= 65, "Axis needs 2..65 samples");
    double last = -std::numeric_limits<double>::infinity();
    bool neutral = false;
    for (const auto& value : values) {
        auto n = number(value);
        require(n > last, "Axis samples must be strictly increasing");
        last = n;
        neutral |= n == 0;
    }
    require(neutral, "Axis must contain neutral zero");
    return values;
}
Json sampledBinding(Id target, std::string_view property, const Array& xs, const Array& ys,
    const std::function<Json(double,double)>& sample) {
    Array values, set;
    for (const auto& x : xs) {
        Array row, flags;
        for (const auto& y : ys) { row.push_back(sample(number(x), number(y))); flags.push_back(true); }
        values.push_back(std::move(row)); set.push_back(std::move(flags));
    }
    return Object{{"node", target}, {"param_name", property}, {"values", values},
        {"isSet", set}, {"interpolate_mode", "Linear"}};
}
void validateBinding(const Index& index, const Json& parameter, const Json& binding) {
    auto target = id(get(binding, "node"));
    auto property = string(get(binding, "param_name"));
    const bool link = property == "X" || property == "Y";
    require(link ? index.parameters.contains(target) : index.nodes.contains(target), "Binding target does not exist");
    if (!link) {
        const bool transform = contains({"zSort", "transform.t.x", "transform.t.y", "transform.t.z",
            "transform.r.x", "transform.r.y", "transform.r.z", "transform.s.x", "transform.s.y"}, property);
        const bool material = contains({"opacity", "alphaThreshold", "tint.r", "tint.g", "tint.b",
            "screenTint.r", "screenTint.g", "screenTint.b", "emissionStrength"}, property);
        const bool physics = contains({"outputScale.x", "outputScale.y", "gravity", "length",
            "frequency", "angleDamping", "lengthDamping"}, property);
        require(transform || (index.part(target) && (material || property == "deform")) ||
            (physics && string(get(index.node(target), "type")) == "SimplePhysics"),
            "Unsupported property for selected node type");
    }
    const auto& axes = get(parameter, "axis_points").as_array();
    const auto& values = get(binding, "values").as_array();
    const auto& flags = get(binding, "isSet").as_array();
    require(axes.size() == 2 && values.size() == axes[0].as_array().size() && flags.size() == values.size(),
        "Binding X dimension mismatch");
    for (std::size_t x = 0; x < values.size(); ++x) {
        require(values[x].as_array().size() == axes[1].as_array().size() &&
            flags[x].as_array().size() == values[x].as_array().size(), "Binding Y dimension mismatch");
        for (std::size_t y = 0; y < values[x].as_array().size(); ++y) {
            require(flags[x].as_array()[y].is_bool(), "Invalid isSet cell");
            const auto& v = values[x].as_array()[y];
            if (property == "deform") {
                auto points = vertices(index.node(target));
                require(v.is_array() && v.as_array().size() == points.size(), "Deform vertex count mismatch");
                for (const auto& p : v.as_array()) (void)point(p);
            } else {
                const auto scalar = number(v);
                if (property == "opacity") require(scalar >= 0 && scalar <= 1, "Opacity outside [0,1]");
            }
        }
    }
}
std::array<double,2> displacement(std::string_view mechanism, std::array<double,2> p,
        double x, double y, const Json& control, std::size_t vertex) {
    auto origin = point(get(control, "origin"));
    auto direction = point(get(control, "direction"));
    double length = std::hypot(direction[0], direction[1]);
    require(length > 1e-8, "Degenerate local direction");
    direction[0] /= length; direction[1] /= length;
    const auto extent = number(get(control, "extent"));
    require(extent > 1e-8, "Invalid local extent");
    const auto amount = number(get(control, "amount"));
    double weight = 1;
    if (auto* weights = control.as_object().if_contains("weights")) {
        require(vertex < weights->as_array().size(), "Missing vertex weight");
        weight = number(weights->as_array()[vertex]);
        require(weight >= 0 && weight <= 1, "Vertex weight outside [0,1]");
    }
    const double px = p[0] - origin[0], py = p[1] - origin[1];
    const double u = px * direction[0] + py * direction[1];
    const double v = -px * direction[1] + py * direction[0];
    double du = 0, dv = 0;
    if (mechanism == "multi-anchor") {
        const auto& anchors = get(control, "anchors").as_array();
        require(anchors.size() >= 2, "Multi-anchor deformation needs two or more anchors");
        double sum = 0;
        std::array<double, 2> motion{};
        for (const auto& anchor : anchors) {
            auto position = point(get(anchor, "position"));
            const auto dx = point(get(anchor, "motion_x")), dy = point(get(anchor, "motion_y"));
            const auto distance = std::hypot(p[0] - position[0], p[1] - position[1]);
            const auto influence = 1 / std::max(1e-12, distance * distance);
            sum += influence;
            for (int i = 0; i < 2; ++i) motion[i] += influence * (dx[i] * x + dy[i] * y);
        }
        // Interpolating supports is a kinematic field, not a runtime rope solver.
        return {weight * motion[0] / sum, weight * motion[1] / sum};
    } else if (mechanism == "modes") {
        std::array<double,2> motion{};
        const auto& modes = get(control, "modes").as_array();
        require(!modes.empty(), "At least one displacement mode is required");
        for (const auto& mode : modes) {
            const auto& offsets = get(mode, "offsets").as_array();
            require(vertex < offsets.size(), "Missing modal vertex");
            const auto offset = point(offsets[vertex]);
            const auto gain = number(get(mode, "gain_x")) * x + number(get(mode, "gain_y")) * y;
            for (int i = 0; i < 2; ++i) motion[i] += gain * offset[i];
        }
        return {weight * motion[0], weight * motion[1]};
    } else if (mechanism == "oscillation") {
        du = amount * x * extent;
        dv = amount * y * extent;
    } else if (contains({"bend", "articulation", "branches", "pendulum", "ribbon"}, mechanism)) {
        const double angle = amount * x * (mechanism == "bend" ? std::clamp(u / extent, 0., 1.) : 1.);
        du = std::cos(angle) * u - std::sin(angle) * v - u;
        dv = std::sin(angle) * u + std::cos(angle) * v - v;
    } else if (contains({"width", "expansion", "soft-volume"}, mechanism)) {
        du = amount * x * u;
        dv = amount * (mechanism == "width" ? y : x) * v;
    } else if (mechanism == "opening") {
        // Signed local height closes onto the shared tangent; endpoints remain fixed.
        const double t = std::clamp(std::abs(u) / extent, 0., 1.);
        dv = -v * amount * x * (1 - t * t);
    } else if (mechanism == "residual") {
        du = amount * x * y * extent;
    } else {
        du = amount * x * extent;
        dv = amount * y * extent;
    }
    return {weight * (du * direction[0] - dv * direction[1]),
        weight * (du * direction[1] + dv * direction[0])};
}
}

Json ngDeformPattern(std::string_view pattern, const Json& document, const Json& selection,
        const Json& request, Checkpoint checkpoint) {
    if (request.is_object() && request.as_object().empty()) {
        const auto plan = ngPlanPattern(pattern, document, selection, checkpoint);
        if (get(plan, "status") == "ready") {
            auto effective = selection;
            effective.as_object()["matches"] = get(plan, "matches");
            effective.as_object()["unresolved"] = Array{};
            auto applied = ngDeformPattern(pattern, document, effective, get(plan, "recipe"), checkpoint);
            // Keep the public receipt bound to the detection result, while recording
            // the narrower executable selection and the exact generated recipe.
            auto& receipt = applied.as_object().at("receipt").as_object();
            receipt["execution_selection"] = effective;
            receipt["selection"] = digest(selection);
            receipt["automatic_plan"] = plan;
            return applied;
        }
        require(false, "Automatic plan requires input: " + string(get(plan, "reason")));
    }
    const auto& spec = ngPatternSpec(pattern);
    validateSelection(spec, document, selection);
    Index index(document);
    auto targets = selectedTargets(selection);
    require(!targets.empty(), array(selection, "candidates").empty() ?
        "No matched targets; deformation is not applicable" :
        "Structural candidates need review: specify their roles using mode=manual before deformation");
    require(optional(selection, "unresolved", Array{}).as_array().empty(),
        "Resolve ambiguous matches or use an explicit manual selection");
    Json result = document;
    Array changed, created, operations;
    const auto mechanism = std::string_view(spec.mechanism);
    auto& root = result.as_object().at("nodes");
    if (contains({"mask", "dodge", "mask-network"}, mechanism)) {
        const auto mode = string(optional(request, "mask_mode", mechanism == "dodge" ? "DodgeMask" : "Mask"));
        require(mode == "Mask" || mode == "DodgeMask", "Unknown mask mode");
        for (const auto& match : get(selection, "matches").as_array()) for (auto target : roleIds(match, "target")) {
            require(index.part(target), "Mask consumer must be a Part");
            auto* node = findNode(root, target);
            auto masks = optional(*node, "masks", Array{}).as_array();
            for (auto source : roleIds(match, "mask")) {
                bool exists = false;
                for (const auto& mask : masks)
                    exists |= id(get(mask, "source")) == source && string(get(mask, "mode")) == mode;
                if (!exists) masks.push_back(Object{{"source", source}, {"mode", mode}});
            }
            node->as_object()["masks"] = masks;
            changed.push_back(target);
        }
    } else if (mechanism == "clip" || (mechanism == "material" && !request.as_object().contains("controls")) || mechanism == "scope") {
        for (auto target : targets) {
            auto* node = findNode(root, target);
            if (mechanism == "scope") {
                const auto property = string(get(request, "property"));
                require(property == "dynamic_deformation" || property == "propagate_meshgroup",
                    "Expected a deformation scope property");
                require(get(request, "value").is_bool(), "Scope value must be boolean");
                const auto type = string(get(*node, "type"));
                require(property == "dynamic_deformation" ? type == "MeshGroup" :
                    (type == "Composite" || type == "DynamicComposite"), "Invalid scope target type");
                node->as_object()[property] = get(request, "value");
            } else {
                require(index.part(target), "Material target must be a Part");
                const auto blend = string(get(request, "blend_mode"));
                require(mechanism != "clip" || blend == "ClipToLower" || blend == "SliceFromLower", "Expected lower-alpha mode");
                require(contains({"Normal", "Multiply", "Screen", "Overlay", "ColorDodge", "LinearDodge",
                    "AddGlow", "ClipToLower", "SliceFromLower"}, blend), "Unsupported blend mode");
                node->as_object()["blend_mode"] = blend;
            }
            changed.push_back(target);
        }
    } else if (mechanism == "weld") {
        const auto& links = get(request, "links").as_array();
        require(!links.empty(), "Explicit boundary correspondence is required");
        for (const auto& link : links) {
            auto source = id(get(link, "source")), target = id(get(link, "target"));
            require(index.part(source) && index.part(target) && source != target, "Invalid welding endpoints");
            bool allowed = false;
            for (const auto& match : get(selection, "matches").as_array()) {
                auto sources = roleIds(match, "target"), ends = roleIds(match, "seam");
                allowed |= std::find(sources.begin(), sources.end(), source) != sources.end() &&
                    std::find(ends.begin(), ends.end(), target) != ends.end();
            }
            require(allowed, "Welding endpoints are outside the selected roles");
            auto n = vertices(index.node(source)).size(), m = vertices(index.node(target)).size();
            const auto& indices = get(link, "indices").as_array();
            require(indices.size() == n, "Welding needs one correspondence per source vertex");
            Array reverse;
            for (std::size_t i = 0; i < m; ++i) reverse.push_back(-1);
            std::size_t sourceVertex = 0, mapped = 0;
            for (const auto& value : indices) {
                auto v = number(value);
                require(v >= -1 && v < static_cast<double>(m) && std::floor(v) == v, "Invalid welding correspondence");
                if (v >= 0) {
                    require(number(reverse[static_cast<std::size_t>(v)]) == -1, "Welding must be one-to-one");
                    reverse[static_cast<std::size_t>(v)] = sourceVertex;
                    ++mapped;
                }
                ++sourceVertex;
            }
            require(mapped > 0, "Welding has no corresponding vertices");
            auto weight = number(get(link, "weight"));
            require(weight >= 0 && weight <= 1, "Welding weight outside [0,1]");
            auto* node = findNode(root, source);
            auto welds = optional(*node, "weldedLinks", Array{}).as_array();
            for (const auto& old : welds) require(id(get(old, "targetUUID")) != target, "Welding already exists; do not overwrite authored links");
            welds.push_back(Object{{"targetUUID", target}, {"indices", indices}, {"weight", weight}});
            node->as_object()["weldedLinks"] = welds;
            auto* other = findNode(root, target);
            auto reciprocals = array(*other, "weldedLinks");
            for (const auto& old : reciprocals) require(id(get(old, "targetUUID")) != source,
                "Reciprocal welding already exists");
            reciprocals.push_back(Object{{"targetUUID", source}, {"indices", reverse}, {"weight", 1 - weight}});
            other->as_object()["weldedLinks"] = reciprocals;
            changed.push_back(source);
            changed.push_back(target);
        }
    } else {
        // Intent (amplitude, axes and examples) is supplied separately from target selection.
        // Every control creates an actual serialized parameter, never a success-only plan.
        if (optional(result, "param").is_null()) result.as_object()["param"] = Array{};
        require(!(mechanism == "material" && request.as_object().contains("blend_mode")),
            "Use separate recipes for static blend mode and animated material controls");
        const auto& controls = get(request, "controls").as_array();
        require(!controls.empty(), "At least one deformation control is required");
        for (const auto& control : controls) {
            check(checkpoint);
            Index current(result);
            Id parameterId = control.as_object().contains("uuid") ? id(get(control, "uuid")) : unusedId(current, 1);
            require(!current.parameters.contains(parameterId) && !current.nodes.contains(parameterId), "Control UUID already exists");
            const auto xs = axis(control, "x"), ys = axis(control, "y");
            auto normalize = [](const Array& values) {
                Array result;
                const auto low = number(values.front()), span = number(values.back()) - low;
                for (const auto& value : values) result.push_back((number(value) - low) / span);
                return result;
            };
            Array bindings;
            auto label = string(optional(control, "label", spec.label));
            for (const auto& [_, old] : current.parameters)
                require(string(get(*old, "name")) != label, "Control label already exists; choose a distinct label");
            if (contains({"transfer", "surface"}, mechanism) && !control.as_object().contains("bindings")) {
                const auto sourceParameter = id(get(control, "source_parameter"));
                const auto sourceNode = id(get(control, "source_node"));
                require(current.parameters.contains(sourceParameter) && current.nodes.contains(sourceNode),
                    "Missing transfer source");
                const auto& source = *current.parameters.at(sourceParameter);
                const auto property = string(get(control, "property"));
                const auto sourceBindings = array(source, "bindings");
                const Json* sourceBinding = nullptr;
                for (const auto& binding : sourceBindings)
                    if (id(get(binding, "node")) == sourceNode && string(get(binding, "param_name")) == property) {
                        require(sourceBinding == nullptr, "Ambiguous source binding");
                        sourceBinding = &binding;
                    }
                require(sourceBinding != nullptr, "Source has no requested binding");
                validateBinding(current, source, *sourceBinding);
                const auto& sourceAxes = get(source, "axis_points").as_array();
                require(sourceAxes == Array{normalize(xs), normalize(ys)} &&
                    get(source, "min") == Json(Array{xs.front(), ys.front()}) &&
                    get(source, "max") == Json(Array{xs.back(), ys.back()}),
                    "Transfer requires source sampling axes; explicit resampling is not implicit");
                for (auto target : targets) {
                    if (mechanism == "surface") {
                        bool allowed = false;
                        for (const auto& match : get(selection, "matches").as_array()) {
                            const auto surfaces = roleIds(match, "surface");
                            allowed |= std::find(surfaces.begin(), surfaces.end(), sourceNode) != surfaces.end();
                        }
                        require(allowed && property == "deform", "Surface source must be a selected surface deformation");
                    }
                    Json transferred = *sourceBinding;
                    transferred.as_object()["node"] = target;
                    if (property == "deform") {
                        const auto& mapping = get(control, "correspondence").as_array();
                        require(mapping.size() == vertices(current.node(target)).size(), "Transfer correspondence count mismatch");
                        const auto basis = get(control, "basis").as_array();
                        require(basis.size() == 2, "Expected explicit source-to-target 2D basis");
                        const auto row0 = point(basis[0]), row1 = point(basis[1]);
                        for (auto& row : transferred.as_object().at("values").as_array()) for (auto& cell : row.as_array()) {
                            const auto sourceOffsets = cell.as_array();
                            Array offsets;
                            for (const auto& vertexMap : mapping) {
                                const auto& ids = get(vertexMap, "indices").as_array();
                                const auto& weights = get(vertexMap, "weights").as_array();
                                require(!ids.empty() && ids.size() == weights.size(), "Invalid correspondence weights");
                                std::array<double,2> offset{};
                                double sum = 0;
                                for (std::size_t i = 0; i < ids.size(); ++i) {
                                    auto vertex = number(ids[i]), w = number(weights[i]);
                                    require(vertex >= 0 && std::floor(vertex) == vertex && vertex < sourceOffsets.size() &&
                                        w >= 0 && w <= 1, "Invalid correspondence index/weight");
                                    const auto p = point(sourceOffsets[static_cast<std::size_t>(vertex)]);
                                    sum += w;
                                    for (int dim = 0; dim < 2; ++dim) offset[dim] += w * p[dim];
                                }
                                require(std::abs(sum - 1) < 1e-6, "Correspondence weights must sum to one");
                                offsets.push_back(Array{row0[0] * offset[0] + row0[1] * offset[1],
                                    row1[0] * offset[0] + row1[1] * offset[1]});
                            }
                            cell = offsets;
                        }
                    }
                    bindings.push_back(transferred);
                    changed.push_back(target);
                }
            } else if (control.as_object().contains("bindings")) {
                // Authored examples and residual fields use the same selected-target gate.
                bindings = get(control, "bindings").as_array();
                require(!bindings.empty(), "Empty authored control");
                for (const auto& b : bindings) {
                    auto target = id(get(b, "node"));
                    require(std::find(targets.begin(), targets.end(), target) != targets.end(), "Authored binding escaped selection");
                }
            } else if (mechanism == "link") {
                const auto source = id(get(control, "source_parameter")), destination = id(get(control, "destination_parameter"));
                require(current.parameters.contains(source) && current.parameters.contains(destination) && source != destination,
                    "Parameter link endpoints must be existing, distinct parameters");
                bool affectsSelection = false;
                for (const auto& binding : array(*current.parameters.at(destination), "bindings")) {
                    const auto node = id(get(binding, "node"));
                    require(std::find(targets.begin(), targets.end(), node) != targets.end(),
                        "Linked destination affects a node outside the target selection");
                    affectsSelection = true;
                }
                require(affectsSelection, "Destination parameter has no selected bindings");
                // Link the existing input rather than inventing an unconnected proxy parameter.
                auto* owner = findParameter(result.as_object().at("param").as_array(), source);
                const auto output = string(get(control, "output_axis"));
                require(output == "X" || output == "Y", "Invalid destination axis");
                const auto input = string(get(control, "input_axis"));
                require(input == "X" || input == "Y", "Invalid source axis");
                const auto gain = number(get(control, "gain"));
                const auto bias = number(optional(control, "bias", 0));
                const auto& oldAxes = get(*owner, "axis_points").as_array();
                const auto& low = get(*owner, "min").as_array();
                const auto& high = get(*owner, "max").as_array();
                Array ax, ay;
                for (int dim = 0; dim < 2; ++dim) for (const auto& value : oldAxes[dim].as_array())
                    (dim == 0 ? ax : ay).push_back(number(low[dim]) + number(value) * (number(high[dim]) - number(low[dim])));
                auto existing = optional(*owner, "bindings", Array{}).as_array();
                for (const auto& b : existing) require(!(id(get(b, "node")) == destination &&
                    get(b, "param_name").is_string() && string(get(b, "param_name")) == output), "Link already exists");
                existing.push_back(sampledBinding(destination, output, ax, ay, [&](double x,double y) {
                    return Json(bias + gain * (input == "X" ? x : y));
                }));
                owner->as_object()["bindings"] = existing;
                operations.push_back(Object{{"kind", "link"}, {"source", source}, {"destination", destination}});
                continue;
            } else {
                require(!contains({"transfer", "surface"}, mechanism),
                    "This mechanism requires registered, sampled binding fields; no guessed correspondence is applied");
                for (auto target : targets) {
                    check(checkpoint);
                    if (contains({"opacity", "state", "zsort", "gain", "physics-frame", "transform", "translation", "lighting", "follower", "material"}, mechanism)) {
                        std::string property;
                        if (mechanism == "opacity" || mechanism == "state") property = "opacity";
                        else if (mechanism == "zsort") property = "zSort";
                        else if (mechanism == "gain") property = string(optional(control, "property", "outputScale.x"));
                        else property = string(get(control, "property"));
                        require(contains({"opacity", "zSort", "outputScale.x", "outputScale.y", "transform.t.x",
                            "transform.t.y", "transform.r.z", "transform.s.x", "transform.s.y", "gravity", "length",
                            "frequency", "angleDamping", "lengthDamping", "tint.r", "tint.g", "tint.b",
                            "screenTint.r", "screenTint.g", "screenTint.b", "alphaThreshold", "emissionStrength"}, property), "Invalid scalar property");
                        if (property.starts_with("outputScale")) require(string(get(current.node(target), "type")) == "SimplePhysics", "Output gain requires physics node");
                        if (property == "opacity") require(index.part(target), "Opacity target must be a Part");
                        const double gainX = number(get(control, "gain_x"));
                        const double gainY = number(optional(control, "gain_y", 0));
                        const double neutral = number(get(control, "neutral"));
                        bindings.push_back(sampledBinding(target, property, xs, ys, [&](double x,double y) {
                            return Json(neutral + x * gainX + y * gainY);
                        }));
                    } else {
                        require(index.part(target), "Analytic deformation requires a Part");
                        const auto points = vertices(current.node(target));
                        if (contains({"region", "covering", "soft-volume"}, mechanism))
                            require(get(control, "weights").as_array().size() == points.size(), "Protected-region weights are required");
                        bindings.push_back(sampledBinding(target, "deform", xs, ys, [&](double x,double y) {
                            Array offsets;
                            for (std::size_t v = 0; v < points.size(); ++v) {
                                auto d = displacement(mechanism, points[v], x, y, control, v);
                                offsets.push_back(Array{d[0], d[1]});
                            }
                            return Json(offsets);
                        }));
                    }
                    changed.push_back(target);
                }
            }
            Json parameter = Object{{"uuid", parameterId}, {"name", label}, {"is_vec2", true},
                {"min", Array{xs.front(), ys.front()}}, {"max", Array{xs.back(), ys.back()}},
                {"defaults", Array{0,0}}, {"axis_points", Array{normalize(xs), normalize(ys)}}, {"bindings", bindings}};
            if (auto* key = control.as_object().if_contains("structural_key"))
                parameter.as_object()["structural_key"] = *key;
            for (const auto& binding : bindings) validateBinding(current, parameter, binding);
            if (pattern == "p18") if (const auto* driver = control.as_object().if_contains("driver_parameter")) {
                const auto source = id(*driver);
                auto* owner = findParameter(result.as_object().at("param").as_array(), source);
                require(owner != nullptr, "Missing aperture follower driver");
                auto existing = array(*owner, "bindings");
                for (auto binding : bindings) {
                    const auto target = id(get(binding, "node"));
                    require(std::ranges::none_of(existing, [&](const auto& item) {
                        return id(get(item, "node")) == target && get(item, "param_name") == "deform";
                    }), "Aperture follower already has a deformation binding");
                    validateBinding(current, *owner, binding);
                    existing.push_back(binding);
                    operations.push_back(Object{{"kind", "aperture_follower_binding"},
                        {"source", source}, {"target", target}});
                }
                owner->as_object()["bindings"] = existing;
                continue;
            }
            result.as_object().at("param").as_array().push_back(parameter);
            created.push_back(parameterId);
            if (const auto* physics = control.as_object().if_contains("blink_physics")) {
                require(pattern == "p19", "Blink physics requires a P19 control");
                const auto input = id(get(*physics,"driver"));
                require(current.parameters.contains(input), "Missing Blink physics input");
                auto* owner = findParameter(result.as_object().at("param").as_array(),input);
                const auto& keys = get(*physics,"anchor_x").as_array();
                const auto length = number(get(*physics,"length"));
                require(length > 0., "Invalid Blink pendulum length");
                const auto nodeId = unusedId(Index(result),1);
                auto& root = result.as_object().at("nodes").as_object();
                if (!root.contains("children")) root["children"] = Array{};
                root.at("children").as_array().push_back(Object{
                    {"uuid",nodeId},{"type","SimplePhysics"},{"name",label},{"enabled",true},
                    {"zsort",0.},{"lockToRoot",false},{"pinToMesh",false},
                    {"transform",Object{{"trans",Array{0.,0.,0.}},{"rot",Array{0.,0.,0.}},
                        {"scale",Array{1.,1.}}}},
                    {"param",parameterId},{"model_type","Pendulum"},{"map_mode","XY"},
                    {"gravity",1.},{"length",length},{"frequency",1.},
                    {"angle_damping",.5},{"length_damping",.5},
                    {"output_scale",Array{1.,1.}},{"local_only",false}});
                const Index wired(result);
                Array flags;
                for (const auto& row : keys) {
                    Array set;
                    for (std::size_t i=0; i<row.as_array().size(); ++i) set.push_back(true);
                    flags.push_back(std::move(set));
                }
                Json anchor = Object{{"node",nodeId},{"param_name","transform.t.x"},
                    {"values",keys},{"isSet",flags},{"interpolate_mode","Linear"}};
                validateBinding(wired,*owner,anchor);
                owner->as_object().at("bindings").as_array().push_back(std::move(anchor));
                operations.push_back(Object{{"kind","blink_physics"},{"source",input},
                    {"target",nodeId},{"destination",parameterId}});
                continue;
            }
            Id source = 0;
            if (auto* driver = control.as_object().if_contains("driver_parameter")) source = id(*driver);
            if (auto* surface = control.as_object().if_contains("driver_surface")) {
                require(!source && current.nodes.contains(id(*surface)), "Invalid shared surface driver");
                Index withControl(result);
                for (const auto& [uuid, p] : withControl.parameters)
                    if (optional(*p, "structural_driver_surface") == *surface) source = uuid;
                if (!source) {
                    source = unusedId(withControl, 1);
                    result.as_object().at("param").as_array().push_back(Object{
                        {"uuid", source}, {"name", "Surface lighting input/" + std::to_string(id(*surface))},
                        {"structural_driver_surface", *surface}, {"is_vec2", true},
                        {"min", Array{xs.front(), ys.front()}}, {"max", Array{xs.back(), ys.back()}},
                        {"defaults", Array{0,0}}, {"axis_points", Array{normalize(xs), normalize(ys)}},
                        {"bindings", Array{}}});
                    created.push_back(source);
                    operations.push_back(Object{{"kind", "created_surface_input"},
                        {"surface", *surface}, {"parameter", source}});
                }
            }
            if (source) {
                auto* owner = findParameter(result.as_object().at("param").as_array(), source);
                require(owner != nullptr && source != parameterId, "Missing automatic control driver");
                // The planner uses the driver's physical sample coordinates verbatim.
                // Do not normalize them a second time when linking the new control.
                Array driverX, driverY;
                const auto& driverAxes = get(*owner, "axis_points").as_array();
                for (int dim = 0; dim < 2; ++dim)
                    for (const auto& v : driverAxes[dim].as_array()) {
                        const auto lo = number(get(*owner, "min").as_array()[dim]);
                        const auto hi = number(get(*owner, "max").as_array()[dim]);
                        (dim == 0 ? driverX : driverY).push_back(lo + number(v) * (hi - lo));
                    }
                for (int dim = 0; dim < 2; ++dim) {
                    auto link = sampledBinding(parameterId, dim == 0 ? "X" : "Y", driverX, driverY,
                        [dim](double x, double y) { return Json(dim == 0 ? x : y); });
                    owner->as_object().at("bindings").as_array().push_back(link);
                }
                operations.push_back(Object{{"kind", "generated_control_driver"},
                    {"source", source}, {"destination", parameterId}});
            }
        }
    }
    finite(result);
    Json receipt = Object{{"schema", "autorig.pattern-receipt/1"}, {"pattern", spec.id},
        {"before", digest(document)}, {"after", digest(result)}, {"selection", digest(selection)},
        {"changed_nodes", changed}, {"created_parameters", created}, {"operations", operations},
        {"scope", "serialized_controls"}, {"time_response_verified", false}};
    auto report = ngVerifyPattern(pattern, document, result, selection, receipt, checkpoint);
    require(get(report, "passed").as_bool(), "Pattern mutation failed verification");
    return Object{{"document", result}, {"receipt", receipt}};
}

Json ngVerifyPattern(std::string_view pattern, const Json& before, const Json& after,
        const Json& selection, const Json& receipt, Checkpoint checkpoint) {
    if (auto* execution = receipt.as_object().if_contains("execution_selection")) {
        require(string(get(receipt, "selection")) == digest(selection), "Automatic selection mismatch");
        const auto plan = ngPlanPattern(pattern, before, selection, checkpoint);
        require(get(plan, "status") == "ready" && get(plan, "matches") == get(*execution, "matches") &&
            plan == get(receipt, "automatic_plan"), "Automatic plan is not reproducible");
        auto inner = receipt;
        inner.as_object().erase("execution_selection");
        inner.as_object()["selection"] = digest(*execution);
        auto report = ngVerifyPattern(pattern, before, after, *execution, inner, checkpoint);
        report.as_object()["automatic_plan_verified"] = true;
        report.as_object()["policy"] = get(plan, "policy");
        return report;
    }
    const auto& spec = ngPatternSpec(pattern);
    validateSelection(spec, before, selection);
    require(string(get(receipt, "schema")) == "autorig.pattern-receipt/1" &&
        string(get(receipt, "pattern")) == pattern, "Receipt belongs to another pattern");
    require(string(get(receipt, "before")) == digest(before) && string(get(receipt, "after")) == digest(after) &&
        string(get(receipt, "selection")) == digest(selection), "Receipt/source mismatch");
    finite(after);
    Index previous(before), current(after);
    auto targets = selectedTargets(selection);
    std::set<Id> physicsNodes;
    for (const auto& op : array(receipt,"operations")) if (optional(op,"kind") == "blink_physics") {
        const auto node = id(get(op,"target")), input = id(get(op,"source")), output = id(get(op,"destination"));
        require(pattern == "p19" && current.nodes.contains(node) && !previous.nodes.contains(node) &&
            physicsNodes.insert(node).second && previous.parameters.contains(input) &&
            current.parameters.contains(output) && !previous.parameters.contains(output),
            "Unexpected Blink physics endpoints");
        require(current.parents.at(node) == id(get(get(after,"nodes"),"uuid")) &&
            get(current.node(node),"type") == "SimplePhysics" && id(get(current.node(node),"param")) == output,
            "Invalid Blink physics node");
        const auto& outputBindings = array(*current.parameters.at(output),"bindings");
        require(!outputBindings.empty() && std::ranges::all_of(outputBindings,[&](const auto& binding) {
            return std::ranges::find(targets,id(get(binding,"node"))) != targets.end();
        }), "Blink physics output escaped the selected Parts");
        const auto& inputBindings = array(*current.parameters.at(input),"bindings");
        require(std::ranges::any_of(inputBindings,[&](const auto& binding) {
            return id(get(binding,"node")) == node && get(binding,"param_name") == "transform.t.x";
        }), "Blink physics anchor is disconnected");
    }
    require(previous.nodes.size() + physicsNodes.size() == current.nodes.size(), "Unexpected Node creation/deletion");
    if (pattern == "p16") for (const auto& match : get(selection, "matches").as_array())
        for (auto uuid : roleIds(match, "seam")) targets.push_back(uuid);
    for (const auto& [uuid, node] : previous.nodes) {
        check(checkpoint);
        require(current.nodes.contains(uuid), "Original Node disappeared");
        auto a = *node, b = current.node(uuid);
        // Child edits are checked independently; hierarchy and original meshes must remain intact.
        a.as_object().erase("children"); b.as_object().erase("children");
        require(previous.parents.at(uuid) == current.parents.at(uuid), "Unexpected reparenting");
        if (std::find(targets.begin(), targets.end(), uuid) == targets.end())
            require(a == b, "Mutation escaped the selected Nodes");
        require(optional(*node, "mesh") == optional(current.node(uuid), "mesh"), "Original mesh/UV changed");
    }
    for (const auto& [uuid, parameter] : previous.parameters) {
        require(current.parameters.contains(uuid), "Original parameter disappeared");
        if (*parameter == *current.parameters.at(uuid)) continue;
        auto a = *parameter, b = *current.parameters.at(uuid);
        const auto oldBindings = array(a, "bindings"), newBindings = array(b, "bindings");
        a.as_object().erase("bindings"); b.as_object().erase("bindings");
        require(a == b && newBindings.size() > oldBindings.size(), "Link changed original parameter settings");
        for (std::size_t i = 0; i < oldBindings.size(); ++i)
            require(oldBindings[i] == newBindings[i], "Link changed original bindings");
        for (std::size_t i = oldBindings.size(); i < newBindings.size(); ++i) {
            if (pattern == "p19" && physicsNodes.contains(id(get(newBindings[i],"node")))) {
                const auto target = id(get(newBindings[i],"node"));
                const auto& property = get(newBindings[i],"param_name");
                require(property == "transform.t.x" &&
                    std::ranges::any_of(array(receipt,"operations"),[&](const auto& op) {
                        return optional(op,"kind") == "blink_physics" &&
                            id(get(op,"source")) == uuid && id(get(op,"target")) == target;
                    }), "Unexpected Blink physics input binding");
                validateBinding(current,*current.parameters.at(uuid),newBindings[i]);
                continue;
            }
            if (pattern == "p18" && get(newBindings[i], "param_name") == "deform") {
                const auto target = id(get(newBindings[i], "node"));
                require(std::find(targets.begin(), targets.end(), target) != targets.end(),
                    "Aperture follower escaped the selected Parts");
                require(std::ranges::any_of(array(receipt, "operations"), [&](const auto& op) {
                    return optional(op, "kind") == "aperture_follower_binding" &&
                        id(get(op, "source")) == uuid && id(get(op, "target")) == target;
                }), "Aperture follower binding is missing from the receipt");
                validateBinding(current, *current.parameters.at(uuid), newBindings[i]);
                continue;
            }
            if (pattern != "p02") {
                bool generatedDriver = false;
                for (const auto& op : array(receipt, "operations"))
                    generatedDriver |= optional(op, "kind") == "generated_control_driver" &&
                        id(get(op, "source")) == uuid &&
                        get(op, "destination") == get(newBindings[i], "node") &&
                        !previous.parameters.contains(id(get(op, "destination")));
                require(generatedDriver, "Existing parameter was modified outside generated driver links");
            }
            require(contains({"X", "Y"}, string(get(newBindings[i], "param_name"))), "Unexpected link binding");
            validateBinding(current, *current.parameters.at(uuid), newBindings[i]);
        }
    }
    for (const auto& [uuid, parameter] : current.parameters) {
        check(checkpoint);
        std::set<std::pair<Id,std::string>> bindings;
        if (previous.parameters.contains(uuid) && *previous.parameters.at(uuid) == *parameter) continue;
        for (const auto& binding : array(*parameter, "bindings")) {
            // Validate generated controls in full; do not relabel legacy malformed inputs as newly generated.
            if (!previous.parameters.contains(uuid)) validateBinding(current, *parameter, binding);
            if (get(binding, "param_name").is_string()) {
                auto property = string(get(binding, "param_name"));
                require(bindings.emplace(id(get(binding, "node")), property).second, "Duplicate binding");
            }
        }
    }
    // Detect cycles in actual parameter-axis links, not in hierarchy group IDs.
    std::map<Id,std::vector<Id>> edges;
    for (const auto& [uuid,p] : current.parameters)
        for (const auto& b : array(*p, "bindings"))
            if (get(b, "param_name").is_string() && contains({"X","Y"}, string(get(b, "param_name")))) {
                auto target = id(get(b, "node"));
                require(current.parameters.contains(target), "Parameter link target missing");
                edges[uuid].push_back(target);
            }
    std::map<Id,int> colors;
    std::function<void(Id)> visit = [&](Id v) {
        require(colors[v] != 1, "Parameter dependency cycle");
        if (colors[v] == 2) return;
        colors[v] = 1;
        for (auto target : edges[v]) visit(target);
        colors[v] = 2;
    };
    for (const auto& [v,_] : edges) visit(v);
    return Object{{"passed", true}, {"pattern", spec.id}, {"checked", "references, dimensions, selection, preserved geometry, parameter dependencies"},
        {"visual_review_required", true}, {"time_response_verified", false}};
}

Json ngParsePatternJson(std::string_view text) {
    boost::json::parse_options options;
    options.numbers = boost::json::number_precision::precise;
    return boost::json::parse(text, {}, options);
}

InxDocument ngReadPatternInx(std::span<const std::uint8_t> bytes) {
    require(bytes.size() >= 12, "Truncated INX header");
    const std::array<std::uint8_t, 8> signature{'T','R','N','S','R','T','S',0};
    require(std::equal(signature.begin(), signature.end(), bytes.begin()), "Expected an INX document");
    std::uint32_t length = 0;
    for (int i = 8; i < 12; ++i) length = (length << 8) | bytes[i];
    require(length <= bytes.size() - 12, "Truncated INX JSON");
    InxDocument result;
    result.prefix.assign(bytes.begin(), bytes.begin() + 8);
    result.document = ngParsePatternJson(std::string_view(reinterpret_cast<const char*>(bytes.data() + 12), length));
    result.payload.assign(bytes.begin() + 12 + length, bytes.end());
    (void)Index(result.document);
    return result;
}
std::vector<std::uint8_t> ngWritePatternInx(const InxDocument& source, const Json& document) {
    finite(document);
    (void)Index(document);
    auto bytes = boost::json::serialize(document);
    require(bytes.size() <= std::numeric_limits<std::uint32_t>::max(), "INX JSON is too large");
    auto result = source.prefix;
    require(result.size() == 8, "Invalid INX prefix");
    for (int shift : {24, 16, 8, 0}) result.push_back(static_cast<std::uint8_t>(bytes.size() >> shift));
    result.insert(result.end(), bytes.begin(), bytes.end());
    result.insert(result.end(), source.payload.begin(), source.payload.end());
    return result;
}
}
