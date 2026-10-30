# T68 http-client: plain HTTP parts (row I153) and TLS parts (row I154).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures> -DCA_FILE=<test CA pem>
# Every request goes to the fixture server through the net guard and host map.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fixture.test=http://127.0.0.1:${port}")
set(host_dir "${FIXDIR}/fixture.test")
file(SHA256 "${host_dir}/data.bin" want_sha)
set(out_dir "${WORK}/t68")
file(MAKE_DIRECTORY "${out_dir}")

# fetch(<var prefix> <out file or ""> <args...>): runs the CLI, sets <prefix>_<counter>.
function(fetch prefix out)
  set(extra)
  if(out)
    file(REMOVE "${out_dir}/${out}")
    set(extra --out "${out_dir}/${out}")
  endif()
  execute_process(COMMAND "${CLI}" fetch ${ARGN} ${extra}
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  set(${prefix}_raw "${o}${e}" PARENT_SCOPE)
  set(${prefix}_rc "${rc}" PARENT_SCOPE)
  foreach(k http_status http_error http_requests redirects body_bytes content_length elapsed_ms tls_handshakes)
    if(o MATCHES "(^|\n)${k}=([^\n]*)")
      set(${prefix}_${k} "${CMAKE_MATCH_2}" PARENT_SCOPE)
    else()
      set(${prefix}_${k} "" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

function(expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "T68 ${what}: got '${got}', want '${want}'")
  endif()
endfunction()

function(expect_body what out)
  file(SHA256 "${out_dir}/${out}" got)
  expect("${what} body sha256" "${got}" "${want_sha}")
endfunction()

# 1. Plain GET: status 200, whole body, one request.
fetch(plain plain.bin http://fixture.test/plain)
expect("plain status" "${plain_http_status}" 200)
expect("plain requests" "${plain_http_requests}" 1)
expect("plain length" "${plain_body_bytes}" 20000)
expect("plain tls" "${plain_tls_handshakes}" 0)
expect_body("plain" plain.bin)

# 2. Chunked transfer decodes to the same bytes.
fetch(chunked chunked.bin http://fixture.test/chunked)
expect("chunked status" "${chunked_http_status}" 200)
expect("chunked content_length" "${chunked_content_length}" -1)
expect_body("chunked" chunked.bin)

# 3. Range bytes=0-1023 answers 206 with exactly 1024 bytes equal to the file prefix.
fetch(range range.bin http://fixture.test/plain --range 0-1023)
expect("range status" "${range_http_status}" 206)
expect("range length" "${range_body_bytes}" 1024)
file(READ "${host_dir}/data.bin" whole HEX LIMIT 1024)
file(READ "${out_dir}/range.bin" part HEX)
expect("range prefix" "${part}" "${whole}")

# 4. A 3-hop 301 chain (relative, absolute, relative) ends at the file.
fetch(hop hop.bin http://fixture.test/hop1)
expect("hop status" "${hop_http_status}" 200)
expect("hop redirects" "${hop_redirects}" 3)
expect("hop requests" "${hop_http_requests}" 4)
expect_body("hop" hop.bin)

# 5. At most 5 redirects: r1 needs 6.
fetch(many "" http://fixture.test/r1)
expect("too many redirects error" "${many_http_error}" too_many_redirects)
expect("too many redirects count" "${many_redirects}" 5)

# 6. Write verbs reach the server with the right method and body.
fetch(put "" http://fixture.test/put --method PUT --data "${host_dir}/put_body.txt")
expect("put status" "${put_http_status}" 201)
fetch(post "" http://fixture.test/post --method POST --data "${host_dir}/put_body.txt")
expect("post status" "${post_http_status}" 202)
fetch(del "" http://fixture.test/delete --method DELETE)
expect("delete status" "${del_http_status}" 204)

# 7. First byte timeout: 3 s +- 0.3 s against a 6 s first-byte delay.
fetch(slow "" http://fixture.test/slow_first)
expect("first byte error" "${slow_http_error}" first_byte_timeout)
if(slow_elapsed_ms LESS 2700 OR slow_elapsed_ms GREATER 3300)
  message(FATAL_ERROR "T68 first byte timeout fired at ${slow_elapsed_ms} ms, want 3000 +- 300")
endif()

# 8. Total timeout: 30 s +- 1 s against a body that stalls 35 s after the first half.
fetch(stall "" http://fixture.test/slow_body)
expect("total timeout error" "${stall_http_error}" total_timeout)
if(stall_elapsed_ms LESS 29000 OR stall_elapsed_ms GREATER 31000)
  message(FATAL_ERROR "T68 total timeout fired at ${stall_elapsed_ms} ms, want 30000 +- 1000")
endif()

# 8b. A caller User-Agent replaces the default (I725p).
fetch(ua "" http://fixture.test/uapath --user-agent "TestAgent/9 (+mailto:x@example.test)")

# 9. The fixture's request log shows what the client sent (a stale cache would not).
fetch(log log.json http://fixture.test/__fixture/log)
file(READ "${out_dir}/log.json" logjson)
string(JSON n LENGTH "${logjson}")
set(seen "")
math(EXPR last "${n} - 1")
foreach(i RANGE ${last})
  string(JSON m GET "${logjson}" ${i} method)
  string(JSON p GET "${logjson}" ${i} path)
  string(JSON len GET "${logjson}" ${i} body_len)
  string(APPEND seen "${m} ${p} ${len};")
  if(m STREQUAL "PUT")
    string(JSON bsha GET "${logjson}" ${i} body_sha256)
    file(SHA256 "${host_dir}/put_body.txt" want_put)
    expect("PUT body hash" "${bsha}" "${want_put}")
  endif()
  if(p STREQUAL "/uapath")
    string(JSON ua2 GET "${logjson}" ${i} headers user-agent)
    expect("caller user agent" "${ua2}" "TestAgent/9 (+mailto:x@example.test)")
  endif()
  if(p STREQUAL "/plain" AND len EQUAL 0)
    string(JSON ua GET "${logjson}" ${i} headers user-agent)
    if(NOT ua MATCHES "^Selecta/[0-9]+\\.[0-9]+\\.[0-9]+ \\(\\+https://")
      message(FATAL_ERROR "T68 default user agent: got '${ua}'")
    endif()
  endif()
endforeach()
foreach(want "PUT /put 21" "POST /post 21" "DELETE /delete 0" "GET /hop3 0" "GET /r6 0")
  if(NOT seen MATCHES "${want};")
    message(FATAL_ERROR "T68 request log lacks '${want}': ${seen}")
  endif()
endforeach()

# 10. TLS (I154): the fixture CA via SELECTA_CA_FILE verifies and one handshake is counted.
if(EXISTS "${WORK}/tls_port" AND CA_FILE)
  file(READ "${WORK}/tls_port" tls_port)
  string(STRIP "${tls_port}" tls_port)
  # The host map sends "localhost" straight to the fixture's IPv4 address (I215). Without it the client
  # resolves localhost to ::1 first, and any stranger listening on ::1 with the same port number answers
  # instead of the fixture (handshake fails). The name stays localhost, so SNI, Host and certificate
  # verification are unchanged.
  set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
  set(ENV{SELECTA_NET_MAP} "localhost=http://127.0.0.1:${tls_port}")
  set(ENV{SELECTA_CA_FILE} "${CA_FILE}")
  fetch(tls tls.bin https://localhost:${tls_port}/plain)
  expect("tls error" "${tls_http_error}" ok)
  expect("tls status" "${tls_http_status}" 200)
  expect("tls handshakes" "${tls_tls_handshakes}" 1)
  expect("tls length" "${tls_body_bytes}" 20000)
  expect_body("tls" tls.bin)
  # An unknown CA (the OS store does not hold the test CA) fails verification, no body.
  unset(ENV{SELECTA_CA_FILE})
  fetch(badca badca.bin https://localhost:${tls_port}/plain)
  expect("unknown CA error" "${badca_http_error}" tls_verify_failed)
  expect("unknown CA body" "${badca_body_bytes}" 0)
else()
  message(FATAL_ERROR "T68 TLS parts need the TLS fixture listener and the test CA (openssl at configure time)")
endif()
