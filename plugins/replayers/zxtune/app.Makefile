# SPDX-License-Identifier: GPL-3.0-or-later
binary_name := selecta-zxtune
dirs.root := ../..
include $(dirs.root)/make/default.mak

source_files := stub.cpp

libraries.common = analysis async \
                  binary binary_compression binary_format \
                  core core_plugins_players core_plugins_archives \
                  debug devices_aym devices_aym_dumper devices_beeper devices_dac devices_fm devices_saa \
                  formats_archived formats_archived_multitrack formats_chiptune formats_packed formats_multitrack \
                  io \
                  l10n_stub \
                  module module_conversion module_properties \
                  parameters platform platform_version \
                  sound strings \
                  tools
libraries.3rdparty = hvl lazyusf2 v2m lhasa lzma zlib

include $(dirs.root)/makefile.mak
