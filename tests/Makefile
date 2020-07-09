BIGFILE=test_bigfile.dat
BIGFILE_SIZE=3000000019  #Prime number
#BIGFILE_SIZE=0
CONTROL_FILE = testcmds.bb
#
all:
check:
	./runtests.sl -c $(CONTROL_FILE) -H localhost -s $(BIGFILE_SIZE) -b $(BIGFILE)
clean:
	/bin/rm -f *~
distclean: clean
	/bin/rm -f $(BIGFILE) $(CONTROL_FILE) $(CONTROL_FILE).res $(CONTROL_FILE).log
.PHONY: all check clean distclean
