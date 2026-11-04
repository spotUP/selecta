# Selecta plugin SDK

A Selecta plugin is a shared module that exports one symbol and returns a tag-list descriptor. The SDK is the headers in
`include/selecta/` (MIT) plus the CMake package `SelectaSDK`. A plugin includes only those headers and libc. It never links
`selecta_core`: the host process owns the one shared core, and the module reaches it only through the `sel_host` table.

## Entry point (include/selecta/plugin.h)

```c
const sel_tag* sel_plugin_entry(uint32_t host_api);
```

The function returns a list ended by `SEL_TAG_END` holding `SEL_PLUGIN_KIND`, `SEL_PLUGIN_ID`, `SEL_PLUGIN_VERSION` and
`SEL_PLUGIN_TAGS`. `SEL_PLUGIN_TAGS` points to the kind-specific hook list. Return `NULL` when `host_api` is too old for the
module. Kinds are the `SEL_PLUGIN_KIND_*` values in plugin.h: replayer 1, output 2, DSP 3, viz 4, UI 5, converter 6,
source 7, integration 8, scripting 9.

Only `sel_plugin_entry` is exported from the module (see building.md).

## Tags (include/selecta/tags.h, tags.def)

A tag is `{uint32_t tag; uintptr_t value;}`. Lists end with `SEL_TAG_END`; `SEL_TAG_MORE` chains another list. A plugin
declares only the tags it implements. The host skips unknown tags and asks for a capability by tag, never by offset. Tags are
never renumbered; a changed meaning gets a new tag. Private tags set the `SEL_TAG_USER` bit (tags.h) and never appear in
tags.def. Function pointers go into a tag value with `SEL_TAG_FN` (plugin.h, as used by examples/plugin-sine/sine.c).

## Host table (include/selecta/host.h, api.h)

The host passes a `const sel_host*` to hooks. It is versioned by `size`: check `host->size` before touching a field added
after your minimum version. `host->api_version` is `SEL_API_VERSION` (api.h, currently 2).

| Field | Use |
|-------|-----|
| `mem_alloc(host, size, kind)`, `mem_free` | All memory. Sample memory goes through the host, never `malloc` after start. |
| `read` | Read from a host file handle. |
| `log(host, level, msg)` | Diagnostics. |
| `clock_ms` | Millisecond clock. |
| `voices` | The voice table of a VOICE replayer (voices.h). |
| `config_get(host, key, def)` | Configuration string. |
| `query(host, ask, answer)` | Non-hot host queries as tag lists. |
| `net`, `catalogue` | Network and library accessors; NULL means unavailable (host.h). |

## Versioning rules

1. `sel_host` fields are appended, never reordered or removed (host.h).
2. Tags are never renumbered and never change meaning (tags.def).
3. `SEL_API_VERSION` rises when the host gains something a plugin may require. A module compares `host_api` in its entry.
4. A plugin that needs a newer host field checks `host->size` first and degrades or returns `NULL` from the entry.
5. Public headers are C99 and compile as C++ (`extern "C"`).

## Documents

- replayer.md: the replayer contract, with examples/plugin-sine as the worked case.
- building.md: building a module, the search path, licensing.
