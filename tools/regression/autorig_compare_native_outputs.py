"""Compare saved original Python/D native resources exclusively through NJC."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def maximum_error(a,b):
    x,y = np.asarray(a,dtype=float),np.asarray(b,dtype=float)
    if x.shape != y.shape:
        raise ValueError(f'Native resource shapes differ: {x.shape} versus {y.shape}')
    return float(np.max(np.abs(x-y))) if x.size else 0.


def walk(items):
    for item in items:
        yield item
        yield from walk(item.get('children') or [])


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--scripts',type=Path,required=True)
    parser.add_argument('--njc',type=Path,required=True)
    parser.add_argument('--python',type=Path,required=True)
    parser.add_argument('--state',type=Path,required=True)
    parser.add_argument('--d-model',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    sys.path.insert(0,str(args.scripts))
    from riglib.live import Live
    from riglib.model import _trs, _mesh
    n=Live(args.njc)
    def snapshot_nodes():
        roots=[item for item in n.find('*')['items'] if item['typeId']!='Parameter']
        nodes=list(walk(roots))
        values={item['uuid']:n.read(item['uuid'])['item']['data'] for item in nodes}
        result={}
        def visit(item,parent):
            data=values[item['uuid']]
            local,_,_=_trs(data.get('transform',{}))
            world=np.asarray(parent)@np.asarray(local) if parent is not None else None
            if data.get('lockToRoot') or data.get('pinToMesh'):
                world=None
            mesh,_=_mesh(data,True)
            result[item['uuid']]={'mesh':mesh,'nominal_world_matrix':world}
            for child in item.get('children') or []:
                visit(child,world)
        for root in roots:
            visit(root,np.eye(4))
        return result
    state=read(args.state)
    original=read(args.python/'native-state.json')
    saved_bones=read(args.python/'scaffold-readback.json')['native_bones']
    bone_items=list(walk(n.find('DepthBone')['items']))
    bone_ids={item['name']:item['uuid'] for item in bone_items}
    parameters={item['name']:item['uuid'] for item in n.find('Parameter')['items']}
    target_ids={original['bones'][name]:uid for name,uid in bone_ids.items()}
    grids={target['domain_id']:target['grid'] for target in state['targets']}
    target_ids.update({original['grids'][name]:uid for name,uid in grids.items()})
    target_ids.update({uid:uid for uid in original['child_grids']})
    bones=[]
    for name,expected in saved_bones.items():
        actual=n.read(bone_ids[name])['item']['data']
        errors={key:maximum_error(expected[key],actual[key]) for key in ['restHead','restTail','hingeAxis','restRoll']}
        flags={key:expected[key]==actual[key] for key in ['allowParentToTargets','lockToRoot',
            'lockRotation','lockTranslation','pinToMesh']}
        errors.update({'transform.'+key:maximum_error(expected['transform'][key],actual['transform'][key])
                       for key in ['trans','rot','scale']})
        bones.append({'name':name,'errors':errors,'flags':flags})
    bindings=[]
    for expected in read(args.python/'depth-angle-program.json')['bindings']:
        source=expected['target']['uuid']
        if source not in target_ids:
            continue
        target=target_ids[source]
        parameter=parameters[expected['parameter']['name']]
        uri=f'resource://nijigenerate/bindings/get?parameter={parameter}&target={target}&name={expected["name"]}'
        actual=n.read_resources([uri])[0]['item']
        if actual is None:
            bindings.append({'target':expected['target']['name'],'parameter':expected['parameter']['name'],
                'name':expected['name'],'available':False,
                'maximum_expected_value':float(np.max(np.abs(expected['data']['values']))),
                'maximum_error':None,'keys_equal':False})
            continue
        bindings.append({'target':expected['target']['name'],'parameter':expected['parameter']['name'],
            'name':expected['name'],'available':True,'axes_error':max(maximum_error(a,b) for a,b in
                zip(expected['axisValues'],actual['axisValues'],strict=True)),
            'keys_equal':expected['data']['isSet']==actual['data']['isSet'],
            'maximum_error':maximum_error(expected['data']['values'],actual['data']['values'])})
    d_nodes=snapshot_nodes()
    try:
        n.open(args.python/'rigged.inx')
        p_nodes=snapshot_nodes()
    finally:
        n.open(args.d_model)
    materials=[]
    for material in state['materials']:
        if material['static']:
            continue
        uid=material['uuid']
        a,b=p_nodes[uid],d_nodes[uid]
        av,bv=np.asarray(a['mesh']['vertices']),np.asarray(b['mesh']['vertices'])
        am,bm=a['nominal_world_matrix'],b['nominal_world_matrix']
        world_error=None
        if am is not None and bm is not None:
            am,bm=np.asarray(am),np.asarray(bm)
            world_error=maximum_error(av@am[:2,:2].T+am[:2,3],bv@bm[:2,:2].T+bm[:2,3])
        materials.append({'part':uid,'triangles_equal':a['mesh']['indices']==b['mesh']['indices'],
            'uv_error':maximum_error(a['mesh']['uvs'],b['mesh']['uvs']),
            'nominal_world_vertex_error':world_error})
    report={'bones':bones,'bindings':bindings,'materials':materials,
            'policy':'Saved NJC resources; nominal affine mesh comparison is separate from actual render comparison'}
    report['passed']=(len(bone_items)==len(saved_bones) and set(bone_ids)==set(saved_bones) and
        all(all(row['flags'].values()) and max(row['errors'].values())<=1e-4 for row in bones) and
        len(bindings)==len(read(args.python/'depth-angle-program.json')['bindings']) and
        all(row['available'] and row['keys_equal'] and row['axes_error']<=1e-6 and
            row['maximum_error']<=.0003 for row in bindings) and
        all(row['triangles_equal'] and row['uv_error']<=1e-7 and
            row['nominal_world_vertex_error'] is not None and row['nominal_world_vertex_error']<=.0003
            for row in materials))
    args.out.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('Bones',len(bones),'bindings',len(bindings),'materials',len(materials))
    print('Bone maximum error',max(error for row in bones for error in row['errors'].values()))
    print('Binding maximum error',max(row['maximum_error'] for row in bindings if row['available']))
    print('Unavailable bindings',[row for row in bindings if not row['available']])
    print('Material UV maximum error',max(row['uv_error'] for row in materials))
    print('Material world maximum error',max((row['nominal_world_vertex_error'] or 0) for row in materials))
    if not report['passed']:
        raise SystemExit('Saved original Python/D native resources differ')


if __name__=='__main__':
    main()
