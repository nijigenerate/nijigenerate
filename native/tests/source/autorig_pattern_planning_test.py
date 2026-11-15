"""Exercise automatic construction, driver links, manual replacement and neutral invariants."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def part(uuid, masks=(), blend='Normal'):
    return dict(uuid=uuid, type='Part', name=f'opaque-label-{uuid}',
                mesh=dict(verts=[0, 0, 10, 0, 10, 4, 0, 4], indices=[0, 1, 2, 0, 2, 3]),
                masks=[dict(source=s, mode='Mask') for s in masks], blend_mode=blend)


def parameter(uuid, target, collapse=False):
    axis = [0, 1] if collapse else [-1, 0, 1]
    values = []
    for x in axis:
        offsets = [[0, 4*x], [0, 4*x], [0, 0], [0, 0]] if collapse else [[x, 0]]*4
        values.append([offsets]*3)
    return dict(uuid=uuid, name=f'input-{uuid}', is_vec2=True, min=[axis[0], -1], max=[1, 1],
                defaults=[0, 0], axis_points=[[0, 1] if collapse else [0, .5, 1], [0, .5, 1]],
                bindings=[dict(node=target, param_name='deform', values=values,
                               isSet=[[True]*3 for _ in axis], interpolate_mode='Linear')])


def main():
    binary = str(Path(sys.argv[1]).resolve())
    fixture = dict(nodes=dict(uuid=1, type='Node', name='root', children=[
        part(2), part(3, [2]), part(4, [3]), part(5), part(6, [5], 'Multiply')]),
        param=[parameter(50, 2, True), parameter(60, 5)])
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)

        def run(name, document, overrides=None):
            encoded = json.dumps(document).encode()
            source = root / f'{name}.inx'
            payload = b'TEX_SECT'+struct.pack('>I', 0)
            source.write_bytes(b'TRNSRTS\0'+struct.pack('>I', len(encoded))+encoded+payload)
            output = root / name
            args = [binary, str(source), str(output)]
            if overrides is not None:
                recipes = root / f'{name}.json'
                recipes.write_text(json.dumps(overrides))
                args.append(str(recipes))
            completed = subprocess.run(args, capture_output=True, text=True)
            assert completed.returncode == 0, (name, completed.returncode, completed.stdout, completed.stderr)
            report = json.loads((output/'report.json').read_text())
            data = Path(report['output']).read_bytes()
            length = int.from_bytes(data[8:12], 'big')
            assert data[12+length:] == payload
            return output, {p['pattern']: p for p in report['patterns']}, json.loads(data[12:12+length])

        output, reports, after = run('auto', fixture)
        for pattern in ('p13', 'p14', 'p18', 'p19', 'p20'):
            assert reports[pattern]['state'] == 'applied', (pattern, reports[pattern])
            assert json.loads((output/f'{pattern}-verification.json').read_text())['automatic_plan_verified']
        assert after['nodes'] == fixture['nodes'], 'Geometry, original masks and neutral art must remain intact'
        original_ids = {p['uuid'] for p in fixture['param']}
        created = [p for p in after['param'] if p['uuid'] not in original_ids]
        assert created
        for p in created:
            neutral = [axis.index((0-lo)/(hi-lo)) for axis, lo, hi in zip(p['axis_points'], p['min'], p['max'])]
            for binding in p['bindings']:
                value = binding['values'][neutral[0]][neutral[1]]
                assert value == (1 if binding['param_name'] == 'opacity' else [[0, 0]]*4)
        driver = next(p for p in after['param'] if p['uuid'] == 50)
        followers = driver['bindings'][1:]
        assert len(followers) == 2
        assert all(b['param_name'] == 'deform' for b in followers)
        for binding in followers:
            assert binding['values'][0][1] == [[0, 0]]*4
            assert binding['values'][-1][1] == [[0, 2]]*4

        renamed = copy.deepcopy(fixture)
        for node in renamed['nodes']['children']:
            node['name'] = 'unrelated'
        for p in renamed['param']:
            p['name'] = 'random-'+str(p['uuid'])
        other, _, _ = run('renamed', renamed)
        for pattern in ('p13', 'p14', 'p18', 'p19', 'p20'):
            a = json.loads((output/f'{pattern}-plan.json').read_text())
            b = json.loads((other/f'{pattern}-plan.json').read_text())
            assert a == b, 'Names must not affect roles, recipe or drivers'

        _, reports, selected = run('manual', fixture, {'p18': {'selection': {
            'mode': 'manual', 'matches': [{'roles': {'target': [4], 'support': [2]}}]}}})
        assert reports['p18']['state'] == 'applied'
        assert [b['node'] for p in selected['param'] if p['uuid'] == 50
                for b in p['bindings'][1:] if b['param_name'] == 'deform'] == [4]
        _, reports, empty = run('empty', fixture, {'p18': {'selection': {'mode': 'manual', 'matches': []}}})
        assert reports['p18']['state'] == 'failed'
        assert next(p for p in empty['param'] if p['uuid'] == 50)['bindings'][0] == fixture['param'][0]['bindings'][0]
        assert len([b for p in empty['param'] if p['uuid'] == 50 for b in p['bindings']
                    if b['param_name'] == 'deform']) == 1
        _, reports, empty = run('empty-condition', fixture, {'p18': {'selection': {
            'mode': 'auto', 'condition': {'type': 'Part', 'min_aspect': 100}}}})
        assert reports['p18']['state'] == 'failed'
        assert next(p for p in empty['param'] if p['uuid'] == 50)['bindings'][0] == fixture['param'][0]['bindings'][0]
        assert len([b for p in empty['param'] if p['uuid'] == 50 for b in p['bindings']
                    if b['param_name'] == 'deform']) == 1

        _, reports, second = run('second', after)
        assert not any(p['state'] == 'applied' for p in reports.values()), 'Do not duplicate generated controls'
        assert second == after
        renamed_generated = copy.deepcopy(after)
        for p in renamed_generated['param']:
            p['name'] = 'renamed-'+str(p['uuid'])
        _, reports, second = run('renamed-generated', renamed_generated)
        assert not any(p['state'] == 'applied' for p in reports.values())
        assert second == renamed_generated
        unrigged = copy.deepcopy(fixture)
        unrigged['param'] = []
        _, reports, independent = run('unrigged-lighting', unrigged)
        assert reports['p13']['state'] == reports['p14']['state'] == 'applied'
        surface_inputs = [p for p in independent['param'] if p.get('structural_driver_surface') == 5]
        assert len(surface_inputs) == 1, 'Lighting layers must share one generated input per supporting surface'
        assert len(surface_inputs[0]['bindings']) == 4, 'Both opacity and displacement must be driven'
        assert independent['nodes'] == unrigged['nodes']
        for pattern in ('p03', 'p06', 'p07', 'p22', 'p23', 'p24', 'p25', 'p26', 'p27', 'p29', 'p32', 'p34', 'p35'):
            roles = {'target': [3]}
            if pattern in ('p07', 'p23', 'p25', 'p26', 'p27'):
                roles['support'] = [2]
            _, result, _ = run('roles-'+pattern, fixture, {pattern: {'selection': {
                'mode': 'manual', 'matches': [{'roles': roles}]}}})
            assert result[pattern]['state'] == 'applied', (pattern, result[pattern])
        for pattern, roles, intent in (
            ('p01', {'target': [3]}, {'source_node': 5, 'source_parameter': 60}),
            ('p04', {'target': [3], 'surface': [5]}, {}),
            ('p02', {'target': [5]}, {'source_parameter': 50, 'destination_parameter': 60,
                                    'input_axis': 'X', 'output_axis': 'X'}),
            ('p05', {'target': [3], 'support': [2]}, {}),
            ('p12', {'target': [3]}, {'gain_x': 2}),
            ('p21', {'target': [3]}, {})):
            _, result, _ = run('intent-'+pattern, fixture, {pattern: {'selection': {
                'mode': 'manual', 'matches': [{'roles': roles}], 'intent': intent}}})
            assert result[pattern]['state'] == 'applied', (pattern, result[pattern])
        seam = copy.deepcopy(fixture)
        seam['nodes']['children'][3]['transform'] = {'trans': [10, 0, 0]}
        _, reports, welded = run('seam', seam, {'p16': {'selection': {
            'mode': 'manual', 'matches': [{'roles': {'target': [2], 'seam': [5]}}]}}})
        assert reports['p16']['state'] == 'applied', reports['p16']
        link = welded['nodes']['children'][0]['weldedLinks'][0]
        assert sum(i >= 0 for i in link['indices']) == 2
        _, reports, _ = run('anchors', seam, {'p28': {'selection': {
            'mode': 'manual', 'matches': [{'roles': {'target': [3], 'support': [2, 5]}}]}}})
        assert reports['p28']['state'] == 'applied', reports['p28']
        opening = copy.deepcopy(fixture)
        opening['nodes']['children'][1]['mesh'] = dict(
            verts=[0, 0, 5, 0, 10, 0, 10, 4, 5, 4, 0, 4],
            indices=[0, 1, 5, 1, 4, 5, 1, 2, 4, 2, 3, 4])
        _, reports, _ = run('opening', opening, {'p17': {'selection': {
            'mode': 'manual', 'matches': [{'roles': {'target': [3]}}]}}})
        assert reports['p17']['state'] == 'applied', reports['p17']
        for pattern, roles, intent in (
            ('p11', {'target': [3]}, {}),
            ('p15', {'target': [3]}, {'blend_mode': 'Screen'}),
            ('p13', {'target': [3]}, {'surface_node': 5, 'driver_parameter': 60}),
            ('p14', {'target': [3], 'surface': [5]}, {'driver_parameter': 60})):
            _, reports, _ = run('settings-'+pattern, fixture, {pattern: {'selection': {
                'mode': 'manual', 'matches': [{'roles': roles}], 'intent': intent}}})
            assert reports[pattern]['state'] == 'applied', (pattern, reports[pattern])
        physics = copy.deepcopy(fixture)
        physics['nodes']['children'].extend([
            dict(uuid=7, type='SimplePhysics', name='unnamed'),
            dict(uuid=8, type='MeshGroup', name='unnamed', dynamic_deformation=False)])
        for pattern, roles, intent in (
            ('p30', {'target': [7], 'support': [5]}, {'property': 'transform.t.x', 'gain_x': 2}),
            ('p31', {'target': [7]}, {'property': 'outputScale.x', 'gain_x': .2}),
            ('p33', {'target': [8]}, {'property': 'dynamic_deformation', 'value': True})):
            _, reports, _ = run('node-'+pattern, physics, {pattern: {'selection': {
                'mode': 'manual', 'matches': [{'roles': roles}], 'intent': intent}}})
            assert reports[pattern]['state'] == 'applied', (pattern, reports[pattern])
    print('Automatic plans: neutral preservation, driver motion, name invariance, manual replacement and no duplicate controls passed.')


if __name__ == '__main__':
    main()
