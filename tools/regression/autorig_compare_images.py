"""Compare identically captured Python and D rig poses without image alignment."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def compare_pose(name, python_path, d_path, output):
    left = np.asarray(Image.open(python_path).convert("RGBA"), dtype=np.float64)
    right = np.asarray(Image.open(d_path).convert("RGBA"), dtype=np.float64)
    if left.shape != right.shape:
        raise ValueError(f"{name}: render dimensions differ; recapture with the same viewport")
    # Compare premultiplied color: RGB outside the visible alpha is irrelevant.
    left_color = left[:, :, :3]*left[:, :, 3:4]/255
    right_color = right[:, :, :3]*right[:, :, 3:4]/255
    color_error = np.abs(left_color-right_color)
    alpha_error = np.abs(left[:, :, 3]-right[:, :, 3])
    occupied = (left[:, :, 3]>32) | (right[:, :, 3]>32)
    intersection = (left[:, :, 3]>32) & (right[:, :, 3]>32)
    changed = (color_error.max(axis=2)>2) | (alpha_error>2)
    yy, xx = np.indices(occupied.shape)
    background = np.where(((xx//16+yy//16)%2)[..., None], 224, 192)
    def checkerboard(rgba):
        alpha = rgba[:, :, 3:4]/255
        return Image.fromarray(np.uint8(np.clip(rgba[:, :, :3]*alpha+background*(1-alpha), 0, 255)))
    panels = [checkerboard(left), checkerboard(right),
              Image.fromarray(np.uint8(np.clip(color_error*4, 0, 255))),
              Image.fromarray(np.uint8(np.clip(alpha_error*4, 0, 255))).convert("RGB")]
    width, height = panels[0].size
    canvas = Image.new("RGB", (width*4, height+32), "white")
    draw = ImageDraw.Draw(canvas)
    for index, (label, panel) in enumerate(zip(("Python", "D", "Color difference x4", "Alpha difference x4"), panels)):
        draw.text((index*width+8, 8), label, fill="black")
        canvas.paste(panel, (index*width, 32))
    sheet = output/f"{name}.png"
    canvas.save(sheet)
    count = int(occupied.sum())
    return dict(pose=name, python=str(python_path.resolve()), d=str(d_path.resolve()),
                comparison=str(sheet.resolve()), width=width, height=height,
                occupied_pixels=count, silhouette_iou=float(intersection.sum()/count) if count else 1.,
                color_mae_on_support=float(color_error[occupied].mean()) if count else 0.,
                alpha_mae_on_support=float(alpha_error[occupied].mean()) if count else 0.,
                changed_pixel_fraction_on_support=float(changed[occupied].mean()) if count else 0.,
                maximum_color_error=float(color_error.max()), maximum_alpha_error=float(alpha_error.max()))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pairs", type=Path, required=True,
                        help='JSON array of {pose, python, d}; each image must have identical capture settings')
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    pairs = json.loads(args.pairs.read_text(encoding="utf-8"))
    if not pairs:
        raise ValueError("At least one captured pose is required")
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    for pair in pairs:
        name = pair["pose"]
        if not name or any(ch not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for ch in name):
            raise ValueError("Pose names must be filename-safe")
        results.append(compare_pose(name, Path(pair["python"]), Path(pair["d"]), args.out))
    report = dict(policy="Same capture settings; no geometric alignment, resizing, or independent cropping",
                  visual_review_required=True, poses=results)
    (args.out/"comparison.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    for result in results:
        print(result["pose"], "silhouette IoU", result["silhouette_iou"],
              "color MAE", result["color_mae_on_support"])


if __name__ == "__main__":
    main()
