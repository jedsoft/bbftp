#!/bin/sh

if test -z "$SSH_AGENT_PID"
then
  echo 2>&1 "ssh-agent not started"
  exit 1
fi

MEMCHECK="valgrind --tool=memcheck --leak-check=yes --error-limit=no --num-callers=25"
TESTDIR=`pwd`

# BBFTPD="$TESTDIR/../bbftp-server/bbftpd/bbftpd"

BBFTPD="$TESTDIR/runserver.sh -l DEBUG"
BBFTP="$TESTDIR/../bbftp-client/bbftpc/bbftp"

NUM_RETRY=1
N=0
for X in "dir /tmp" "dir /foobar" "cd /tmp" "cd /foobar" \
  "get $TESTDIR/test_dir.sh get$N.dat" \
  "lcd /tmp" "lcd /foobar"
do
  $MEMCHECK $BBFTP -e "$X" -r $NUM_RETRY -m -E "$BBFTPD" -I ~/.ssh/id_rsa -u $LOGNAME localhost 2>test_$N.err > test_$N.log
  N=`expr $N + 1`
done
