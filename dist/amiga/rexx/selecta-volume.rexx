/* selecta-volume.rexx <n>: set the volume of the running Selecta to n (0 to 100).
 * RC 0 = ok, 10 = bad argument or the command failed (last_error= in STATUS), 20 = Selecta is closing. */
PARSE ARG n .
IF n = '' | ~DATATYPE(n, 'W') THEN DO
   SAY 'usage: rx selecta-volume.rexx <0-100>'
   EXIT 10
END
IF n < 0 | n > 100 THEN DO
   SAY 'usage: rx selecta-volume.rexx <0-100>'
   EXIT 10
END
IF ~SHOW('P', 'SELECTA') THEN DO
   SAY 'Selecta is not running (no SELECTA port)'
   EXIT 10
END
ADDRESS SELECTA 'VOLUME' n
IF RC ~= 0 THEN SAY 'VOLUME failed, RC='RC
EXIT RC
