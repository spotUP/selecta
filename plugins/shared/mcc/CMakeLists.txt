# I685b spike: a real MUI external class library (UP-Spike.mcc) and a program that opens it by name. Added from
# plugins/shared/CMakeLists.txt only inside if(SEL_AMIGA), so host builds never see this directory.
# Skeleton: libnix libinit.o (Resident + RTF_AUTOINIT + Open/Close/Expunge/Reserved) + sys-include/stabs.h, no startup code.
set(MCC_LIBINIT_O "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/lib/libinit.o")
set_source_files_properties("${MCC_LIBINIT_O}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
# libinit.o first: the Resident must be the first thing in the hunk, with its -1 returning code ahead of it
add_executable(up_spike_mcc "${MCC_LIBINIT_O}" lib/libheader.c lib/spike_class.c)
set_target_properties(up_spike_mcc PROPERTIES OUTPUT_NAME "UP-Spike.mcc")
target_include_directories(up_spike_mcc PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include")
target_link_options(up_spike_mcc PRIVATE -nostartfiles)

add_executable(mcc_spike demo/spike.c)
set_target_properties(mcc_spike PROPERTIES OUTPUT_NAME "mcc-spike")
target_link_libraries(mcc_spike PRIVATE mui)
# stabs.h emits the function table with asm() at file scope; -std=c99 hides the asm keyword, so the one file that uses it gets -fasm
set_source_files_properties(lib/libheader.c PROPERTIES COMPILE_OPTIONS "-fasm")
