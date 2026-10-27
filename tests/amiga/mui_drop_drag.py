#!/usr/bin/env python3
"""mui_drop_drag.py -- Workbench icon drag for rig-m4-mui-drop (MUI-P5-6).

  mui_drop_drag.py <rig dir> open <drawer window title prefix>
  mui_drop_drag.py <rig dir> drag <drawer window title prefix> <Selecta window title>

`open` double-clicks the first Workbench icon (Ram Disk) and then the first icon in that window (the drawer), so the
drawer window with the song icon stays open on the Workbench screen. `drag` sends the Selecta window behind it with its
depth gadget (ami.py gclick), drags the first icon of the drawer window onto the Selecta window (the mouse script
pattern of winch_rig.py drag()), then brings the Selecta window forward again. Icon positions come from the pixels (the
window list has no icons): the centre of the first blob of non-background pixels inside the window. Exit 0 on success."""
import os, shlex, subprocess, sys, time
from collections import Counter

RIG = sys.argv[1]
sys.path.insert(0, RIG)
import ami

BORDER = (4, 16, 24, 12)  # left, title bar, right, bottom: the window frame the scan skips


def windows():
    out = []
    for line in ami.req(0x0D).decode('latin-1').splitlines():
        if line.startswith('W '):
            f = shlex.split(line)
            w, h = map(int, f[4].split('x'))
            out.append((f[-1], (int(f[2]), int(f[3]), w, h)))
    return out


def find(prefix):
    for title, box in windows():
        if title.startswith(prefix):
            return box
    sys.exit('no window titled %r*: %s' % (prefix, [t for t, _ in windows()]))


def icons(box):
    """Centres of the icons in a window: blobs of non-background rows separated by more than 4 clear rows."""
    x, y, w, h = box
    x0, y0, x1, y1 = x + BORDER[0], y + BORDER[1], x + w - BORDER[2], y + h - BORDER[3]
    gw, gh, rows = ami.grab((x0, y0, x1 - x0, y1 - y0))
    c = Counter()
    for r in rows:
        for i in range(0, len(r), 3):
            c[r[i:i + 3]] += 1
    bg = c.most_common(1)[0][0]
    ink = [[i for i in range(gw) if r[i * 3:i * 3 + 3] != bg] for r in rows]
    out, start, last = [], None, None
    for j, xs in enumerate(ink + [None]):
        if xs:
            start = j if start is None else start
            last = j
        elif start is not None and (xs is None or j - last > 4):
            cols = [i for jj in range(start, last + 1) for i in ink[jj]]
            out.append((x0 + (min(cols) + max(cols)) // 2, y0 + (start + last) // 2))
            start = None
    if not out:
        sys.exit('no icon pixels in window box %s' % (box,))
    return out


def first_icon(box):
    return icons(box)[0]


def mouse(events):
    kx, ky = ami.pointer_scale()
    ami.script(*[('move', int(e[1] * kx), int(e[2] * ky)) if e[0] == 'move' else e for e in events])


def dclick(pt):
    mouse([('move',) + pt, ('wait', 2), ('button', 0, 1), ('wait', 2), ('button', 0, 0), ('wait', 3),
           ('button', 0, 1), ('wait', 2), ('button', 0, 0), ('wait', 10)])


def drag(frm, to):
    x, y = frm
    dx, dy = to[0] - x, to[1] - y
    ev = [('move', x, y), ('wait', 2), ('button', 0, 1), ('wait', 3)]
    for i in range(1, 9):
        ev += [('move', x + dx * i // 8, y + dy * i // 8), ('wait', 2)]
    mouse(ev + [('wait', 3), ('button', 0, 0), ('wait', 10)])


def depth(title):
    subprocess.run([sys.executable, os.path.join(RIG, 'ami.py'), 'gclick', title, 'sys:depth'], check=True, env=os.environ)


if sys.argv[2] == 'open':
    for pt in icons(find('Workbench')):  # the Ram Disk is one of the disk icons: try each until its window opens
        dclick(pt)
        time.sleep(2)
        if any(t.startswith('Ram Disk') for t, _ in windows()):
            break
    dclick(first_icon(find('Ram Disk')))
    time.sleep(2)
    find(sys.argv[3])
elif sys.argv[2] == 'drag':
    depth(sys.argv[4])
    time.sleep(1)
    dbox = find(sys.argv[3])
    sbox = find(sys.argv[4])
    drag(first_icon(dbox), (min(sbox[0] + sbox[2] - 100, dbox[0] + dbox[2] + 150), dbox[1] + dbox[3] + 150))
    time.sleep(2)
    depth(sys.argv[4])
else:
    sys.exit('mode?')
