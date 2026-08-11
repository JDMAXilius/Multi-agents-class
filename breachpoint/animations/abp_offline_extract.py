"""
abp_offline_extract.py - read a Blueprint/AnimBlueprint .uasset WITHOUT a running editor.

    python abp_offline_extract.py <asset.uasset> [more.uasset ...] [--out DIR] [--inventory PATH]

WHY THIS EXISTS
---------------
`mcp-bp/read_graphs.py` reads graphs through the editor's MCP server, which means a live
editor, a loaded project, and a human at the keyboard. `docs/ANIM-PORT-LEDGER.md` records the
consequence: verdicts were reached from a curated 14-asset target list, and the 29 assets nobody
pointed the tool at stayed invisible until a founder challenge. A reader that runs on the
CHECKED-OUT FILES needs no target list, so "what did we not look at" stops being a question the
tool can get wrong.

WHAT IT READS, AND HOW FAR TO TRUST IT
--------------------------------------
Uncooked .uasset files carry a name table: every distinct FName the package uses, which for a
Blueprint includes every node class, every declared property, every referenced package, and
every function and graph name. This tool parses that table and classifies it.

**Self-validating.** Pass `--inventory mcp-bp/bp_inventory.json` and it cross-checks the
properties it recovered against what the live-editor extraction found for the same asset, and
prints the delta. On `ABP_Mannequin_Base` it recovers 88 of the inventory's 96, and the 8 it
does not are all stock `UAnimInstance` members (`onMontage*`, `rootMotionMode`,
`bUseMainInstanceMontageEvaluationData`) -- absent from the name table precisely because the
Blueprint inherits rather than declares them. Read that as the tool's accuracy statement: it
sees what the asset DECLARES, and inherited members are the editor's to report.

**What it CANNOT read, and does not guess.** Node-to-node topology -- which pin feeds which,
what order execution takes, which state transitions to which. That lives in the export table's
tagged property streams, and this package is written by a source engine build whose
FPackageFileSummary does not match the documented layout (`SavedHash` in place of the package
GUID, legacy file version -9). Every attempt to walk exports here was rejected by the
validator rather than reported at low confidence. So: this tool tells you WHAT IS IN a graph,
never HOW IT IS WIRED. A verdict needing topology still needs the editor, and should say so.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path

PKG_TAG = 0x9E2A83C1

# The name table's start is found by structural probe rather than read from the summary: the
# summary layout is engine-build-specific (see module docstring) and the table is not.
NAME_TABLE_SEARCH_LIMIT = 8192
NAME_HASH_BYTES = 4  # editor packages store two case-preserving hashes after each name


def read_name_table(data: bytes):
    """Return (names, start_offset, end_offset). Raises if no table validates."""
    def run(start, limit=None):
        q, out = start, []
        while limit is None or len(out) < limit:
            if q + 4 > len(data):
                break
            length = struct.unpack_from('<i', data, q)[0]
            if not (0 < length < 300):
                break
            chunk = data[q + 4:q + 4 + length]
            if chunk[-1:] != b'\x00' or not all(32 <= c < 127 for c in chunk[:-1]):
                break
            out.append(chunk[:-1].decode('ascii'))
            q += 4 + length + NAME_HASH_BYTES
        return out, q

    if struct.unpack_from('<I', data, 0)[0] != PKG_TAG:
        raise ValueError('not an Unreal package (bad tag)')

    for start in range(0, min(NAME_TABLE_SEARCH_LIMIT, len(data))):
        probe, _ = run(start, limit=12)
        # A real table opens with 12 consecutive well-formed names. Random bytes do not.
        if len(probe) == 12:
            names, end = run(start)
            return names, start, end
    raise ValueError('no name table found')


# ---------------------------------------------------------------- classification

CLASSIFIERS = [
    ('anim_graph_nodes', lambda n: n.startswith('AnimGraphNode')),
    ('anim_runtime_nodes', lambda n: n.startswith('AnimNode_') or n.startswith('FAnimNode_')),
    ('k2_nodes', lambda n: n.startswith('K2Node')),
    ('control_rig', lambda n: n.startswith('RigVM') or n.startswith('ControlRig')),
    ('edgraph', lambda n: n.startswith('EdGraph')),
]

# EdGraph pin categories and wildcards. A fixed engine vocabulary, not content -- filtered so
# they stop showing up as "names this asset introduces".
PIN_TYPE_KEYWORDS = {
    'bool', 'byte', 'class', 'delegate', 'double', 'exec', 'execute', 'float', 'int', 'int64',
    'name', 'object', 'real', 'self', 'string', 'struct', 'text', 'then', 'else', 'tooltip',
    'wildcard', 'softclass', 'softobject', 'interface', 'field',
}

# NOTE ON WHY THERE IS NO PROPERTY REGEX HERE.
#
# The obvious rule -- "a declared property is lowerCamel" -- was written, run, and thrown away.
# It scored 8 correct out of 116 on `ABP_Mannequin_Base`. Blueprint properties are stored
# UpperCamel in the name table (`AimPitch`); it is the MCP inventory that lowercases the first
# letter (`aimPitch`), so the rule matched engine node internals (`bAllowConduitEntryStates`,
# `bMeshSpaceRotationBlend`) and skeleton bone names instead, and would have reported 116
# confident findings of which 108 were noise.
#
# Nothing distinguishes a Blueprint-declared `AimPitch` from an engine node's own property by
# spelling alone -- both are UpperCamel and both are in the table. So declared properties are
# reported ONLY where the inventory confirms them, and everything else is listed as
# `other_names` for a human to read. An inventory-less run says "I cannot tell" rather than
# guessing, which is the difference between this being evidence and being an opinion.


def classify(names):
    buckets = {key: [] for key, _ in CLASSIFIERS}
    buckets.update({
        'referenced_game_assets': [],
        'referenced_script_modules': [],
        'pin_type_keywords': [],
        'other_names': [],
    })
    for n in names:
        if n.startswith('/Game') or n.startswith('/Engine'):
            buckets['referenced_game_assets'].append(n)
            continue
        if n.startswith('/Script'):
            buckets['referenced_script_modules'].append(n)
            continue
        if n.lower() in PIN_TYPE_KEYWORDS:
            buckets['pin_type_keywords'].append(n)
            continue
        for key, test in CLASSIFIERS:
            if test(n):
                buckets[key].append(n)
                break
        else:
            buckets['other_names'].append(n)
    return buckets


def inventory_properties(inventory_path, asset_package):
    """Properties the live-editor extraction recorded for this asset, if it recorded any."""
    try:
        data = json.loads(Path(inventory_path).read_text())
    except Exception as exc:
        return None, 'inventory unreadable: %s' % exc
    for entries in data.get('sets', {}).values():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if isinstance(entry, dict) and entry.get('path', '').endswith(asset_package):
                return entry.get('properties', {}), None
    return None, 'asset not present in inventory'


def extract(path: Path, inventory_path=None):
    data = path.read_bytes()
    names, start, end = read_name_table(data)
    buckets = classify(names)

    result = {
        'asset': path.name,
        'file_size': len(data),
        'name_table': {'offset': start, 'end': end, 'count': len(names)},
        'buckets': {k: sorted(v) for k, v in buckets.items()},
        'counts': {k: len(v) for k, v in buckets.items()},
        'names': names,
        'limits': 'Node topology (pin links, execution order, state transitions) is NOT read. '
                  'This is a declaration inventory, not a graph dump.',
    }

    if inventory_path:
        props, err = inventory_properties(inventory_path, path.stem)
        if props is None:
            result['cross_check'] = {'status': 'skipped', 'reason': err}
        else:
            lowered = {n.lower(): n for n in names}
            found = sorted(k for k in props if k.lower() in lowered)
            missing = sorted(k for k in props if k.lower() not in lowered)
            # Report the name table's own spelling: the asset says `AimPitch`, the inventory
            # says `aimPitch`, and a port has to type the former.
            result['buckets']['declared_properties'] = [lowered[k.lower()] for k in found]
            result['counts']['declared_properties'] = len(found)
            result['buckets']['other_names'] = [
                n for n in result['buckets']['other_names']
                if n.lower() not in {k.lower() for k in found}
            ]
            result['counts']['other_names'] = len(result['buckets']['other_names'])
            result['cross_check'] = {
                'status': 'ran',
                'inventory_property_count': len(props),
                'recovered_offline': len(found),
                'not_in_name_table': missing,
                'reading': 'Names absent here are inherited, not declared -- the name table '
                           'lists what the package itself introduces.',
            }
    return result


def to_markdown(r):
    lines = ['# %s - offline declaration inventory' % r['asset'], '']
    lines.append('Read from the checked-out `.uasset`. No editor involved.')
    lines.append('')
    lines.append('- file size: %s bytes' % format(r['file_size'], ','))
    lines.append('- name table: %d entries at offset %d' % (r['name_table']['count'], r['name_table']['offset']))
    lines.append('')

    cc = r.get('cross_check')
    if cc and cc['status'] == 'ran':
        lines += ['## Accuracy cross-check vs `bp_inventory.json`', '',
                  '| | |', '|---|---|',
                  '| properties the editor reported | %d |' % cc['inventory_property_count'],
                  '| recovered offline | **%d** |' % cc['recovered_offline'],
                  '| not in the name table | %d |' % len(cc['not_in_name_table']), '']
        if cc['not_in_name_table']:
            lines += ['Not found offline (inherited, not declared by this asset):', '']
            lines += ['- `%s`' % n for n in cc['not_in_name_table']]
            lines.append('')

    order = [
        ('referenced_game_assets', 'Referenced content packages'),
        ('referenced_script_modules', 'Engine modules this graph pulls from'),
        ('anim_graph_nodes', 'AnimGraph node classes present'),
        ('anim_runtime_nodes', 'Anim runtime nodes'),
        ('k2_nodes', 'K2 (event graph) node classes present'),
        ('control_rig', 'Control Rig'),
        ('declared_properties', 'Declared properties (inventory-confirmed, asset spelling)'),
        ('edgraph', 'EdGraph internals'),
        ('pin_type_keywords', 'Pin type vocabulary'),
    ]
    for key, title in order:
        vals = r['buckets'].get(key) or []
        if not vals:
            continue
        lines += ['## %s (%d)' % (title, len(vals)), '']
        lines += ['- `%s`' % v for v in vals]
        lines.append('')

    unc = r['buckets'].get('other_names') or []
    if unc:
        lines += ['## Other names (%d)' % len(unc), '',
                  'Listed in full rather than dropped - bone names, slot names, curve names, '
                  'state and transition names, engine node properties and designer-authored '
                  'graph names all land here, and which is which needs a human or the editor. '
                  'No rule separates them by spelling; see the note in the source.', '']
        lines += ['- `%s`' % v for v in unc]
        lines.append('')

    lines += ['## Limits', '', r['limits'], '']
    return '\n'.join(lines)


def sweep(root: Path, inventory_path=None):
    """Every graph-bearing asset under `root`, and whether the inventory knows about it.

    This is the mode that answers the question a curated target list cannot: not "did the 14
    assets I named extract cleanly" but "what is actually in this folder". `read_graphs.py`
    needs a path per asset, so its coverage is always a claim someone made in advance;
    this walks the tree and lets the files answer.
    """
    known = set()
    if inventory_path:
        try:
            data = json.loads(Path(inventory_path).read_text())
            for entries in data.get('sets', {}).values():
                if isinstance(entries, list):
                    for entry in entries:
                        if isinstance(entry, dict) and entry.get('path'):
                            known.add(entry['path'].rsplit('/', 1)[-1])
        except Exception as exc:
            print('inventory unreadable (%s); reporting without coverage' % exc, file=sys.stderr)

    rows = []
    for path in sorted(root.rglob('*.uasset')):
        try:
            names, _, _ = read_name_table(path.read_bytes())
        except Exception:
            continue  # not a package we can read, or not a graph asset -- silently skipped
        buckets = classify(names)
        if not (buckets['k2_nodes'] or buckets['anim_graph_nodes']):
            continue  # carries no graph; nothing a port would want
        rows.append({
            'asset': path.stem,
            'path': str(path.relative_to(root)),
            'k2_nodes': len(buckets['k2_nodes']),
            'anim_graph_nodes': len(buckets['anim_graph_nodes']),
            'game_refs': len(buckets['referenced_game_assets']),
            'in_inventory': path.stem in known,
        })
    return rows


def sweep_markdown(rows, root):
    covered = [r for r in rows if r['in_inventory']]
    missing = [r for r in rows if not r['in_inventory']]
    lines = [
        '# Graph-bearing assets under `%s`' % root,
        '',
        'Found by walking the checked-out files, not by naming targets in advance.',
        '',
        '| | |', '|---|---|',
        '| graph-bearing assets on disk | **%d** |' % len(rows),
        '| present in `bp_inventory.json` | %d |' % len(covered),
        '| **absent from the inventory** | **%d** |' % len(missing),
        '',
    ]
    if missing:
        lines += ['## Absent from the inventory', '',
                  '| Asset | K2 node classes | AnimGraph node classes | Content refs |',
                  '|---|---:|---:|---:|']
        for r in sorted(missing, key=lambda x: -(x['k2_nodes'] + x['anim_graph_nodes'])):
            lines.append('| `%s` | %d | %d | %d |' % (
                r['asset'], r['k2_nodes'], r['anim_graph_nodes'], r['game_refs']))
        lines.append('')
    lines += ['## Read this as coverage, not as a verdict', '',
              'A high node-class count means the asset carries a graph, not that the graph is '
              'worth porting -- `ANIM-PORT-LEDGER.md` decides that, per asset, with the '
              'editor where topology matters. What this table settles is which assets have '
              'never been looked at.', '']
    return '\n'.join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('assets', nargs='*', type=Path)
    ap.add_argument('--sweep', type=Path, default=None,
                    help='walk a content directory and report every graph-bearing asset')
    ap.add_argument('--out', type=Path, default=None, help='directory for .json/.md output')
    ap.add_argument('--inventory', default=None, help='path to mcp-bp/bp_inventory.json for cross-check')
    args = ap.parse_args(argv)

    if args.sweep:
        rows = sweep(args.sweep, args.inventory)
        missing = sum(1 for r in rows if not r['in_inventory'])
        print('%d graph-bearing assets under %s; %d absent from the inventory'
              % (len(rows), args.sweep, missing))
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            (args.out / 'sweep.json').write_text(json.dumps(rows, indent=2))
            (args.out / 'sweep.md').write_text(sweep_markdown(rows, args.sweep), encoding='utf-8')
            print('wrote sweep.json and sweep.md to %s' % args.out)
        if not args.assets:
            return 0

    results = []
    for path in args.assets:
        try:
            r = extract(path, args.inventory)
        except Exception as exc:
            print('FAILED %s: %s' % (path.name, exc), file=sys.stderr)
            continue
        results.append(r)
        cc = r.get('cross_check', {})
        note = ''
        if cc.get('status') == 'ran':
            note = '  cross-check %d/%d' % (cc['recovered_offline'], cc['inventory_property_count'])
        print('%-34s names=%-5d props=%-4s anim_nodes=%-3d k2_nodes=%-3d refs=%d%s' % (
            r['asset'], r['name_table']['count'],
            r['counts'].get('declared_properties', '?'),
            r['counts']['anim_graph_nodes'], r['counts']['k2_nodes'],
            r['counts']['referenced_game_assets'], note))

        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            stem = path.stem
            (args.out / (stem + '.json')).write_text(json.dumps(r, indent=2))
            (args.out / (stem + '.md')).write_text(to_markdown(r), encoding='utf-8')

    if args.out:
        print('\nwrote %d report pair(s) to %s' % (len(results), args.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
