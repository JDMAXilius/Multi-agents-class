"""
abp_offline_extract.py - read a Blueprint / AnimBlueprint `.uasset` with no editor running.

    python animations/abp_offline_extract.py <asset.uasset> [...] [--inventory J] [--out DIR]
    python animations/abp_offline_extract.py --sweep Content/FPSTemplate [--inventory J] [--out DIR]

WHY THIS EXISTS
---------------
`mcp-bp/read_graphs.py` reads graphs through the editor's MCP server: a live editor, a loaded
project, a human at the keyboard, and a path supplied per asset. It works. Its one structural
weakness is recorded in `docs/ANIM-PORT-LEDGER.md`, which paid for it: a curated target list is
a claim about what matters, and every conclusion drawn from it inherits that claim silently.
"14/14 found" read as completeness and only ever meant "14/14 of what I was told to look at."

This reader runs on the checked-out files. It needs no target list, so "what did we never look
at" stops being a question the tool can get wrong. It complements `mcp-bp/`; it does not
replace it, for the reason under LIMITS.

HOW IT READS
------------
An uncooked `.uasset` carries a name table: every distinct FName the package uses. For a
Blueprint that includes every node class, every declared property, every referenced package,
and every function and graph name. This parses that table and classifies it.

The table is located by structural probe rather than by reading the summary's NameOffset. That
is deliberate: this project's packages come from a source engine build whose
`FPackageFileSummary` does not match the documented layout -- legacy file version -9, and a
20-byte `SavedHash` where the package GUID used to be -- so a version-driven walk of the
summary desynchronises and reads garbage. Twelve consecutive well-formed length-prefixed
ASCII names is a signature random bytes do not produce, and it does not care which build wrote
the file.

ACCURACY IS MEASURED, NOT ASSERTED
----------------------------------
Pass `--inventory mcp-bp/bp_inventory.json` and every run cross-checks itself against the
live-editor extraction already committed for the same asset, and prints the delta. On
`ABP_Mannequin_Base` it recovers 88 of 96; all 8 misses are stock `UAnimInstance` members
(`onMontage*` x6, `rootMotionMode`, `bUseMainInstanceMontageEvaluationData`) which are absent
from the name table precisely because the Blueprint inherits rather than declares them. Read
that as the accuracy statement: it sees what an asset DECLARES, and inherited members are the
editor's to report.

LIMITS -- READ BEFORE QUOTING ANY NUMBER FROM THIS
--------------------------------------------------
**No topology.** Pin links, execution order, state transitions, blend weights: not read, and
not guessed. That data lives in the export table's tagged property streams, and export walks
against these packages were attempted and rejected by their own validator rather than reported
at low confidence.

So this answers *what is in a graph*, never *how it is wired*. A verdict that needs topology
needs the editor, and should say so.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

PKG_TAG = 0x9E2A83C1

# How far into the file to look for the name table before giving up.
NAME_TABLE_SEARCH_LIMIT = 8192

# Editor packages store two case-preserving hashes after each name entry.
NAME_HASH_BYTES = 4

# A run this long of well-formed names is the table; shorter runs occur by chance.
PROBE_RUN = 12


# --------------------------------------------------------------------------- name table

def read_name_table(data: bytes):
    """Return (names, start_offset, end_offset), or raise if no table validates."""

    def run(start, limit=None):
        pos, out = start, []
        while limit is None or len(out) < limit:
            if pos + 4 > len(data):
                break
            length = struct.unpack_from('<i', data, pos)[0]
            if not 0 < length < 300:
                break
            chunk = data[pos + 4:pos + 4 + length]
            if chunk[-1:] != b'\x00' or not all(32 <= c < 127 for c in chunk[:-1]):
                break
            out.append(chunk[:-1].decode('ascii'))
            pos += 4 + length + NAME_HASH_BYTES
        return out, pos

    if len(data) < 4 or struct.unpack_from('<I', data, 0)[0] != PKG_TAG:
        raise ValueError('not an Unreal package (bad tag)')

    for start in range(min(NAME_TABLE_SEARCH_LIMIT, len(data))):
        if len(run(start, limit=PROBE_RUN)[0]) == PROBE_RUN:
            names, end = run(start)
            return names, start, end
    raise ValueError('no name table found')


# --------------------------------------------------------------------------- classification

CLASSIFIERS = [
    ('anim_graph_nodes', lambda n: n.startswith('AnimGraphNode')),
    ('anim_runtime_nodes', lambda n: n.startswith('AnimNode_') or n.startswith('FAnimNode_')),
    ('k2_nodes', lambda n: n.startswith('K2Node')),
    ('control_rig', lambda n: n.startswith('RigVM') or n.startswith('ControlRig')),
    ('edgraph', lambda n: n.startswith('EdGraph')),
]

# EdGraph pin categories and wildcards: a fixed engine vocabulary, filtered out so they stop
# reading as "names this asset introduces".
PIN_TYPE_KEYWORDS = {
    'bool', 'byte', 'class', 'delegate', 'double', 'else', 'exec', 'execute', 'field', 'float',
    'int', 'int64', 'interface', 'name', 'object', 'real', 'self', 'softclass', 'softobject',
    'string', 'struct', 'text', 'then', 'tooltip', 'wildcard',
}

# WHY THERE IS NO "DECLARED PROPERTY" REGEX HERE.
#
# The obvious rule -- a declared property is lowerCamel -- was written, measured, and thrown
# away. It scored 8 correct out of 116 on ABP_Mannequin_Base. Blueprint properties are stored
# UpperCamel in the name table (`AimPitch`); it is the MCP inventory that lowercases the first
# letter (`aimPitch`). So the rule matched engine node internals (`bAllowConduitEntryStates`,
# `bMeshSpaceRotationBlend`) and skeleton bone names instead, and would have published 116
# confident findings of which 108 were noise.
#
# Nothing separates a Blueprint-declared `AimPitch` from an engine node's own property by
# spelling: both are UpperCamel, both are in the table. Properties are therefore reported ONLY
# where the inventory confirms them, and everything else is listed under `other_names` for a
# human to read. A run without `--inventory` says "I cannot tell" instead of guessing, which is
# the difference between this being evidence and being an opinion.


def classify(names):
    buckets = {key: [] for key, _ in CLASSIFIERS}
    buckets.update({
        'referenced_game_assets': [],
        'referenced_script_modules': [],
        'pin_type_keywords': [],
        'other_names': [],
    })
    for name in names:
        if name.startswith('/Game') or name.startswith('/Engine'):
            buckets['referenced_game_assets'].append(name)
        elif name.startswith('/Script'):
            buckets['referenced_script_modules'].append(name)
        elif name.lower() in PIN_TYPE_KEYWORDS:
            buckets['pin_type_keywords'].append(name)
        else:
            for key, test in CLASSIFIERS:
                if test(name):
                    buckets[key].append(name)
                    break
            else:
                buckets['other_names'].append(name)
    return buckets


# --------------------------------------------------------------------------- cross-check

def inventory_properties(inventory_path, asset_stem):
    """What the live-editor extraction recorded for this asset, if it recorded anything."""
    try:
        data = json.loads(Path(inventory_path).read_text())
    except Exception as exc:
        return None, 'inventory unreadable: %s' % exc
    for entries in data.get('sets', {}).values():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if isinstance(entry, dict) and entry.get('path', '').endswith(asset_stem):
                return entry.get('properties', {}), None
    return None, 'asset not present in inventory'


# --------------------------------------------------------------------------- per-asset

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
            by_lower = {n.lower(): n for n in names}
            found = sorted(k for k in props if k.lower() in by_lower)
            missing = sorted(k for k in props if k.lower() not in by_lower)
            confirmed_lower = {k.lower() for k in found}

            # Report the asset's own spelling: it says `AimPitch`, the inventory says
            # `aimPitch`, and a port has to type the former.
            result['buckets']['declared_properties'] = [by_lower[k] for k in confirmed_lower]
            result['counts']['declared_properties'] = len(found)
            result['buckets']['other_names'] = [
                n for n in result['buckets']['other_names'] if n.lower() not in confirmed_lower
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


SECTION_ORDER = [
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


def to_markdown(r):
    lines = ['# %s - offline declaration inventory' % r['asset'], '',
             'Read from the checked-out `.uasset`. No editor involved.', '',
             '- file size: %s bytes' % format(r['file_size'], ','),
             '- name table: %d entries at offset %d'
             % (r['name_table']['count'], r['name_table']['offset']), '']

    cc = r.get('cross_check')
    if cc and cc['status'] == 'ran':
        lines += ['## Accuracy cross-check vs `bp_inventory.json`', '', '| | |', '|---|---:|',
                  '| properties the editor reported | %d |' % cc['inventory_property_count'],
                  '| recovered offline | **%d** |' % cc['recovered_offline'],
                  '| not in the name table | %d |' % len(cc['not_in_name_table']), '']
        if cc['not_in_name_table']:
            lines += ['Not found offline - inherited, not declared by this asset:', '']
            lines += ['- `%s`' % n for n in cc['not_in_name_table']] + ['']
    elif cc:
        lines += ['## Accuracy cross-check', '',
                  'Not run: %s. Declared properties are not reported without it - see the '
                  'note in `abp_offline_extract.py`.' % cc['reason'], '']

    for key, title in SECTION_ORDER:
        vals = r['buckets'].get(key) or []
        if vals:
            lines += ['## %s (%d)' % (title, len(vals)), ''] + ['- `%s`' % v for v in sorted(vals)] + ['']

    other = r['buckets'].get('other_names') or []
    if other:
        lines += ['## Other names (%d)' % len(other), '',
                  'Listed in full rather than dropped. Bone names, slot names, curve names, '
                  'state and transition names, engine node properties and designer-authored '
                  'graph names all land here, and which is which needs a human or the editor. '
                  'No rule separates them by spelling; see the note in the source.', '']
        lines += ['- `%s`' % v for v in sorted(other)] + ['']

    return '\n'.join(lines + ['## Limits', '', r['limits'], ''])


# --------------------------------------------------------------------------- sweep

def sweep(root: Path, inventory_path=None):
    """Every graph-bearing asset under `root`, and whether the inventory knows about it.

    This is the mode a curated target list cannot provide: not "did the assets I named
    extract cleanly" but "what is actually in this folder".
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
            continue  # not a package this can read; skipped rather than counted
        buckets = classify(names)
        if not (buckets['k2_nodes'] or buckets['anim_graph_nodes']):
            continue  # carries no graph, so nothing a port would want
        rows.append({
            'asset': path.stem,
            'path': str(path.relative_to(root)).replace('\\', '/'),
            'k2_nodes': len(buckets['k2_nodes']),
            'anim_graph_nodes': len(buckets['anim_graph_nodes']),
            'game_refs': len(buckets['referenced_game_assets']),
            'in_inventory': path.stem in known,
        })
    return rows


