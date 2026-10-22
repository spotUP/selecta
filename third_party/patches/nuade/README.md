# nuade patches

No patches. `third_party/nuade` is the upstream submodule at
111c9657965f00e9a94fec3545ce238d43af57f4, built unmodified.

Policy (rule 5): a change to vendored code is a last resort. If one becomes
unavoidable, keep it as a numbered `NNNN-short-name.patch` in this directory,
apply it from CMake, and record the reason and the upstream status in the
plan's Deviations section.
