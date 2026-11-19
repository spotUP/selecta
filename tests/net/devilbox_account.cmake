# I1469 devilbox-account (design doc 9.2, D-M3-12): selecta-cli devilbox signup / login / logout against the fixture
# server. The server's own error text comes back (409 taken, 400 short password, 401 wrong password), a good signup or
# login stores the token and the name, the password is never written, and logout removes both files.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=${fx}")
set(cfg "${WORK}/i1469-cfg")
file(REMOVE_RECURSE "${cfg}")
file(MAKE_DIRECTORY "${cfg}")

function(acct outvar password)
  file(WRITE "${WORK}/i1469-pw" "${password}\n")
  execute_process(COMMAND "${CLI}" devilbox ${ARGN} --password-stdin --config-dir "${cfg}"
    INPUT_FILE "${WORK}/i1469-pw" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  file(REMOVE "${WORK}/i1469-pw")
  set(${outvar} "${out}${err}" PARENT_SCOPE)
endfunction()

function(expect out pattern what)
  if(NOT out MATCHES "${pattern}")
    message(FATAL_ERROR "I1469 ${what}: want ${pattern}\n${out}")
  endif()
endfunction()

acct(o "fixture-password" signup taken)
expect("${o}" "devilbox_signup=failed status=409\ndevilbox_error=Username already taken" "taken name")
acct(o "abc" signup newuser)
expect("${o}" "status=400\ndevilbox_error=Password must be at least 6 characters" "short password")
if(EXISTS "${cfg}/devilbox.token")
  message(FATAL_ERROR "I1469 a failed signup left a token")
endif()

acct(o "fixture-password" signup newuser)
expect("${o}" "devilbox_signup=ok status=201" "signup")
file(READ "${cfg}/devilbox.token" tok)
file(READ "${cfg}/devilbox.user" who)
expect("${tok}" "^fixture-jwt-signup-newuser$" "signup token file")
expect("${who}" "^newuser$" "user file")

acct(o "wrong-password" login newuser)
expect("${o}" "devilbox_login=failed status=401\ndevilbox_error=Invalid username or password" "wrong password")
acct(o "fixture-password" login newuser)
expect("${o}" "devilbox_login=ok status=200" "login")
file(READ "${cfg}/devilbox.token" tok)
expect("${tok}" "^fixture-jwt-newuser$" "login token file")

file(GLOB files "${cfg}/*")
foreach(f ${files})
  file(READ "${f}" body)
  if(body MATCHES "fixture-password|wrong-password")
    message(FATAL_ERROR "I1469 password written to ${f}")
  endif()
endforeach()

execute_process(COMMAND "${CLI}" devilbox logout --config-dir "${cfg}" OUTPUT_VARIABLE o ERROR_VARIABLE e)
expect("${o}" "devilbox_logout=ok" "logout")
if(EXISTS "${cfg}/devilbox.token" OR EXISTS "${cfg}/devilbox.user")
  message(FATAL_ERROR "I1469 logout left the token or the name")
endif()
message(STATUS "devilbox_account_ok")
