"""Integration checks for unconditional processor execution and isolated failures."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='autorig-batch-test-') as temporary:
        root = Path(temporary)
        document = {'nodes': {'uuid': 1, 'type': 'Node', 'name': 'root', 'children': [
            {'uuid': 2, 'type': 'Part', 'name': 'consumer',
             'mesh': {'verts': [0, 0, 2, 0, 0, 2]}, 'masks': [{'source': 3, 'mode': 'Mask'}]},
            {'uuid': 3, 'type': 'Part', 'name': 'source',
             'mesh': {'verts': [0, 0, 2, 0, 0, 2]}, 'masks': []}]}, 'param': []}
        encoded = json.dumps(document).encode()
        payload = b'TEX_SECT' + struct.pack('>I', 0)
        model = root / 'incomplete-base.inx'
        model.write_bytes(b'TRNSRTS\0' + struct.pack('>I', len(encoded)) + encoded + payload)

        def run(name, extra=()):
            directory = root / name
            subprocess.run([executable, str(model), str(directory), *extra], check=True, capture_output=True)
            report = json.loads((directory / 'report.json').read_text())
            patterns = {p['pattern']: p for p in report['patterns']}
            assert set(patterns) == {'p%02d' % n for n in range(1, 36)}
            for p in patterns.values():
                assert p['requested_task'] == 'verify'
                assert p['tasks']['detect']['attempts'] == 1
                assert p['tasks']['deform']['attempts'] == 1
                assert p['verify'] in ('Succeeded', 'Blocked')
            result = Path(report['output']).read_bytes()
            size = struct.unpack_from('>I', result, 8)[0]
            assert result[12+size:] == payload
            return report, patterns, json.loads(result[12:12+size])

        report, patterns, result = run('defaults')
        assert patterns['p08']['state'] == 'verified_unchanged'
        assert patterns['p31']['deform'] == 'Failed'
        assert patterns['p31']['verify'] == 'Blocked'
        assert result == document and not report['changed']

        recipes = root / 'recipes.json'
        recipes.write_text(json.dumps({'p08': {'mask_mode': 'DodgeMask'}}))
        report, patterns, result = run('authored', [str(recipes)])
        assert patterns['p08']['state'] == 'applied' and report['changed']
        assert {'source': 3, 'mode': 'DodgeMask'} in result['nodes']['children'][0]['masks']

        recipes.write_text(json.dumps({'p08': {'mask_mode': 'invalid'}}))
        report, patterns, result = run('failure', [str(recipes)])
        assert patterns['p08']['deform'] == 'Failed'
        assert patterns['p35']['tasks']['deform']['attempts'] == 1
        assert result == document and not report['changed']
    print('All 35 processors executed; mutation, blocked verification, and failure isolation passed.')


if __name__ == '__main__':
    main()
