#!/usr/bin/env python3
"""Extract Project Eagle's pedestrian path network from S&S/CDimages/default/paths.img.

This is the file that answers "how do peds walk the pavements of Vice City".
Eagle does ship a complete pedestrian network for its whole expanded world. It
is easy to miss for three separate reasons, and all three had to be unpicked:

  1. It is inside an IMG archive, so it does not show up in a file search. The
     loose data/Paths/NODES*.DAT are the stock 64 and are overridden by this.
  2. The node files are in fastman92 Limit Adjuster's FLA4 format, not stock,
     so a stock parser rejects them outright.
  3. Positions are still int16 divided by 8, which wraps every 8192 units, and
     Eagle's world is 48000 across. The tile index says which wrap applies.

Result: 4096 tiles, 508 of them populated, 288,609 nodes of which 147,175 are
vehicle and 141,434 pedestrian. 73.5% of the pedestrian nodes lie outside San
Andreas. The vehicle count matches pecore_gps_graph.pegg exactly, which is a
useful independent check that this is being read correctly.

FORMAT
------
IMG is standard VER2: 'VER2', uint32 count, then 32-byte directory entries of
uint32 offset (2048-byte sectors), uint16 streaming size (sectors), uint16 size
in archive, char[24] name.

Each NODES entry:

    0   int32   -1                      marker
    4   char[4] "FLA4"                  fastman92 extended paths
    8   int32   numNodes
    12  int32   numVehicleNodes
    16  int32   numPedNodes             numNodes == vehicle + ped
    20  int32   numCarPathLinks
    24  int32   numAddresses
    28          nodes, 44 bytes each    (stock is 28)

Record sizes were solved by fitting predicted file length against 40 real tiles;
node 44, car path link 24, node address 4, navi link 4, link length 1,
intersection 1, all fitting every tile exactly.

Node, 44 bytes. Only the fields below are established:

    8   int16[3]  x, y, z, each divided by 8, wrapping at +/-4096
    22  uint8     path width, divided by 16

Vehicle nodes come first and pedestrian nodes are the tail, as in stock.

WORLD MAPPING
-------------
The grid is 64x64 tiles of 750 units, matching the limit adjuster's reported
"Number of path tiles per dimension: 64" and "Paths map size 48000". A tile's
cell origin is

    x = (tileIndex % 64  - 32) * 750
    y = (tileIndex // 64 - 32) * 750

which puts the world at -24000..23250. Since a stored coordinate wraps every
8192 units, the true value is the one congruent to it that falls inside the
tile's own cell. Every one of the 141,434 pedestrian nodes lands inside its own
cell under this rule, which is what makes it trustworthy.

Usage:
  python3 deploy/extract-eagle-ped-paths.py <paths.img> [-o out.json]
"""

import argparse
import json
import os
import struct
import sys

SECTOR = 2048
NODE_SIZE = 44
HEADER_SIZE = 28
TILES_PER_AXIS = 64
CELL = 750.0
WRAP = 8192.0
POS_SCALE = 8.0
WIDTH_SCALE = 16.0


def read_img_entries(blob):
    if blob[:4] != b'VER2':
        raise ValueError('not a VER2 IMG archive')
    count = struct.unpack_from('<I', blob, 4)[0]
    for i in range(count):
        at = 8 + i * 32
        offset, streaming, in_archive = struct.unpack_from('<IHH', blob, at)
        name = blob[at + 8:at + 32].split(b'\0')[0].decode('latin1')
        size = (streaming or in_archive) * SECTOR
        yield name, offset * SECTOR, size


def unwrap(value, low, high):
    """Pick the representative of `value` (mod 8192) inside this tile's cell."""
    return value + round(((low + high) / 2.0 - value) / WRAP) * WRAP


def parse_tile(blob, tile_index):
    if len(blob) < HEADER_SIZE or blob[4:8] != b'FLA4':
        return None
    num, vehicle, ped, _, _ = struct.unpack_from('<5i', blob, 8)
    if num <= 0 or num != vehicle + ped:
        return None

    x0 = (tile_index % TILES_PER_AXIS - 32) * CELL
    y0 = (tile_index // TILES_PER_AXIS - 32) * CELL

    nodes = []
    for i in range(vehicle, num):
        base = HEADER_SIZE + i * NODE_SIZE
        xi, yi, zi = struct.unpack_from('<3h', blob, base + 8)
        nodes.append({
            'x': round(unwrap(xi / POS_SCALE, x0, x0 + CELL), 3),
            'y': round(unwrap(yi / POS_SCALE, y0, y0 + CELL), 3),
            'z': round(zi / POS_SCALE, 3),
            'w': round(blob[base + 22] / WIDTH_SCALE, 3),
            't': tile_index,
        })
    return nodes, num, vehicle, ped


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('archive', help='path to S&S/CDimages/default/paths.img')
    parser.add_argument('-o', '--out', help='write the pedestrian nodes as JSON')
    args = parser.parse_args()

    if not os.path.exists(args.archive):
        sys.exit('no such file: %s' % args.archive)
    blob = open(args.archive, 'rb').read()

    ped_nodes = []
    tiles = populated = total = vehicle_total = ped_total = 0
    for name, offset, size in read_img_entries(blob):
        if not name.lower().startswith('nodes'):
            continue
        tiles += 1
        digits = ''.join(c for c in name if c.isdigit())
        if not digits:
            continue
        parsed = parse_tile(blob[offset:offset + size], int(digits))
        if not parsed:
            continue
        nodes, num, vehicle, ped = parsed
        ped_nodes.extend(nodes)
        populated += 1
        total += num
        vehicle_total += vehicle
        ped_total += ped

    if not ped_nodes:
        sys.exit('no pedestrian nodes found; the archive may not be Eagle\'s paths.img')

    xs = [n['x'] for n in ped_nodes]
    ys = [n['y'] for n in ped_nodes]
    zs = [n['z'] for n in ped_nodes]
    beyond = sum(1 for n in ped_nodes if abs(n['x']) > 3000 or abs(n['y']) > 3000)
    widths = sorted(n['w'] for n in ped_nodes if n['w'] > 0)

    print('NODES tiles        %d' % tiles)
    print('  populated        %d' % populated)
    print('nodes total        %d' % total)
    print('  vehicle          %d' % vehicle_total)
    print('  pedestrian       %d' % ped_total)
    print('extent             x %.0f..%.0f  y %.0f..%.0f  z %.0f..%.0f'
          % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    print('beyond San Andreas %d  (%.1f%%)' % (beyond, 100.0 * beyond / len(ped_nodes)))
    if widths:
        print('path width         median %.2f  max %.2f'
              % (widths[len(widths) // 2], widths[-1]))

    if args.out:
        with open(args.out, 'w') as handle:
            json.dump({'source': os.path.abspath(args.archive),
                       'count': len(ped_nodes), 'nodes': ped_nodes}, handle)
        print('wrote              %s (%.1f MB)'
              % (args.out, os.path.getsize(args.out) / 1048576.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
