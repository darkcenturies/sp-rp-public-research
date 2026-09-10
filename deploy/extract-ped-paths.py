#!/usr/bin/env python3
"""Extract San Andreas' pedestrian path node graph from NODES*.DAT.

San Andreas does not work out routes at runtime. It ships a precomputed node
graph and walks it, which is why its pedestrians handle stairs, alleys and large
obstacles perfectly. SP-RP has been raycasting terrain with ColAndreas and
deriving routes on the fly, which is a different mechanism with different
failure modes. This reads the game's own graph so the server can walk it too.

Format, from reference/gta-reversed/source/game_sa/PathFind.cpp
(CPathFind::LoadPathFindData) and PathFind.h:

  header   5 x int32   numNodes, numVehNodes, numPedNodes,
                       numCarPathLinks, numAddresses
  nodes    numNodes    x 28 bytes  (CPathNode, VALIDATE_SIZE 0x1C)
  carlinks numCarPathLinks x 14    (CCarPathLink, 0xE)
  links    (numAddresses + 16*12) x 4   (CNodeAddress, 0x4)
  navi     numAddresses x 2        (CCarPathLinkAddress, 0x2)
  lengths  (numAddresses + 16*12) x 1
  isects   (numAddresses + 16*12) x 1

Vehicle nodes come first; the pedestrian nodes are the tail of the array
(PathFind.cpp:1241). Positions are int16 divided by 8, path width is a byte
divided by 16.

IMPORTANT: this reads stock San Andreas only. Every node in NODES*.DAT lies
inside +/-3000, and Project Eagle ships those 64 files byte-identical to stock.
Eagle navigates its expanded cities a different way: fastman92 Limit Adjuster
sets "Paths map size = 48000", and PECore ships its own 28 MB graph at
data/pecore_gps_graph.pegg. So this extractor covers San Andreas and nothing
beyond it. See docs/parity/GTA_PARITY_WORKBOOK.md.

Usage:
  python3 deploy/extract-ped-paths.py <paths-dir> [-o out.json]
"""

import argparse
import json
import os
import struct
import sys

NODE_SIZE = 28
CAR_PATH_LINK_SIZE = 14
NODE_ADDRESS_SIZE = 4
CAR_PATH_LINK_ADDRESS_SIZE = 2
DYNAMIC_LINK_SLACK = 16 * 12  # NUM_DYNAMIC_LINKS_PER_AREA * 12
POS_SCALE = 8.0
WIDTH_SCALE = 16.0


class Node:
    __slots__ = ('area', 'index', 'x', 'y', 'z', 'width', 'base_link',
                 'num_links', 'dead_end', 'switched_off', 'water',
                 'dont_wander', 'behaviour', 'links')

    def as_row(self):
        return [self.area, self.index,
                round(self.x, 3), round(self.y, 3), round(self.z, 3),
                round(self.width, 3), self.behaviour,
                1 if self.dont_wander else 0,
                sorted(self.links)]


