#include "detail/evidence.hpp"
#include "observation.hpp"
#include <map>
#include <set>

namespace nijigenerate::autorig::deterministic::evidence {
using namespace detail;

Json ngRigDetectSurfaceExclusions(Json state) {
    auto materials = atEvidence(state, "materials").sourceArrayValue();
    std::map<std::uint64_t, Json> byId;
    for (auto material : materials) byId[ngRigUnsigned(atEvidence(material, "uuid"))] = material;
    JsonArray changes;
    for (auto material : materials) {
        if (atEvidence(material, "static").as_bool()
                || getEvidence(material, "semantic_source", literalEvidence("")).as_string().view()
                    != "alpha_proximal_support_candidate") continue;
        const auto supportId = ngRigUnsigned(atEvidence(material, "support_part"));
        const auto found = byId.find(supportId);
        if (found == byId.end()) continue;
        const auto cloud = ngRigPoints(atEvidence(material, "cloud"));
        const auto assignedCoverage = observation::ngRigAlphaCoverage(found->second, cloud,
            compat::RetainedString::fromStaticLiteral("alpha_runs_32"), nullptr);
        if (assignedCoverage >= .5) continue;
        Json receiver;
        double bestCoverage = .95;
        bool ambiguous = false;
        for (auto surface : materials) {
            if (atEvidence(surface, "static").as_bool() || atEvidence(surface, "role").as_string().view() != "face") continue;
            const auto coverage = observation::ngRigAlphaCoverage(surface, cloud,
                compat::RetainedString::fromStaticLiteral("alpha_runs_32"), nullptr);
            if (coverage < .95) continue;
            if (!receiver.is_null()) ambiguous = true;
            if (coverage >= bestCoverage) { receiver = surface; bestCoverage = coverage; }
        }
        if (receiver.is_null() || ambiguous) continue;
        const auto previousRole = atEvidence(material, "role");
        const auto receiverId = atEvidence(receiver, "uuid");
        // A supported surface overlay follows the face instead of an unrelated
        // hair/ear mechanism. Do not invent an eyelid feature or change masks.
        setEvidence(material, "role", literalEvidence("face_feature"));
        setEvidence(material, "owner", literalEvidence("head"));
        setEvidence(material, "chart", literalEvidence("face"));
        setEvidence(material, "support_part", receiverId);
        setEvidence(material, "semantic_source", literalEvidence("painted_surface_coverage_over_proximal_hint"));
        changes.append(objectEvidence({{stringLiteralEvidence("part"), atEvidence(material, "uuid")},
            {stringLiteralEvidence("path"), atEvidence(material, "path")},
            {stringLiteralEvidence("scope"), literalEvidence("unsupported_deformation_family")},
            {stringLiteralEvidence("excluded_role"), previousRole},
            {stringLiteralEvidence("replacement_role"), literalEvidence("face_feature")},
            {stringLiteralEvidence("support_part"), receiverId},
            {stringLiteralEvidence("previous_support_coverage"), assignedCoverage},
            {stringLiteralEvidence("surface_coverage"), bestCoverage},
            {stringLiteralEvidence("visual_review_required"), true}}));
    }
    return changes;
}

Json ngRigDetectExclusions(Json observation, Json options, std::shared_ptr<framework::AutoRigTaskContext> context) {
    // Resolve explicit Part/Node selections before anatomical inference. A node
    // selection applies to its descendants; a more specific selection wins.
    auto effective = duplicateEvidence(options);
    auto overrides = duplicateEvidence(getEvidence(options, "materials", SettingsJsonObject{}));
    auto selection = getEvidence(options, "rig_exclusions", SettingsJsonObject{});
    std::map<std::uint64_t, std::uint64_t> parents;
    std::set<std::uint64_t> known, materialIds, excluded, included;
    known.insert(ngRigUnsigned(atEvidence(observation, "rootId")));
    for (auto node : getEvidence(observation, "groups", JsonArray{}).sourceArrayValue()) {
        const auto id = ngRigUnsigned(atEvidence(node, "uuid"));
        known.insert(id);
        auto parent = getEvidence(node, "parent", Json{});
        if (!parent.is_null()) parents[id] = ngRigUnsigned(parent);
    }
    for (auto node : getEvidence(observation, "hierarchy_nodes", JsonArray{}).sourceArrayValue()) {
        auto id = ngRigUnsigned(atEvidence(node, "uuid"));
        known.insert(id);
        auto parent = getEvidence(node, "parent", Json{});
        if (!parent.is_null()) parents[id] = ngRigUnsigned(parent);
    }
    for (auto material : atEvidence(observation, "materials").sourceArrayValue()) {
        auto id = ngRigUnsigned(atEvidence(material, "uuid"));
        materialIds.insert(id);
        known.insert(id);
        auto parent = getEvidence(material, "parent", Json{});
        if (!parent.is_null()) parents[id] = ngRigUnsigned(parent);
    }
    for (auto id : getEvidence(selection, "exclude", JsonArray{}).sourceArrayValue()) excluded.insert(ngRigUnsigned(id));
    for (auto id : getEvidence(selection, "include", JsonArray{}).sourceArrayValue()) included.insert(ngRigUnsigned(id));
    for (auto id : excluded) {
        enforceEvidence(known.contains(id), "Excluded Part/Node does not exist in this observation", 34);
        enforceEvidence(!included.contains(id), "Part/Node cannot be both included and excluded", 35);
    }
    for (auto id : included) enforceEvidence(known.contains(id), "Included Part/Node does not exist in this observation", 37);
    std::map<std::uint64_t, int> decisions;
    std::map<std::uint64_t, std::string> reasons;
    for (auto material : atEvidence(observation, "materials").sourceArrayValue()) {
        const auto id = ngRigUnsigned(atEvidence(material, "uuid"));
        std::set<std::uint64_t> visited;
        int decision = 0;
        for (auto ancestor = id; ancestor && visited.insert(ancestor).second;) {
            if (excluded.contains(ancestor)) { decision = -1; break; }
            if (included.contains(ancestor)) { decision = 1; break; }
            auto parent = parents.find(ancestor);
            ancestor = parent == parents.end() ? 0 : parent->second;
        }
        decisions[id] = decision;
        const bool empty = atEvidence(material, "cloud").sourceArrayValue().empty();
        const auto path = atEvidence(material, "path").as_string();
        const auto existing = evidenceObject(overrides).if_contains(path.view());
        auto rule = duplicateEvidence(existing ? *existing : Json(SettingsJsonObject{}));
        if (decision < 0 || empty) {
            setEvidence(rule, "static", true);
            reasons[id] = decision < 0 ? "manual_part_or_node_exclusion" : "empty_alpha_support";
        } else if (decision > 0) {
            // Inclusion changes rig eligibility, never artwork visibility.
            setEvidence(rule, "static", false);
        }
        overrides.sourceIndexAssignSlot(path) = rule;
    }
    setEvidence(effective, "materials", overrides);
    auto result = ngRigClassifyMaterials(observation, effective, context);
    const auto scopedExclusions = ngRigDetectSurfaceExclusions(result);
    JsonArray records, parts, nodes, review;
    for (auto id : excluded) if (!materialIds.contains(id)) nodes.append(Json(id));
    for (auto material : result.sourceIndexValue("materials").sourceArrayValue()) {
        const auto id = ngRigUnsigned(atEvidence(material, "uuid"));
        if (decisions[id] > 0 && atEvidence(material, "active").as_bool()
                && !atEvidence(material, "cloud").sourceArrayValue().empty()) {
            setEvidence(material, "static", false);
            // A background candidate explicitly included by the user still
            // needs an anatomical role; do not silently guess a body region.
            if (atEvidence(material, "role").as_string().view() == "background") {
                setEvidence(material, "role", literalEvidence("local"));
                setEvidence(material, "owner", literalEvidence("local"));
                setEvidence(material, "chart", literalEvidence("local"));
            }
            if (!evidenceObject(material).if_contains("owner")) {
                for (auto rule : atEvidence(templates::ngRigMaterialRoles(), "rules").sourceArrayValue())
                    if (atEvidence(rule, "id").as_string() == atEvidence(material, "role").as_string()) {
                        setEvidence(material, "owner", atEvidence(rule, "owner"));
                        setEvidence(material, "chart", atEvidence(rule, "chart"));
                        break;
                    }
            }
        }
        const bool stationary = atEvidence(material, "static").as_bool();
        auto semantic = getEvidence(material, "semantic_source", literalEvidence(""));
        if (stationary) {
            auto reason = reasons[id];
            if (reason.empty()) reason = !atEvidence(material, "active").as_bool() ? "inactive_source"
                : atEvidence(material, "role").as_string().view() == "background" ? "background_candidate"
                : "classified_static_or_clipping_dependency";
            parts.append(Json(id));
            records.append(objectEvidence({{stringLiteralEvidence("part"), Json(id)},
                {stringLiteralEvidence("path"), atEvidence(material, "path")},
                {stringLiteralEvidence("reason"), Json(core::settings::SettingsString(compat::RetainedString(reason)))},
                {stringLiteralEvidence("evidence"), semantic},
                {stringLiteralEvidence("scope"), literalEvidence("basic_rig")}}));
        } else if (semantic.as_string().view() == "alpha_proximal_support_candidate") {
            // Proximity alone is insufficient evidence to discard an ornament,
            // mask source, shadow, alternate expression, or detached limb.
            review.append(objectEvidence({{stringLiteralEvidence("part"), Json(id)},
                {stringLiteralEvidence("reason"), literalEvidence("proximity_only_assignment")},
                {stringLiteralEvidence("support"), getEvidence(material, "support_part", Json{})}}));
        }
    }
    setEvidence(result, "options", options);
    setEvidence(result, "rig_exclusions", objectEvidence({
        {stringLiteralEvidence("parts"), parts}, {stringLiteralEvidence("nodes"), nodes},
        {stringLiteralEvidence("records"), records}, {stringLiteralEvidence("requires_review"), review},
        {stringLiteralEvidence("scoped_parts"), scopedExclusions},
        {stringLiteralEvidence("validated"), true}}));
    return result;
}

Json ngRigVerifyExclusions(Json state, Json program) {
    const auto detection = atEvidence(state, "rig_exclusions");
    std::set<std::uint64_t> excluded;
    for (auto id : atEvidence(detection, "parts").sourceArrayValue()) excluded.insert(ngRigUnsigned(id));
    const bool humanoid = getEvidence(program, "kind", literalEvidence("humanoid")).as_string().view() == "humanoid";
    const auto domains = humanoid ? atEvidence(program, "domains").sourceArrayValue() : JsonArray{};
    const auto localCarriers = humanoid ? JsonArray{} : atEvidence(program, "carriers").sourceArrayValue();
    JsonArray violations;
    for (auto domain : domains)
        for (auto part : atEvidence(domain, "parts").sourceArrayValue())
            if (excluded.contains(ngRigUnsigned(part))) violations.append(objectEvidence({
                {stringLiteralEvidence("part"), part}, {stringLiteralEvidence("domain"), atEvidence(domain, "id")}}));
    for (auto carrier : localCarriers) {
        const auto part = atEvidence(carrier, "part");
        if (excluded.contains(ngRigUnsigned(part))) violations.append(objectEvidence({
            {stringLiteralEvidence("part"), part},
            {stringLiteralEvidence("reason"), literalEvidence("excluded_part_has_local_rig_carrier")}}));
    }
    for (auto material : atEvidence(state, "materials").sourceArrayValue())
        if (excluded.contains(ngRigUnsigned(atEvidence(material, "uuid"))) && !atEvidence(material, "static").as_bool())
            violations.append(objectEvidence({{stringLiteralEvidence("part"), atEvidence(material, "uuid")},
                {stringLiteralEvidence("reason"), literalEvidence("excluded_material_is_not_static")}}));
    for (auto target : getEvidence(state, "targets", JsonArray{}).sourceArrayValue())
        if (excluded.contains(ngRigUnsigned(atEvidence(target, "part"))))
            violations.append(objectEvidence({{stringLiteralEvidence("part"), atEvidence(target, "part")},
                {stringLiteralEvidence("reason"), literalEvidence("excluded_part_has_native_rig_target")}}));
    for (auto change : getEvidence(detection, "scoped_parts", JsonArray{}).sourceArrayValue()) {
        const auto part = ngRigUnsigned(atEvidence(change, "part"));
        bool found = false;
        const auto verifyFamily = [&](Json target) {
            found = true;
            if (atEvidence(target, "owner").as_string().view() != "head"
                    || atEvidence(target, "chart").as_string().view() != "face")
                violations.append(objectEvidence({{stringLiteralEvidence("part"), Json(part)},
                    {stringLiteralEvidence("reason"), literalEvidence("surface_overlay_has_unsupported_deformation_family")}}));
        };
        for (auto domain : domains)
            for (auto member : atEvidence(domain, "parts").sourceArrayValue())
                if (ngRigUnsigned(member) == part) verifyFamily(domain);
        for (auto carrier : localCarriers)
            if (ngRigUnsigned(atEvidence(carrier, "part")) == part) verifyFamily(carrier);
        if (!found) violations.append(objectEvidence({{stringLiteralEvidence("part"), Json(part)},
            {stringLiteralEvidence("reason"), literalEvidence("surface_overlay_missing_from_rig")}}));
    }
    return objectEvidence({{stringLiteralEvidence("passed"), violations.empty()},
        {stringLiteralEvidence("violations"), violations},
        {stringLiteralEvidence("excluded_parts"), atEvidence(detection, "parts")},
        {stringLiteralEvidence("visual_review_required"), true}});
}
}