def sweep_markdown(rows, root):
    missing = [r for r in rows if not r['in_inventory']]
    lines = ['# Graph-bearing assets under `%s`' % root, '',
             'Found by walking the checked-out files, not by naming targets in advance.', '',
             '| | |', '|---|---:|',
             '| graph-bearing assets on disk | **%d** |' % len(rows),
             '| present in `bp_inventory.json` | %d |' % (len(rows) - len(missing)),
             '| **absent from the inventory** | **%d** |' % len(missing), '']

    if missing:
        lines += ['## Absent from the inventory', '',
                  '| Asset | K2 node classes | AnimGraph node classes | Content refs |',
                  '|---|---:|---:|---:|']
        for r in sorted(missing, key=lambda x: -(x['k2_nodes'] + x['anim_graph_nodes'])):
            lines.append('| `%s` | %d | %d | %d |'
                         % (r['asset'], r['k2_nodes'], r['anim_graph_nodes'], r['game_refs']))
        lines.append('')

    return '\n'.join(lines + [
        '## Read this as coverage, not as a verdict', '',
        'A high node-class count means an asset carries a graph, not that the graph is worth '
        'porting -- `docs/ANIM-PORT-LEDGER.md` decides that, per asset, with the editor where '
        'topology matters. What this table settles is which assets have never been looked at.',
        ''])


