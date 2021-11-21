BIGFILE=test_bigfile.dat
BIGFILE_SIZE=3000019  #Prime number
#BIGFILE_SIZE=1
CONTROL_FILE = testcmds.bb
#
all:
	@echo Targets: check, memcheck, clean, distclean
check:
	./runtests.sl -c $(CONTROL_FILE) -H localhost -s $(BIGFILE_SIZE) -b $(BIGFILE)
	@echo Test passed, now run 'make clean'
memcheck:
	./runtests.sl --memcheck -c $(CONTROL_FILE) -H localhost -s $(BIGFILE_SIZE) -b $(BIGFILE)
clean:
	/bin/rm -f *~
	rm -f testcmds.bb* $(BIGFILE)
distclean: clean
	/bin/rm -f $(BIGFILE) $(CONTROL_FILE) $(CONTROL_FILE).res $(CONTROL_FILE).log
.PHONY: all check clean distclean
