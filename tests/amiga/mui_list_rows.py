#!/usr/bin/env python3
"""mui_list_rows.py -- prints the rows of the Selecta MUI window's List, for rig-m4-mui-list (MUI-P3-2..P3-4, I558).

  mui_list_rows.py <rig dir> [window title]      one line per row: <index>|<selected 0/1>|<label>

The labels are the entry text the List holds (wasabi reads each entry pointer as a string), so tree markers and indentation show.
Exit 0; prints nothing when the window has no List."""
import sys

sys.path.insert(0, sys.argv[1])
import ami


def main():
    title = sys.argv[2] if len(sys.argv) > 2 else 'Selecta'
    with ami._conn(60) as conn:
        m = ami._m()
        for _screen, win in m.ui_windows(m.read_ui(conn, every_screen=True), title):
            for g in win['gadgets']:
                if g['kind'] == 'row':
                    print('%s|%d|%s' % (g['value'], 1 if g['selected'] else 0, g['label']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
