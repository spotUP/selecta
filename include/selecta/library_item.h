/* One row kind of the Your Library list (design doc 2026-10-29-desktop-library-architecture.md section 2, I678a). Public so the
 * library store, the engine and the UI model agree on the numbers without including each other. A kind mask has bit
 * (1u << kind) set for every kind wanted. */
#ifndef SELECTA_LIBRARY_ITEM_H
#define SELECTA_LIBRARY_ITEM_H

enum {
    SEL_LIB_ITEM_LIKED = 0,  /* pinned: Liked Songs */
    SEL_LIB_ITEM_RECENT = 1, /* pinned: Recently Played */
    SEL_LIB_ITEM_LOCAL = 2,  /* pinned: Local Files */
    SEL_LIB_ITEM_PLAYLIST = 3,
    SEL_LIB_ITEM_ALBUM = 4,
    SEL_LIB_ITEM_ARTIST = 5,
    SEL_LIB_ITEM_COMPOSER = 6,
    SEL_LIB_ITEM_SOURCE = 7,
    SEL_LIB_ITEM_KINDS = 8
};
#define SEL_LIB_ITEM_BIT(kind) (1u << (kind))
#define SEL_LIB_ITEM_ALL ((1u << SEL_LIB_ITEM_KINDS) - 1u)
#define SEL_LIB_ITEM_PINNED_MASK \
    (SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LIKED) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_RECENT) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_LOCAL))

/* The lists of a collection page (design doc 2026-10-29-desktop-library-architecture.md section 8), shared by the library and the engine. */
enum {
    SEL_COLL_SONGS = 0, /* the songs in page order: song key, name, artist, album, plays (an album in scan order, an artist or composer by plays; Liked Songs newest like first, Recently Played newest play first with the last play time in the number column, Local Files and a source in scan order) */
    SEL_COLL_ALBUMS,    /* artist page "Albums": album id, name, artist, year, songs (newest first) */
    SEL_COLL_APPEARS,   /* artist page "Appears On": albums of other artists that hold a song by the artist */
    SEL_COLL_MORE,      /* album page "More by": the other albums of the album's artist */
    SEL_COLL_SPELLINGS, /* composer page "Spellings": display, "", "", 0, songs written under that spelling */
    SEL_COLL_HEAD       /* album page header: the album's own row (id, name, artist, year, songs) */
};

/* One row of the list. `key` is the stable id a click routes on (playlist id, album id, artist name, composer canon, source id,
 * or the pinned key). `sub` is raw subtitle data (album artist); the display text is built by sel_uim_library_item_subtitle. */
typedef struct sel_library_item {
    unsigned kind;
    char key[96];
    char name[96];
    char sub[96];
    unsigned songs;
    unsigned added;
    unsigned last_played;
    unsigned opened; /* I678c: epoch seconds the item was last opened (opened table), 0 never */
    unsigned art_id;
    int pinned;
} sel_library_item;

#endif
