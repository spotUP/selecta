#!/usr/bin/env python3
"""mui_list_rows.py -- prints the rows of the Selecta MUI window's List, for rig-m4-mui-list (MUI-P3-2..P3-4, I558).

  mui_list_rows.py <rig dir> [window title]      one line per row: <index>|<selected 0/1>|<label>
  mui_list_rows.py --selftest                    host check of the window choice (I736)

The labels are the entry text the List holds (wasabi reads each entry pointer as a string), so tree markers and indentation show.
Exit 0; prints nothing when the window has no List.

I736: Selecta opens full screen by default (D-M4-3) and then its window has NO title; the screen carries the name "Selecta". The
window is therefore found by its own title or, when it has none, by the title of its screen. Matching the window title alone found no
window, so the rows of the main list never came back."""
import sys


def selecta_windows(screens, title):
    """(screen, window) pairs of the Selecta main window: a window whose title contains TITLE, or an untitled window on a screen whose
    title contains it. Case-insensitive, like wasabi's own title match."""
    want = title.lower()
    out = []
    for sc in screens:
        for win in sc['windows']:
            wt = win['title'].lower()
            if (want in wt) if wt.strip() else (want in sc['title'].lower()):
                out.append((sc, win))
    return out


def selftest():
    screens = [
        {'title': 'Workbench', 'windows': [{'title': 'RAM Disk'}]},
        {'title': 'Selecta', 'windows': [{'title': ''}, {'title': 'Selecta Help'}]},
    ]
    got = [(s['title'], w['title']) for s, w in selecta_windows(screens, 'Selecta')]
    assert got == [('Selecta', ''), ('Selecta', 'Selecta Help')], got
    assert selecta_windows([{'title': 'Workbench', 'windows': [{'title': ''}]}], 'Selecta') == []
    print('[OK] mui_list_rows window choice')
    return 0


def main():
    title = sys.argv[2] if len(sys.argv) > 2 else 'Selecta'
    sys.path.insert(0, sys.argv[1])
    import ami
    with ami._conn(60) as conn:
        m = ami._m()
        for _screen, win in selecta_windows(m.read_ui(conn, every_screen=True), title):
            for g in win['gadgets']:
                if g['kind'] == 'row':
                    print('%s|%d|%s' % (g['value'], 1 if g['selected'] else 0, g['label']))
    return 0


if __name__ == '__main__':
    sys.exit(selftest() if sys.argv[1:] == ['--selftest'] else main())
