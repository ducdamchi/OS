************* REPLICATING UNIX SHELL *************

Last modified: Jan 2024
Author: Duc Dam

***** Replicated Functionality *****
- cd (with and without path)
- fork
- pipe (max 1)
- I/O redirection

***** Test cases *****
Suggested command to test durability of shell.
Note: '<' for stdin, '1>' for stdout, '2>' for stderr
(Copy and paste into shell)

ls 
ls -l 
ls -l | sort
ls -l 2> error.txt | sort &
ls -l 1> out.txt 2> error.txt
grep -i void < shell.c | sort
./standard_out_error 1> out.txt 2> err.txt
./standard_out_error 2> err.txt | grep 5
