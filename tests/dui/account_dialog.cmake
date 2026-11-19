# I1469 dui-account-dialog (design doc 9.2, D-M3-12): the top bar avatar opens the account dialog (Log In form, logged
# out: no name), Tab moves between the fields, Escape closes it without a request.
include("${CMAKE_CURRENT_LIST_DIR}/dui_run.cmake")
set(args --ui desktop --backend mem --canvas 1280x800 --clock fixed --stats)

file(WRITE "${WORK}/acct-0.txt" "wait 1\n")
dui_run(o ${args} --input "${WORK}/acct-0.txt")
if(NOT o MATCHES "account user= open=0 busy=0 clicks=0 avatar=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+)")
  message(FATAL_ERROR "I1469 no logged-out account line\n${o}")
endif()
if(CMAKE_MATCH_3 LESS 8)
  message(FATAL_ERROR "I1469 no avatar in the top bar at 1280x800\n${o}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

file(WRITE "${WORK}/acct-1.txt" "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\nkey tab\nwait 1\n")
dui_run(o ${args} --input "${WORK}/acct-1.txt")
if(NOT o MATCHES "account user= open=1 busy=0 clicks=1 " OR NOT o MATCHES "dialog quits=[0-9]+ open=7 ")
  message(FATAL_ERROR "I1469 the avatar click did not open the account dialog\n${o}")
endif()

file(WRITE "${WORK}/acct-2.txt" "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\nkey escape\nwait 1\n")
dui_run(o ${args} --input "${WORK}/acct-2.txt")
if(NOT o MATCHES "account user= open=0 busy=0 clicks=1 " OR NOT o MATCHES "dialog quits=[0-9]+ open=0 ")
  message(FATAL_ERROR "I1469 Escape did not close the account dialog\n${o}")
endif()
message(STATUS "dui_account_dialog_ok")