# --------------------------------------------------------------------------- cli

def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('assets', nargs='*', type=Path, help='.uasset files to inventory')
    parser.add_argument('--sweep', type=Path, default=None,
                        help='walk a content directory and report every graph-bearing asset')
    parser.add_argument('--inventory', default=None,
                        help='mcp-bp/bp_inventory.json, for the accuracy cross-check')
    parser.add_argument('--out', type=Path, default=None, help='directory for .json/.md output')
    args = parser.parse_args(argv)

    if not args.assets and not args.sweep:
        parser.error('give at least one .uasset, or --sweep DIR')

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

    for path in args.assets:
        try:
            r = extract(path, args.inventory)
        except Exception as exc:
            print('FAILED %s: %s' % (path.name, exc), file=sys.stderr)
            continue

        cc = r.get('cross_check', {})
        note = ('  cross-check %d/%d' % (cc['recovered_offline'], cc['inventory_property_count'])
                if cc.get('status') == 'ran' else '')
        print('%-34s names=%-5d props=%-4s anim_nodes=%-3d k2_nodes=%-3d refs=%d%s'
              % (r['asset'], r['name_table']['count'], r['counts'].get('declared_properties', '?'),
                 r['counts']['anim_graph_nodes'], r['counts']['k2_nodes'],
                 r['counts']['referenced_game_assets'], note))

        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            (args.out / (path.stem + '.json')).write_text(json.dumps(r, indent=2))
            (args.out / (path.stem + '.md')).write_text(to_markdown(r), encoding='utf-8')

    return 0


if __name__ == '__main__':
    sys.exit(main())
