#!/usr/bin/env python3
"""mui_list_dclick.py -- raw mouse double-click on a row of the Selecta MUI window's List, for rig-m4-mui-open (I724).

  mui_list_dclick.py <rig dir> <window title> <row label> [<expected row index>]

Unlike `ami.py ui dclick <label>` (wasabid finds the gadget and clicks its centre) this reads the List box from the row gadgets, works out the
click point from the row geometry (row height from two neighbouring rows, centre of the wanted row) and sends the two presses with
ami._mouse(conn, 1, 0, 2, x, y). Prints `dclick <index> at <x>,<y>`; exits 2 with a message when the row is not found, not in view, or
not at the expected index (the state pin)."""
import sys
import time

sys.path.insert(0, __file__.rsplit('/', 1)[0])
from mui_list_rows import selecta_windows  # noqa: E402


def main():
    rig, title, label = sys.argv[1:4]
    want_idx = int(sys.argv[4]) if len(sys.argv) > 4 else None
    sys.path.insert(0, rig)
    import ami
    with ami._conn(60) as conn:
        m = ami._m()
        for _sc, win in selecta_windows(m.read_ui(conn, every_screen=True), title):
            rows = [g for g in win['gadgets'] if g['kind'] == 'row' and g['height']]
            for g in rows:
                if g['label'].split()[:1] == [label]:
                    idx = int(g['value'])
                    if want_idx is not None and idx != want_idx:
                        print('row %r is at index %d, expected %d' % (label, idx, want_idx))
                        return 2
                    x, y = g['x'] + g['width'] // 2, g['y'] + g['height'] // 2
                    ami._mouse(conn, 1, 0, 1, x, y)
                    time.sleep(0.3)
                    ami._mouse(conn, 1, 0, 2, x, y)
                    print('dclick %d at %d,%d (row box %dx%d)' % (idx, x, y, g['width'], g['height']))
                    return 0
        print('row %r not in view' % label)
        return 2


if __name__ == '__main__':
    sys.exit(main())
