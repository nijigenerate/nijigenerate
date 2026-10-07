"""Create review sheets exclusively from an original Python run's renders."""
import argparse
import hashlib
import html
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    data = json.loads((args.run/'validation.json').read_text(encoding='utf-8'))
    rows = list(data['rendered'])
    support = json.loads((args.run/'head-support-review.json').read_text(encoding='utf-8'))
    rows.extend({**r, 'label': 'support-after-'+r['label']} for r in support['rendered'])
    images = {}
    bounds = []
    for row in rows:
        path = Path(row['file'])
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest.lower() != row['sha256'].lower():
            raise ValueError('Original render hash differs: '+str(path))
        images[row['label']] = Image.open(path).convert('RGBA')
        bounds.append(images[row['label']].getbbox())
    body = (min(b[0] for b in bounds)-12, min(b[1] for b in bounds)-12,
            max(b[2] for b in bounds)+12, max(b[3] for b in bounds)+12)
    # One constant camera-space region for every face pose and local control.
    face = (760, 60, 1160, 410)
    local = (855, 190, 1055, 330)
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
        small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 15)
    except OSError:
        font = small = ImageFont.load_default()
    sheets = []

    def sheet(name, title, selected, columns, crop, scale=1):
        w, h = round((crop[2]-crop[0])*scale), round((crop[3]-crop[1])*scale)
        top, label_height, gap = 62, 64, 8
        canvas = Image.new('RGB', (columns*(w+gap)+gap,
                                  top+math.ceil(len(selected)/columns)*(h+label_height+gap)), '#f4f4f4')
        draw = ImageDraw.Draw(canvas)
        draw.text((12, 10), 'PYTHON ONLY / '+title, font=font, fill='black')
        draw.text((12, 36), 'Same camera and fixed crop; labels are actual parameter values', font=small, fill='#333333')
        for i, row in enumerate(selected):
            x = gap+(i % columns)*(w+gap)
            y = top+(i//columns)*(h+label_height+gap)
            draw.text((x+4, y+4), row['label'], font=small, fill='black')
            for j, (parameter, value) in enumerate(row['pose'].items()):
                draw.text((x+4, y+23+j*17), parameter+' = '+str(value), font=small, fill='black')
            if not row['pose']:
                draw.text((x+4, y+23), 'All parameters at defaults', font=small, fill='black')
            original = images[row['label']].crop(crop)
            background = Image.new('RGBA', original.size, '#cccccc')
            background.alpha_composite(original)
            tile = background.convert('RGB')
            if scale != 1:
                tile = tile.resize((w, h), Image.Resampling.LANCZOS)
            canvas.paste(tile, (x, y+label_height))
        filename = name+'.png'
        canvas.save(args.out/filename)
        sheets.append({'file': filename, 'title': title, 'poses': [r['label'] for r in selected],
                       'fixed_crop': crop, 'common_display_scale': scale})

    def matching(parameter):
        result = [r for r in rows[:len(data['rendered'])] if list(r['pose']) == [parameter]]
        # X increases left-to-right; Y increases from top-to-bottom.
        return sorted(result, key=lambda r: (r['pose'][parameter][1] if len(r['pose'][parameter])>1 else 0,
                                            r['pose'][parameter][0]))

    for parameter, crop, scale in [('Face::Yaw-Pitch', face, 1), ('Body::Yaw-Pitch', body, .5),
                                   ('Face::Roll', face, 1), ('Body::Roll', body, .5)]:
        selected = matching(parameter)
        name = parameter.replace('::', '-')
        sheet(name, parameter+' / all keys', selected, 5, crop, scale)
        if 'Yaw-Pitch' in parameter:
            extremes = [r for r in selected if all(v in (-1, 0, 1) for v in r['pose'][parameter])]
            sheet(name+'-overview', parameter+' / extremes and neutral', extremes, 3, crop, 1 if crop==face else .6)
    for parameter in ['Eye::L::Blink', 'Eye::R::Blink', 'Eye::L::X-Y', 'Eye::R::X-Y',
                      'Eyebrow::L', 'Eyebrow::R', 'Mouth::Open']:
        sheet(parameter.replace('::', '-'), parameter+' / all keys', matching(parameter), 5, local, 1.6)
    sheet('support', 'Body / Face support poses', rows[len(data['rendered']):], 3, body, .6)
    manifest = {'source': str(args.run.resolve()), 'source_model': str((args.run/'rigged.inx').resolve()),
                'policy': 'Original Python renders only; hashes verified; no pose-specific fitting or retouching',
                'poses': rows, 'sheets': sheets}
    (args.out/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    sections = ''.join('<h2>'+html.escape(s['title'])+'</h2><a href="'+s['file']+'"><img src="'+s['file']+'"></a>' for s in sheets)
    originals = ''.join('<li><a href="'+html.escape(Path(r['file']).as_uri())+'">'+html.escape(r['label']+' '+str(r['pose']))+'</a></li>' for r in rows)
    page = '<!doctype html><meta charset="utf-8"><title>Python rig reference</title><style>body{font:16px sans-serif;background:#eee;margin:24px}img{max-width:100%;height:auto}h2{margin-top:40px}</style><h1>Python rig reference</h1><p>Python版だけの実行結果。各画像をクリックすると原寸の比較シートを開きます。</p>'+sections+'<h2>Original renders / 180 poses</h2><ul>'+originals+'</ul>'
    (args.out/'index.html').write_text(page, encoding='utf-8')
    print('Verified', len(rows), 'original Python renders; created', len(sheets), 'review sheets')


if __name__ == '__main__':
    main()
