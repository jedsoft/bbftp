#!/bin/sh

TESTDIR=`dirname $0`
cd $TESTDIR
LOGFILE=bbftpd.log-$$
echo $@ > $LOGFILE
BBFTPD="$TESTDIR/../bbftp-server/bbftpd/bbftpd"

MEMCHECK="valgrind --tool=memcheck --leak-check=yes --error-limit=no --num-callers=25"
$MEMCHECK $BBFTPD $* 2> bbftpd.err-$$

