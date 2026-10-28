/* selecta-status.rexx: print the play state and the title of the running Selecta.
 *
 * Quoting: the whole Selecta command is ONE string handed to ADDRESS SELECTA. A path with spaces is
 * quoted inside it: ADDRESS SELECTA 'OPEN "DH0:Music/my song.mod"'. A " or \ inside the path is
 * written \" and \\ as on the control socket.
 *
 * STATUS answers one key=value pair per line. RC 0 = ok, 10 = the command failed (the error text is
 * in STATUS key last_error=), 20 = Selecta is closing. */
OPTIONS RESULTS
IF ~SHOW('P', 'SELECTA') THEN DO
   SAY 'Selecta is not running (no SELECTA port)'
   EXIT 10
END
ADDRESS SELECTA 'STATUS'
IF RC ~= 0 THEN DO
   SAY 'STATUS failed, RC='RC
   EXIT RC
END
state = ''
title = ''
rest = RESULT
DO WHILE rest ~= ''
   p = POS('0A'x, rest)
   IF p = 0 THEN DO
      line = rest
      rest = ''
   END
   ELSE DO
      line = LEFT(rest, p - 1)
      rest = SUBSTR(rest, p + 1)
   END
   IF LEFT(line, 6) = 'state=' THEN state = SUBSTR(line, 7)
   IF LEFT(line, 6) = 'title=' THEN title = SUBSTR(line, 7)
END
SAY state':' title
EXIT 0
