# Vendored mbedTLS (plan 2026-10-25-m3-library.md, D-M3-4)

- Version: 3.6.7 (3.6 LTS), release tarball `mbedtls-3.6.7.tar.bz2` from
  `https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2`
- SHA-256 of the tarball: `a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`
- Files kept: `library/*.c` and `*.h`, `include/mbedtls/`, `include/psa/`, `LICENSE` (Apache-2.0 OR GPL-2.0-or-later). `3rdparty/` (Everest, p256-m) is not used.
- The upstream files are unmodified. Our configuration is `selecta_mbedtls_config.h` (upstream default minus server, DTLS, net, timing, PSA file storage); build rules in `CMakeLists.txt` (target `selecta_mbedtls`).
- Desktop only. The Amiga build is plain HTTP and does not compile this directory.
- A fix goes upstream or into `third_party/patches/` (RULES.md rule 5).
