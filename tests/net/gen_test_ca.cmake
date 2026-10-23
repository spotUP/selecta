# Generates the test CA and a localhost server certificate with the openssl CLI
# at configure time into CA_DIR (ca.pem is what SELECTA_CA_FILE points at).
find_program(OPENSSL openssl)
if(NOT OPENSSL)
  message(STATUS "net fixtures: openssl not found, TLS listener disabled")
  return()
endif()
if(EXISTS "${CA_DIR}/server.pem")
  return()
endif()
file(MAKE_DIRECTORY "${CA_DIR}")
file(WRITE "${CA_DIR}/san.cnf" "subjectAltName=DNS:localhost,IP:127.0.0.1\n")
foreach(step
  "req;-x509;-newkey;rsa:2048;-nodes;-days;3650;-subj;/CN=Selecta Test CA;-keyout;${CA_DIR}/ca.key;-out;${CA_DIR}/ca.pem"
  "req;-newkey;rsa:2048;-nodes;-subj;/CN=localhost;-keyout;${CA_DIR}/server.key;-out;${CA_DIR}/server.csr"
  "x509;-req;-in;${CA_DIR}/server.csr;-CA;${CA_DIR}/ca.pem;-CAkey;${CA_DIR}/ca.key;-CAcreateserial;-days;3650;-extfile;${CA_DIR}/san.cnf;-out;${CA_DIR}/server.pem")
  execute_process(COMMAND "${OPENSSL}" ${step} RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "openssl ${step} failed")
  endif()
endforeach()
