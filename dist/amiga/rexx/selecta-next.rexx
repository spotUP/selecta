/* selecta-next.rexx: skip to the next song in the queue of the running Selecta.
 * RC 0 = ok, 10 = the command failed (last_error= in STATUS), 20 = Selecta is closing. */
IF ~SHOW('P', 'SELECTA') THEN DO
   SAY 'Selecta is not running (no SELECTA port)'
   EXIT 10
END
ADDRESS SELECTA 'NEXT'
IF RC ~= 0 THEN SAY 'NEXT failed, RC='RC
EXIT RC
