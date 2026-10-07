"""Compare complete original Python artifacts with actual imported-model D state.

Reads compiler JSON only. It neither constructs synthetic artwork nor reads INX.
"""
import argparse
import json
from pathlib import Path

import numpy as np


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def numeric_error(left, right):
    a, b = np.asarray(left, dtype=float), np.asarray(right, dtype=float)
    if a.shape != b.shape:
        raise ValueError(f'Different numerical shapes: {a.shape} versus {b.shape}')
    return float(np.max(np.abs(a-b))) if a.size else 0.


def compare_operations(expected, actual):
    missing = sorted(set(expected)-set(actual))
    extra = sorted(set(actual)-set(expected))
    errors = []
    for key in sorted(set(expected) & set(actual)):
        try:
            errors.append({'target':key[0], 'parameter':key[1], 'key':key[2:],
                           'maximum_error':numeric_error(expected[key], actual[key])})
        except ValueError as error:
            errors.append({'target':key[0], 'parameter':key[1], 'key':key[2:], 'error':str(error)})
    return {'expected_count':len(expected), 'actual_count':len(actual), 'missing':missing, 'extra':extra,
            'maximum_error':max((r.get('maximum_error',0) for r in errors), default=0),
            'shape_errors':[r for r in errors if 'error' in r], 'operations':errors}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--python', type=Path, required=True)
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--replay', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    state, program, replay = read(args.state), read(args.program), read(args.replay)
    py = lambda name: read(args.python/name)
    report = {'scope':'Actual original Python run and D imported-model state; no generated artwork fixtures'}
    materials = {m['uuid']:m for m in state['materials'] if not m['static']}
    semantics = py('semantics.json')['materials']
    report['semantics'] = {'python_count':len(semantics), 'd_count':len(materials), 'differences':[]}
    for m in semantics:
        d = materials.get(m['part'])
        if d is None or (m['role'],m.get('feature') or '') != (d['role'],d.get('feature') or ''):
            report['semantics']['differences'].append({'part':m['part'],'python':m,'d':d})
    controls = py('shape-controls-program.json')
    expected = {(r['target'],r['parameter'],*r['key']):r['values'] for r in controls['operations']}
    actual = {}
    contact = []
    for mechanism in replay['controls']['mechanisms']:
        name, xs, ys = mechanism['name'], mechanism['axisX'], mechanism['axisY']
        for operation in mechanism['operations']:
            for key in operation['keys']:
                x, y = key['key']
                actual[(operation['part'],name,xs[x],ys[y])] = key['offsets']
        p = controls['parameters'][name]
        assert p['axes'] == [xs,ys], name+' axes differ'
        if 'contact_curves' in mechanism:
            a,b = p['contact_curves'],mechanism['contact_curves']
            fields = ['frame_origin','tangent','neutral_target','upper_lower_edge','lower_upper_edge','open_sclera_lower_reference']
            contact.append({'parameter':name,'fields':{f:numeric_error(a[f],b[f]) for f in fields if f in a and f in b}})
    report['local_controls'] = compare_operations(expected,actual)
    report['contact_profiles'] = contact
    report['draw_order_equal'] = controls['face_draw_order']['operations'] == replay['controls']['draw_order']
    corrections = py('shape-corrections-program.json')
    expected = {(r['target'],r['parameter'],*r['key']):r['values'] for r in corrections['operations']}
    axis = [-1.,-.5,0.,.5,1.]
    actual = {(r['part'],'Face::Yaw-Pitch',axis[r['key'][0]],axis[r['key'][1]]):r['values']
              for r in replay['corrections']['operations']}
    report['cheek_corrections'] = compare_operations(expected,actual)
    skin = replay['corrections']['skin']
    expected_pins = next(r['protected_vertices'] for r in corrections['operations'] if r['target']==skin)
    actual_pins = np.flatnonzero(replay['corrections']['skin_protected_vertices']).tolist()
    report['cheek_anchor_vertices_equal'] = expected_pins == actual_pins
    shoulders = py('shoulder-welding-applied.json')['pairs']
    report['shoulders'] = []
    for p in shoulders:
        d = next(r for r in replay['shoulders'] if r['source']==p['source'] and r['target']==p['target'])
        fields = {f:numeric_error(p['frame'][f],d[f]) for f in ['origin','tangent','up','arm_axis','upper_arm_length']}
        report['shoulders'].append({'source':p['source'],'target':p['target'],'frame_errors':fields,
                                    'matched_fraction_error':abs(p['matched_fraction']-d['matched_fraction']),
                                    'matched_span_error':abs(p['matched_span']-d['matched_span']),
                                    'matching_equal':d['matching']==(p['weight']==0),
                                    'saved_indices_equal':p.get('indices')==next(r['indices'] for r in state['shoulder_pairs'] if r['target']==p['target'])})
    ph = py('program.json')['hierarchy']
    dh = program['hierarchy']
    report['hierarchy'] = {key:ph[key]==dh[key] for key in ['render_scope','surface_parents','body_origin','face_origin','clipping_receivers']}
    report['hierarchy']['group_order_equal'] = [g['id'] for g in ph['groups']] == [g['id'] for g in dh['groups']]
    group_errors = []
    for left,right in zip(ph['groups'],dh['groups']):
        fields = set(left) | set(right)
        mismatches = []
        maximum = 0.
        for field in fields:
            if field not in left or field not in right:
                mismatches.append(field)
            elif field == 'origin':
                maximum = max(maximum,numeric_error(left[field],right[field]))
            elif left[field] != right[field]:
                mismatches.append(field)
        group_errors.append({'id':left['id'],'mismatches':mismatches,'maximum_origin_error':maximum})
    report['hierarchy']['groups'] = group_errors
    report['passed'] = (
        not report['semantics']['differences'] and report['draw_order_equal'] and
        report['cheek_anchor_vertices_equal'] and
        all(not report[name]['missing'] and not report[name]['extra'] and
            not report[name]['shape_errors'] and report[name]['maximum_error']<=1e-4
            for name in ['local_controls','cheek_corrections']) and
        all(all(error<=1e-8 for error in row['frame_errors'].values()) and
            row['matched_fraction_error']<=1e-8 and row['matched_span_error']<=1e-8 and
            row['matching_equal'] and row['saved_indices_equal'] for row in report['shoulders']) and
        all(report['hierarchy'][key] for key in ['render_scope','surface_parents','body_origin',
            'face_origin','clipping_receivers','group_order_equal']) and
        all(not row['mismatches'] and row['maximum_origin_error']<=1e-8 for row in group_errors)
    )
    args.out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['local_controls','cheek_corrections']},ensure_ascii=False))
    for name in ['local_controls','cheek_corrections']:
        print(name,{k:v for k,v in report[name].items() if k!='operations'})
    if not report['passed']:
        raise SystemExit('Original Python/D logic comparison did not pass')


if __name__ == '__main__':
    main()
