"""Compare every original local-control and cheek key with saved NJC bindings."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--scripts',type=Path,required=True)
    parser.add_argument('--njc',type=Path,required=True)
    parser.add_argument('--python',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    sys.path.insert(0,str(args.scripts))
    from riglib.live import Live
    n=Live(args.njc)
    parameters={row['name']:row['uuid'] for row in n.find('Parameter')['items']}
    cache={}
    reports={}
    for label,file in [('controls','shape-controls-program.json'),('cheeks','shape-corrections-program.json')]:
        original=json.loads((args.python/file).read_text(encoding='utf-8'))
        rows=[]
        for operation in original['operations']:
            owner=(operation['target'],operation['parameter'])
            if owner not in cache:
                target,parameter=owner
                uri=f'resource://nijigenerate/bindings/get?parameter={parameters[parameter]}&target={target}&name=deform'
                cache[owner]=n.read_resources([uri])[0]['item']
            binding=cache[owner]
            expected=np.asarray(operation['values'])
            if binding is None:
                rows.append({'target':owner[0],'parameter':owner[1],'key':operation['key'],
                    'binding_available':False,'maximum_expected_value':float(np.max(np.abs(expected)))})
                continue
            x,y=[axis.index(value) for axis,value in zip(binding['axisValues'],operation['key'],strict=True)]
            actual=np.asarray(binding['data']['values'][x][y]).ravel()
            if actual.shape != expected.shape:
                raise ValueError(f'Saved local field shape differs: {owner}, {operation["key"]}')
            rows.append({'target':owner[0],'parameter':owner[1],'key':operation['key'],
                'binding_available':True,'key_set':binding['data']['isSet'][x][y],
                'maximum_error':float(np.max(np.abs(actual-expected)))})
        reports[label]={'count':len(rows),'operations':rows,
            'maximum_error':max((row['maximum_error'] for row in rows if row['binding_available']),default=0.),
            'unavailable':[row for row in rows if not row['binding_available']]}
    reports['passed']=all(report['maximum_error']<=1e-4 and
        all(row['maximum_expected_value']==0 for row in report['unavailable']) and
        all(row['key_set'] for row in report['operations'] if row['binding_available'])
        for report in reports.values())
    args.out.write_text(json.dumps(reports,indent=2),encoding='utf-8')
    print(json.dumps({label:{key:value for key,value in report.items() if key!='operations'}
        for label,report in reports.items() if label!='passed'}))
    if not reports['passed']:
        raise SystemExit('Saved Python/D local fields differ')


if __name__=='__main__':
    main()
