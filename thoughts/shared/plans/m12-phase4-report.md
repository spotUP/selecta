# M1.2 Phase 4 report (I74, I75)

Design sections: architecture design doc 3 and 7. Plan: 2026-10-23-m1-2-playlists-views-control.md Phase 4.

## Status
- T32 view-tree: red first (`Test #24: view-tree ... ***Exception: SegFault`), green after b680b06.
- T24 selecta-tree-view and selecta-drill-view (screen fixtures): green.
- T8 ratchet: 43 -> 39 (VIEW_GROUP, VIEW_EXPAND, VIEW_COLLAPSE, VIEW_DRILL now real). The plan's 27 needs the other phases.
- Whole suite `ctest -E coreaudio`: 44 of 44 passed.

## Behaviour
- Tree and drilldown apply to the queue source only; browse stays a flat directory list.
- Group keys: folder (dirname of queue path), format (lowercase extension). Drilldown on format has two columns (format, then folder), then songs.
- Node ids are sequential from 1 in order of first appearance (not hashed). Table holds 512 groups; the rest fold into a "(more)" group.
- VIEW_GROUP on a regular view switches it to tree. VIEW_DRILL value `..` clears that level and deeper. VIEW_CHANGED carries the style name; group key and drill values are in the snapshot.
- Text UI keys: v cycles style, g toggles folder/format, Backspace in a drilldown steps back, Enter toggles a tree node or drills into a node.
- The bottom help line was left unchanged to keep the T19 golden; v and g are not listed there (the line is already truncated at 80 columns).
- view-unit and view-tree share a song directory: added a RESOURCE_LOCK so `ctest -j2` does not race.
- The tree fixtures use a fixed root `/tmp/selecta-tree-view` so folder names fit the screen width.
