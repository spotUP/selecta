# Internal helper library shared by several plugins (RULES.md rule 3, I228). Not public SDK: only the plugins link it,
# and cmake/sel_check_includes.cmake whitelists this directory next to include/selecta. Include root is plugins/shared,
# so a plugin writes #include "paula/paula_voices.h" or #include "stream/stream_pcm.h".
#   paula/   Paula register bus and voice sink (paula, paula_shadow, paula14 outputs; integer only, runs in the audio task)
#   viz/     header-only pixel helpers and descriptor of the viz plugins (sel_surface clear, rect, line)
#   stream/  header-only helpers of the stream decoders (ID3 tags, PCM resampling for minimp3 and stb_vorbis)
add_library(selecta_plugin_shared STATIC paula/paula_voices.c paula/paula_hw.c)
target_include_directories(selecta_plugin_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_plugin_shared PRIVATE "${PROJECT_SOURCE_DIR}/include")

# I517 phase 2: the pure helpers the core sources and the source plugins (plugins/sources/<id>) both need. One copy
# each, linked by selecta_library and by every plugin that uses them.
#   source/   json_scan, url_path (incl. sel_url_unquote), latin1 (sel_latin1_to_utf8), sync_text.h (chomp, safe path)
#   archive/  zip / lha / lzx / pp20 adapters over the vendored miniz and lhasa (selecta_archive)
add_library(selecta_shared_source STATIC source/json_scan.c source/url_path.c source/latin1.c source/license_name.c source/xml_scan.c)
target_include_directories(selecta_shared_source PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_source PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_library(selecta_shared_archive STATIC archive/archive.c archive/archive_lzx.c archive/archive_pp20.c)
target_include_directories(selecta_shared_archive PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_archive PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(selecta_shared_archive PUBLIC selecta_archive)

# I892c1: RAOP (AirPlay 1) sender, pure part: ALAC escape-frame packer, AES-128-CBC packet encrypt, RSA-OAEP key wrap, SDP
# ANNOUNCE builder. No sockets or OS calls. Crypto from third_party/mbedtls, which src/library/CMakeLists.txt adds off the
# Amiga (SEL_NO_TLS); on the Amiga it is added here, for the RAOP code only (I1004, I895 decides how the plugin ships it).
if(SEL_AMIGA AND NOT TARGET selecta_mbedtls)
  add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/mbedtls ${CMAKE_BINARY_DIR}/third_party/mbedtls)
endif()
# Socket shims (RULES 4a): the POSIX backend on the host, the bsdsocket backend on the Amiga. The Amiga backends hold no
# socket code, they call src/core/amiga/net.c (one socket layer); selecta_library supplies it at link time.
if(SEL_AMIGA)
  set(SEL_RAOP_SOCK raop/raop_sock_amiga.c)
  set(SEL_UPNP_SOCK upnp/upnp_sock_amiga.c)
else()
  set(SEL_RAOP_SOCK raop/raop_sock_posix.c)
  set(SEL_UPNP_SOCK upnp/upnp_sock_posix.c)
endif()
if(TARGET selecta_mbedtls)
  add_library(selecta_shared_raop STATIC raop/raop.c)
  target_include_directories(selecta_shared_raop PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(selecta_shared_raop PUBLIC selecta_mbedtls)
  # I892c2: RTSP session and RTP sender over the socket shim (raop_sock.h).
  add_library(selecta_shared_raop_net STATIC raop/raop_session.c ${SEL_RAOP_SOCK})
  target_include_directories(selecta_shared_raop_net PRIVATE "${PROJECT_SOURCE_DIR}/src/core/amiga")
  target_link_libraries(selecta_shared_raop_net PUBLIC selecta_shared_raop)
endif()

# I894a1: UPnP / DLNA media server discovery (SSDP + device description). All sockets go through upnp/upnp_sock.h. The
# HTTP fetch is src/library/net/http.c, so the user of this library links selecta_library as well.
add_library(selecta_shared_upnp STATIC upnp/upnp.c ${SEL_UPNP_SOCK})
target_include_directories(selecta_shared_upnp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_upnp PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src/library" "${PROJECT_SOURCE_DIR}/src/core/amiga")
target_link_libraries(selecta_shared_upnp PUBLIC selecta_shared_source)

# I892e1: Chromecast CASTV2. cast.c (framing, protobuf, status JSON) is pure and builds everywhere; cast_session.c rides the
# socket table (src/library/net/net.h) and tls.h, so the user links selecta_library as well. On the Amiga the TLS half
# answers SEL_CAST_E_TLS until I899 (sel_tls_open_insecure is the SEL_NO_TLS stub there).
add_library(selecta_shared_mdns STATIC mdns/mdns.c) # I892e1: mDNS browsing; cast discovery now, AirPlay _raop._tcp later
target_include_directories(selecta_shared_mdns PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_mdns PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src/library")
target_link_libraries(selecta_shared_mdns PUBLIC selecta_shared_upnp) # the UDP socket shim lives there (upnp_sock.h)

add_library(selecta_shared_cast STATIC cast/cast.c cast/cast_session.c cast/cast_discover.c cast/cast_http.c)
target_include_directories(selecta_shared_cast PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(selecta_shared_cast PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src/library")
target_link_libraries(selecta_shared_cast PUBLIC selecta_shared_source selecta_shared_mdns)
