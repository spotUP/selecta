/* I433a: the AT3 (.aks) importer parses the official Arkos Tracker 3 example "Lop Ears" into the song model. The
 * expected numbers were read from the song's XML with an independent oracle (python xml.etree, counting only the
 * direct children of <instruments> and of the subsong <tracks>) and match the song as Arkos shows it: title
 * "Lop ears", author Targhan, composer Andy Severn, one subsong "Main" at speed 6 and 50 Hz, a YM-less AY at
 * 1 MHz, 16 positions over 14 patterns. Red proofs: a corrupted header and a wrong format version fail with a
 * message, never a crash. Usage: at3_song_test <fixture.aks>. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "at3_song.h"

static int bad;

static void check(const char *what, long got, long want)
{
  if (got != want) {
    printf("FAIL %s: got %ld want %ld\n", what, got, want);
    bad++;
  }
}

static void check_str(const char *what, const char *got, const char *want)
{
  if (strcmp(got, want) != 0) {
    printf("FAIL %s: got \"%s\" want \"%s\"\n", what, got, want);
    bad++;
  }
}

static void expect_fail(const char *what, const void *data, size_t size)
{
  at3_song *s = NULL;
  char err[128] = "";
  int rc = at3_song_load(data, size, &s, err, sizeof err);
  if (rc == 0 || s != NULL || err[0] == 0) {
    printf("FAIL %s: loaded or no message (rc %d)\n", what, rc);
    bad++;
    at3_song_free(s);
  } else {
    printf("rejected %s: %s\n", what, err);
  }
}

int main(int argc, char **argv)
{
  FILE *f;
  unsigned char *buf;
  long size;
  at3_song *s = NULL;
  char err[128] = "";
  const at3_subsong *ss;
  size_t i, played = 0;

  if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
    printf("FAIL cannot open fixture\n");
    return 1;
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = (unsigned char *)malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
    printf("FAIL read fixture\n");
    return 1;
  }
  fclose(f);

  if (at3_song_load(buf, (size_t)size, &s, err, sizeof err) != 0) {
    printf("FAIL load: %s\n", err);
    return 1;
  }
  check_str("title", s->title, "Lop ears");
  check_str("author", s->author, "Targhan");
  check_str("composer", s->composer, "Andy Severn");
  check("instruments", (long)s->n_instruments, 7);
  check("arpeggios", (long)s->n_arpeggios, 3);
  check("pitches", (long)s->n_pitches, 2);
  check("subsongs", (long)s->n_subsongs, 1);
  ss = &s->subsongs[0];
  check_str("subsong title", ss->title, "Main");
  check("initial speed", ss->initial_speed, 6);
  check("replay milli-Hz", ss->replay_milli_hz, 50000);
  check("psgs", (long)ss->n_psgs, 1);
  if (ss->n_psgs == 1) {
    check("psg clock", ss->psgs[0].frequency_hz, 1000000);
    check("psg reference milli-Hz", ss->psgs[0].reference_milli_hz, 440000);
    check("psg sample player", ss->psgs[0].sample_player_hz, 11025);
    check_str("psg mixing", ss->psgs[0].mixing, "ABC");
    check("psg is ay", ss->psgs[0].type_ym, 0);
  }
  check("tracks", (long)ss->n_tracks, 38);
  check("positions", (long)ss->n_positions, 16);
  check("patterns", (long)ss->n_patterns, 14);
  for (i = 0; i < ss->n_patterns; i++) played += ss->patterns[i].n_channels;
  check("every pattern has 3 channels", (long)played, 14 * 3);
  /* the model keeps the content, not only the counts: some instrument has PSG cells, some track has cells */
  {
    size_t cells = 0, tcells = 0;
    for (i = 0; i < s->n_instruments; i++) cells += s->instruments[i].n_cells;
    for (i = 0; i < ss->n_tracks; i++) tcells += ss->tracks[i].n_cells;
    if (cells == 0 || tcells == 0) {
      printf("FAIL model empty: instrument cells %lu track cells %lu\n", (unsigned long)cells, (unsigned long)tcells);
      bad++;
    }
  }
  at3_song_free(s);

  /* red proofs */
  {
    unsigned char *c = (unsigned char *)malloc((size_t)size);
    memcpy(c, buf, (size_t)size);
    c[0] = 'X'; /* corrupt the zip header: no longer a zip, not XML either */
    expect_fail("corrupt header", c, (size_t)size);
    memcpy(c, buf, (size_t)size);
    memset(c + size - 22, 0, 22); /* wipe the end-of-central-directory record */
    expect_fail("no central directory", c, (size_t)size);
    expect_fail("truncated", buf, (size_t)size / 2);
    expect_fail("too short", buf, 4);
    free(c);
  }
  {
    static const char v[] = "<song><formatVersion>9.9</formatVersion><subsongs/></song>";
    static const char n[] = "<song><formatVersion>3.0</formatVersion></song>";
    expect_fail("wrong version", v, strlen(v));
    expect_fail("no subsongs", n, strlen(n));
  }
  free(buf);

  if (bad) return 1;
  printf("at3_song_ok\n");
  return 0;
}
