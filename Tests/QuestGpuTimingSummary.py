"""Summarize observed GPU groups from a native timing cache, without inventing costs.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import statistics
import struct


def summarize(path):
    with path.open('rb') as stream:
        def value(fmt):
            size = struct.calcsize(fmt)
            data = stream.read(size)
            if len(data) != size:
                raise ValueError('Truncated timing cache')
            return struct.unpack(fmt, data)

        def text():
            size, = value('<I')
            return stream.read(size).decode('utf-8')

        if stream.read(8) != b'CYNTIM01':
            raise ValueError('Unsupported timing cache')
        count, = value('<I')
        names = {}
        for _ in range(count):
            timer, = value('<Q')
            names[timer] = text()
        count, = value('<I')
        tracks = []
        for _ in range(count):
            identity, = value('<Q')
            name = text()
            events, = value('<Q')
            if not identity >> 63:
                stream.seek(events * 32, 1)
                continue
            groups = defaultdict(list)
            unfinished = 0
            for _ in range(events):
                start, end, timer, depth, flags, _ = value('<ddIIII')
                if flags:
                    unfinished += 1
                    continue
                groups[(names[timer], depth)].append((end-start)*1000)
            rows = []
            for (label, depth), durations in sorted(groups.items()):
                durations.sort()
                rows.append(dict(name=label, depth=depth, count=len(durations),
                                 above_2us=sum(t > .002 for t in durations),
                                 minimum_ms=durations[0], median_ms=statistics.median(durations),
                                 p95_ms=durations[int((len(durations)-1)*.95)], maximum_ms=durations[-1]))
            tracks.append(dict(name=name, events=events, unfinished=unfinished, groups=rows))
        return dict(tracks=tracks,
                    note='Observed timestamp intervals only; subgroup boundaries on a tile GPU may have unresolved costs. Hardware stages and Unreal group identity are separate sources.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = summarize(args.cache)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(dict(gpu_tracks=len(report['tracks']),
                         groups=sum(len(t['groups']) for t in report['tracks']),
                         events=sum(t['events'] for t in report['tracks']))))


if __name__ == '__main__':
    main()
