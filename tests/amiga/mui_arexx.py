#!/usr/bin/env python3
"""mui_arexx.py -- sends ARexx commands straight to Selecta's port, for rig-m4-mui-open (MUI-P4-17, I558).

  mui_arexx.py <rig dir> <port> <command> [<command> ...]      one line per command: <rc>|<result>

The rig has no RexxMaster, so `ami.py rexx` (which talks to the REXX port) cannot run a program; wasabi's ARexx call
addresses the named port directly, as amarchy_rig.py does. Exit 0 when every command returned rc 0, else 1."""
import sys

sys.path.insert(0, sys.argv[1])
import ami


def main():
    port, cmds = sys.argv[2], sys.argv[3:]
    bad = 0
    m = ami._m()
    with ami._conn(60) as conn:
        for c in cmds:
            rc, _, res = m.arexx(conn, port, c)
            print('%d|%s' % (rc, res.strip()))
            bad |= 1 if rc else 0
    return bad


if __name__ == '__main__':
    sys.exit(main())
