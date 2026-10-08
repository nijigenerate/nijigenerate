"""Compare permitted, independent Python math with owned D run artifacts.

Only the named independent functions below are executed. No package imports,
reference assets, reference registration code or historical records are loaded.
This does not certify complete pipeline equivalence.
"""
import argparse
import ast
from collections import defaultdict
import hashlib
import json
import re
from pathlib import Path
import numpy as np


def functions(path, names, namespace):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    selected = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    if {node.name for node in selected} != set(names):
        raise ValueError("Independent comparison function is missing")
    namespace.update(np=np, Path=Path, __file__=str(path), defaultdict=defaultdict,
                     digest=lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest(),
                     json_digest=lambda value: hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest())
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(path), "exec"), namespace)
    return namespace


def same_numeric_tree(left, right, tolerance=1e-8):
    if isinstance(left, dict) and isinstance(right, dict):
        return left.keys() == right.keys() and all(same_numeric_tree(left[key],right[key],tolerance) for key in left)
    if isinstance(left, list) and isinstance(right, list):
        return len(left) == len(right) and all(same_numeric_tree(a,b,tolerance) for a,b in zip(left,right))
    if isinstance(left, (int,float)) and isinstance(right, (int,float)):
        return abs(left-right) <= tolerance
    return left == right


def compare(scripts, run, compiled=None):
    anatomy = functions(scripts / "riglib/anatomy.py",
                        ["torso_basis", "solve_scaffold", "depth_field"], {})
    sections = functions(scripts / "riglib/psd_evidence.py", ["section"],
                         {"POLICY": {"section_band": .025}})
    hierarchy = functions(scripts / "riglib/hierarchy.py", ["material_groups", "compile_hierarchy"], {})
    lexical = functions(scripts / "riglib/assembly.py", ["normalized_name"], {"re":re})
    semantics_code = functions(scripts / "riglib/semantic_observation.py", ["name_tokens", "candidate"],
                               {"re":re, "normalized_name":lexical["normalized_name"]})
    read = lambda path: json.loads(path.read_text(encoding="utf-8"))
    def artifact(stage, name):
        attempts = sorted((run/stage).glob("attempt-*"),
                          key=lambda path: int(path.name.split("-")[-1]), reverse=True)
        return next((path/name for path in attempts if (path/name).is_file()), None)
    state_path = artifact("6_layout_compile-domain-layout", "state.json") or \
        artifact("7_compile_compile-rig", "state.json")
    program_path = artifact("6_layout_compile-domain-layout", "program.json") or \
        artifact("7_compile_compile-rig", "program.json")
    if state_path is None or program_path is None:
        raise ValueError("The owned run has no completed compile artifact")
    state = read(state_path)
    program = read(program_path)
    if compiled is not None:
        updated = read(compiled)
        program = updated["program"]
        state["evidence"] = updated["evidence"]
    prior = read(scripts.parent / "structures/humanoid-prior.json")
    fitted = anatomy["solve_scaffold"](state["evidence"], prior)
    errors = {name: float(np.linalg.norm(np.asarray(point) - program["scaffold"]["landmarks"][name]))
              for name, point in fitted["landmarks"].items()}
    materials = {row["uuid"]: row for row in state["materials"]}
    roles = [{"part":c["part"], "owner":c["owner"], "chart":c["owner"]+"/"+c["chart"]}
             for c in program["carriers"]]
    nodes = [{"uuid":row["uuid"], "parent":row.get("parent")} for row in state["materials"]+state["groups"]]
    nodes.append({"uuid":state["rootId"], "parent":None})
    semantics = [dict(row, part=row["uuid"]) for row in state["materials"] if not row["static"]]
    groups, resolved, _ = hierarchy["material_groups"]({"nodes":nodes}, {"materials":roles},
                            {"semantic_materials":semantics})
    depth_differences, bone_differences = [], []
    for carrier in program["carriers"]:
        owner, role, chart = carrier["owner"], carrier["role"], carrier["chart"]
        chart = resolved[carrier["part"]]["chart"].split("/", 1)[1]
        domain = dict(carrier["domain"])
        domain["kind"] = "surface"
        expected_bones = ["Head"] if owner == "head" else ["Pelvis", "Spine", "Chest", "Neck"]
        offset = 0.
        height = fitted["body_height"]
        if owner == "head":
            if chart == "hair:back": domain["kind"] = "back_hair"; offset = height*.003
            elif chart.startswith(("hair", "headwear", "ear")): offset = height*.004
        elif owner == "torso":
            if chart in ("skirt", "skirt_back", "apron"):
                domain["kind"] = "skirt_back" if chart == "skirt_back" else "skirt_front"
                expected_bones = ["Pelvis"]
            elif chart == "tail": domain["kind"] = "appendage"; expected_bones = ["Pelvis"]
            if chart == "apron": offset = height*.003
            elif chart == "tail": offset = -height*.025
            if chart == "neck": domain["kind"] = "neck"; expected_bones = ["Neck"]
            elif chart == "attachment:chest": expected_bones = ["Chest"]
            elif chart == "attachment:pelvis": expected_bones = ["Pelvis"]
        else:
            family = owner.split(":")[0]
            names = ("UpperArm", "Forearm", "Hand") if family == "arm" else ("Thigh", "Shin", "Foot")
            tags = ("R","L") if carrier["side"] == "Both" else (carrier["side"],)
            expected_bones = [name+"."+tag for tag in tags for name in names]
            if chart in ("sleeve", "shoulder"): offset = domain["radius"]*.2
            if chart == "attachment:ankle": expected_bones = ["Foot."+carrier["side"]]
            elif chart == "attachment:thigh": expected_bones = ["Thigh."+carrier["side"]]
            elif chart == "hand": expected_bones = ["Hand."+carrier["side"]]
            elif chart == "foot": expected_bones = ["Foot."+carrier["side"]]
        domain["offset"] = offset
        expected = np.zeros(len(carrier["points"])) if carrier["side"] == "Both" else \
            anatomy["depth_field"](carrier["points"], domain, fitted, prior) / program["native_depth_scale"]
        expected = np.round(expected,6)
        actual = np.asarray(carrier["depth"])
        error = float(np.max(np.abs(expected-actual)))
        if error > 1e-6:
            depth_differences.append({"part":carrier["part"], "path":carrier["path"], "role":role,
                                      "python_kind":domain["kind"], "d_kind":carrier["domain"]["kind"],
                                      "expected_offset_model":offset, "maximum_depth_difference":error})
        if expected_bones != carrier["bones"]:
            bone_differences.append({"path":carrier["path"], "python":expected_bones, "d":carrier["bones"]})
    def cloud(roles):
        return np.concatenate([row["cloud"] for row in materials.values()
                               if not row["static"] and row["role"] in roles])
    torso = cloud({"torso", "bodice", "waistwear"})
    landmarks = state["evidence"]["landmarks"]
    origin = np.asarray(landmarks["neck_base"]["xy"])
    tip = np.asarray(landmarks["pelvis"]["xy"])
    axis = tip-origin
    fractions = {name: float((sections["section"](torso, fraction)-origin)@axis/(axis@axis))
                 for name, fraction in [("chest", .4), ("waist", .9)]}
    gap = prior["torso_axis"]["minimum_station_gap"]
    valid = any(row["role"] == "torso" and not row["static"] for row in materials.values()) and \
        min(np.diff([0, *fractions.values(), 1])) > gap
    stations = {name: float(np.linalg.norm(origin+(fractions[name] if valid else fallback)*axis-
                            landmarks[name]["xy"])) for name, fallback in [("chest", .35), ("waist", .75)]}
    candidate_differences = []
    for material in materials.values():
        if material["static"]: continue
        role, feature = semantics_code["candidate"](material["name"], material["ancestors"])
        if (role is not None and role != material["role"]) or (feature or "") != material.get("feature", ""):
            candidate_differences.append({"path":material["path"], "python_candidate":[role,feature],
                                          "d":[material["role"],material.get("feature", "")]})
    hierarchy_differences = []
    hierarchy_unit_details = []
    if "hierarchy" in program:
        public_nodes = []
        census = state.get("hierarchy_nodes")
        if census is None:
            census = [dict(row,type="DynamicComposite") for row in state["groups"]]
            census += [dict(uuid=row["uuid"],parent=row.get("parent",state["rootId"]),type="Part",
                           source_order=index) for index,row in enumerate(state["materials"])]
            census += [dict(uuid=state["rootId"],parent=None,type="Node")]
        for row in census:
            material = materials.get(row["uuid"])
            public_nodes.append(dict(row,draw_properties=dict(blend_mode=row.get("blend_mode","Normal"),
                                     opacity=row.get("opacity",1),zsort=0),
                                     bounds={"nominal_world_xy":material["bounds"] if material else [0,0,1,1]},
                                     nominal_world_matrix=np.eye(4).tolist()))
        expected_domains = []
        for domain in program["domains"]:
            carrier = next(c for c in program["carriers"] if c["domain_id"] == domain["id"])
            expected_domains.append(dict(id=domain["id"], semantic_chart=domain["owner"]+"/"+domain["chart"],
                                         owner=domain["owner"], bone_sources=carrier["bones"],
                                         parts=[{"uuid":uid} for uid in domain["parts"]]))
        expected_hierarchy = hierarchy["compile_hierarchy"](expected_domains,
            dict(semantic_materials=semantics,side_mapping={"r":"R","l":"L","both":"Both"}),
            dict(nodes=public_nodes),fitted)
        actual_hierarchy = program["hierarchy"]
        for key in ("render_scope","groups","surface_parents","body_origin","face_origin","clipping_receivers"):
            if not same_numeric_tree(expected_hierarchy[key],actual_hierarchy[key]): hierarchy_differences.append(key)
        for domain in expected_domains:
            if domain["render_units"] != actual_hierarchy["render_units"][domain["id"]]:
                hierarchy_differences.append("render_units:"+domain["id"])
                hierarchy_unit_details.append(dict(domain=domain["id"],python=domain["render_units"],
                                                   d=actual_hierarchy["render_units"][domain["id"]]))
    return {"scope":"Independent pre-registration anatomy/grouping/depth on D observations and assigned roles; not complete Python parity",
            "run":run.name, "active_parts":len(program["carriers"]),
            "compiled_override":str(compiled) if compiled is not None else None,
            "depth_numeric_tolerance":1e-6,
            "same_evidence_scaffold_maximum_error":max(errors.values()),
            "python_semantic_group_count":len(groups), "d_domain_count":len(program["domains"]),
            "canonical_chart_changes":sum(resolved[r["part"]]["chart"] != r["chart"] for r in roles),
            "torso_station_errors":stations, "depth_differences":depth_differences,
            "bone_differences":bone_differences, "semantic_candidate_differences":candidate_differences,
            "hierarchy_topology_differences":hierarchy_differences,
            "hierarchy_unit_difference_details":hierarchy_unit_details,
            "hierarchy_census_has_draw_properties":"hierarchy_nodes" in state}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--scripts", type=Path, required=True)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--compiled", type=Path)
    args = parser.parse_args()
    result = compare(args.scripts, args.run, args.compiled)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({key:value for key,value in result.items() if key not in
                     ("depth_differences", "bone_differences", "semantic_candidate_differences")}, ensure_ascii=False))
    print("depth differences:", len(result["depth_differences"]), "bone differences:", len(result["bone_differences"]))
    print("semantic candidate differences:", len(result["semantic_candidate_differences"]))
