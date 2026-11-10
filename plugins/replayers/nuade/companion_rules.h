/* SEL_RP_COMPANIONS rules of the nuade replayers (I166, I1242i, I1242f). One definition, read by adapter.c and by the
 * companion-rules unit test (tests/m3/nuade_companion_rules_test.c). TFMX keeps its samples in a second file; so do Jason
 * Page (smp.*), Mark Cooksey Old (mcs.*), Pierre Adane Packer (smp.* or smp.set), PokeyNoise (pn.*.info), Synth Dream
 * (smp.* or smp.set), Scott Johnston SoundPlayer (smp.*) and Dirk Bialluch (smp.*). Names follow each player's own file
 * lookup in third_party/nuade/players. Pierre Adane (pa_module.c read_sample_set) tries "SMP.<name>" for a "PAP.<name>"
 * song and falls back to "SMP.set" when that file is missing, so the prefix form needs the set rule too. */
#ifndef SELECTA_NUADE_COMPANION_RULES_H
#define SELECTA_NUADE_COMPANION_RULES_H

static const char g_companions[] =
    "mdat.*>smpl.*;*.tfx>*.sam;"
    "jpn.*>smp.*;jpnd.*>jpns.*;*.jpn>*.smp;*.jpnd>*.jpns;"
    "mcr.*>mcs.*;"
    "pap.*>smp.*;pap.*>smp.set;*.pap>smp.set;"
    "pn.*>pn.*.info;"
    "sdr.*>smp.*;sdr.*>smp.set;"
    "sjs.*>smp.*;"
    "tpu.*>smp.*";

#endif
