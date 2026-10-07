"""Compare completed Python output against a D program from the same source.

No INX parsing: both inputs are the compilers' own JSON artifacts. Numerical
fields are sampled at the D coordinates because Python's final native AutoMesh
coordinates have float32 rounding.
"""
import argparse
import json
from pathlib import Path

import numpy as np


def sample(xs, ys, values, query):
    xs, ys, q = np.asarray(xs), np.asarray(ys), np.asarray(query)
    q = np.clip(q, [xs[0], ys[0]], [xs[-1], ys[-1]])
    grid = np.asarray(values).reshape(len(ys), len(xs))
    i = np.clip(np.searchsorted(xs, q[:, 0], side="right")-1, 0, len(xs)-2)
    j = np.clip(np.searchsorted(ys, q[:, 1], side="right")-1, 0, len(ys)-2)
    u = (q[:, 0]-xs[i])/(xs[i+1]-xs[i])
    v = (q[:, 1]-ys[j])/(ys[j+1]-ys[j])
    return (1-v)*((1-u)*grid[j, i]+u*grid[j, i+1])+v*((1-u)*grid[j+1, i]+u*grid[j+1, i+1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--d", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    py = json.loads(args.python.read_text(encoding="utf-8"))
    d = json.loads(args.d.read_text(encoding="utf-8"))
    bones = {b["id"]: b for b in d["scaffold"]["bones"]}
    expected = {b["id"]: b for b in py["scaffold"]["bones"]}
    curves = {(c["parameter"], c["bone"], c["property"]): c
              for p in py["parameters"] for c in p.get("reference_curves", [])}
    actual = {(c["parameter"], c["bone"], c["binding"]): c for c in d["native_drivers"]}
    report = dict(bone_names_match=set(bones) == set(expected),
                  driver_properties_match=set(actual) == set(curves), bones=[], curves=[], surfaces=[])
    for name in sorted(set(bones) & set(expected)):
        a, b = expected[name], bones[name]
        report["bones"].append(dict(bone=name,
            constraints_match=all(a[k] == b[k] for k in ("parent", "lock_to_root", "allow_parent_to_targets")),
            maximum_endpoint_error=max(abs(x-y) for k in ("head", "tail") for x, y in zip(a[k], b[k])),
            pose_origin_z_error=abs(a["pose_origin_z"]-b["pose_origin_z"])))
    length = py["reference_template"]["torso_length"]
    for key in sorted(set(curves) & set(actual)):
        a, b = curves[key], actual[key]
        values = {(tuple(c["key"])): c["value"] for c in b["values"]}
        errors = [abs(values[(x, y)]-a["values"][i][j]*(length if a["units"] == "torso_length" else 1))
                  for i, x in enumerate(a["axes"][0]) for j, y in enumerate(a["axes"][1])]
        report["curves"].append(dict(parameter=key[0], bone=key[1], property=key[2], maximum_error=max(errors)))
    for a in py["domains"]:
        b = next(c for c in d["carriers"] if c["domain_id"] == a["id"])
        q = [[x, y] for y in b["ys"] for x in b["xs"]]
        expected_depth = sample(a["axis_x"], a["axis_y"], a["depth_model_units"], q)
        origin = np.asarray(a["carrier_frame"]["origin"])
        actual_origin = np.asarray([b["parent_to_root"][2], b["parent_to_root"][5]])
        report["surfaces"].append(dict(domain=a["id"], component_match=a["reference_component"] == b["reference_component"],
            origin_error=float(np.max(abs(origin-actual_origin))),
            maximum_depth_error_model=float(np.max(abs(expected_depth-b["depth_model_units"])))))
    report["native_depth_scale_error"] = abs(py["native_depth_scale"]-d["native_depth_scale"])
    args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if not isinstance(v, list)}))
    print("Maximum endpoint error", max(r["maximum_endpoint_error"] for r in report["bones"]))
    print("Maximum driver error", max(r["maximum_error"] for r in report["curves"]))
    print("Maximum resampled surface error", max(r["maximum_depth_error_model"] for r in report["surfaces"]))


if __name__ == "__main__":
    main()