def parse_area(path, area_hint):
    with open(path, 'rb') as handle:
        blob = handle.read()

    if len(blob) < 20:
        raise ValueError('%s is too small to hold a header' % path)

    num_nodes, num_veh, num_ped, num_car_links, num_addresses = struct.unpack_from('<5i', blob, 0)
    if num_nodes != num_veh + num_ped:
        raise ValueError('%s: node counts disagree (%d != %d + %d)'
                         % (path, num_nodes, num_veh, num_ped))

    offset = 20
    nodes_end = offset + num_nodes * NODE_SIZE
    car_links_end = nodes_end + num_car_links * CAR_PATH_LINK_SIZE
    link_count = num_addresses + DYNAMIC_LINK_SLACK
    links_end = car_links_end + link_count * NODE_ADDRESS_SIZE
    navi_end = links_end + num_addresses * CAR_PATH_LINK_ADDRESS_SIZE
    lengths_end = navi_end + link_count

    if lengths_end > len(blob):
        raise ValueError('%s: file is shorter than its header describes' % path)

    # Link table: each entry is an area/node address pair.
    links = []
    for i in range(link_count):
        base = car_links_end + i * NODE_ADDRESS_SIZE
        links.append(struct.unpack_from('<HH', blob, base))
    lengths = blob[navi_end:lengths_end]

    # Pedestrian nodes are the tail of the node array.
    result = []
    for i in range(num_veh, num_nodes):
        base = offset + i * NODE_SIZE
        # 8 bytes of m_next/m_prev are stale pointers on disk; skip them.
        x, y, z = struct.unpack_from('<3h', blob, base + 8)
        base_link, area_id, node_id = struct.unpack_from('<hHH', blob, base + 16)
        width = blob[base + 22]
        flags = struct.unpack_from('<I', blob, base + 24)[0]

        node = Node()
        node.area = area_id if area_id < 64 else area_hint
        node.index = node_id
        node.x = x / POS_SCALE
        node.y = y / POS_SCALE
        node.z = z / POS_SCALE
        node.width = width / WIDTH_SCALE
        node.base_link = base_link
        node.num_links = flags & 0xF
        node.dead_end = bool((flags >> 4) & 1)
        node.switched_off = bool((flags >> 5) & 1)
        node.water = bool((flags >> 7) & 1)
        node.dont_wander = bool((flags >> 10) & 1)
        node.behaviour = (flags >> 20) & 0xF

        node.links = []
        for slot in range(node.num_links):
            index = base_link + slot
            if 0 <= index < len(links):
                target_area, target_node = links[index]
                length = lengths[index] if index < len(lengths) else 0
                node.links.append((target_area, target_node, length))
        result.append(node)

    return result, num_nodes, num_veh, num_ped


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('paths_dir', help='the game data/Paths directory')
    parser.add_argument('-o', '--out', help='write the graph as JSON to this file')
    parser.add_argument('--quiet', action='store_true')
    args = parser.parse_args()

    files = sorted(f for f in os.listdir(args.paths_dir)
                   if f.upper().startswith('NODES') and f.upper().endswith('.DAT'))
    if not files:
        sys.exit('no NODES*.DAT in %s' % args.paths_dir)

    all_nodes = []
    totals = dict(nodes=0, vehicle=0, ped=0, areas=0, failed=0)
    for name in files:
        area_hint = int(''.join(c for c in name if c.isdigit()) or -1)
        try:
            nodes, n, v, p = parse_area(os.path.join(args.paths_dir, name), area_hint)
        except ValueError as error:
            totals['failed'] += 1
            if not args.quiet:
                print('  skipped %s: %s' % (name, error))
            continue
        all_nodes.extend(nodes)
        totals['nodes'] += n
        totals['vehicle'] += v
        totals['ped'] += p
        totals['areas'] += 1

    linked = sum(1 for n in all_nodes if n.links)
    wander = sum(1 for n in all_nodes if not n.dont_wander)
    if not args.quiet:
        print('areas parsed      %d of %d' % (totals['areas'], len(files)))
        print('nodes total       %d' % totals['nodes'])
        print('  vehicle         %d' % totals['vehicle'])
        print('  pedestrian      %d' % totals['ped'])
        print('ped nodes kept    %d' % len(all_nodes))
        print('  with links      %d' % linked)
        print('  wanderable      %d' % wander)
        if all_nodes:
            xs = [n.x for n in all_nodes]
            ys = [n.y for n in all_nodes]
            zs = [n.z for n in all_nodes]
            print('extent            x %.1f..%.1f  y %.1f..%.1f  z %.1f..%.1f'
                  % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
            widths = sorted(n.width for n in all_nodes)
            print('path width        min %.2f  median %.2f  max %.2f'
                  % (widths[0], widths[len(widths) // 2], widths[-1]))

    if args.out:
        payload = {
            'source': os.path.abspath(args.paths_dir),
            'format': 'area,index,x,y,z,width,behaviour,dont_wander,links[[area,node,length]]',
            'count': len(all_nodes),
            'nodes': [n.as_row() for n in all_nodes],
        }
        with open(args.out, 'w') as handle:
            json.dump(payload, handle)
        if not args.quiet:
            print('wrote             %s (%.1f MB)'
                  % (args.out, os.path.getsize(args.out) / 1048576.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
