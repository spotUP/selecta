#!/usr/bin/env python3
"""mui_style_pick.py -- picks a menu item of the Selecta MUI window by path, for rig-m4-mui-styles (MUI-P3-8, I571).

  mui_style_pick.py <rig dir> pick <menu path> [window title]     e.g. pick View/Style/Grid Selecta
  mui_style_pick.py <rig dir> list <window title>                 the menu paths of the window, one per line

`pick` hands the window the message Intuition sends for the item (wasabi ui_menu), so it works whatever the menu looks like.
Exit 0 on success, 1 when the item is missing or ghosted."""
import sys

sys.path.insert(0, sys.argv[1])
import ami


def main():
    verb = sys.argv[2]
    if verb == 'pick':
        with ami._conn(60) as conn:
            ami._m().ui_menu(conn, sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else None)
        return 0
    if verb == 'list':
        for line in ami.req(0x0F, sys.argv[3].encode('latin-1')).decode('latin-1').splitlines():
            print(line)
        return 0
    return 1


if __name__ == '__main__':
    sys.exit(main())
