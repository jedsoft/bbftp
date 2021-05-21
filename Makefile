CONFIGOPTS = --prefix=/usr/local
BBFTPC_ENV_OPTS =
BBFTPD_ENV_OPTS = CFLAGS="$$CFLAGS -Bstatic"
#
usage:
	@cat INSTALL
build:
	cd bbftp-client/bbftpc && $(MAKE)
	cd bbftp-server/bbftpd && $(MAKE)
config:
	cd bbftp-client/bbftpc && ./configure $(CONFIGOPTS)
	cd bbftp-server/bbftpd && $(BBFTPD_ENV_OPTS) ./configure $(CONFIGOPTS)
install:
	cd bbftp-client/bbftpc && $(MAKE) install
	cd bbftp-server/bbftpd && $(MAKE) install
version:
	update_changes_version changes.txt bbftp-client/includes/version.h bbftp-server/includes/version.h
clean:
	cd bbftp-client/bbftpc && $(MAKE) clean
	cd bbftp-server/bbftpd && $(MAKE) clean
	cd tests && $(MAKE) clean
	rm -f *~
distclean: clean
	cd bbftp-client/bbftpc && $(MAKE) distclean
	cd bbftp-server/bbftpd && $(MAKE) distclean
	cd tests && $(MAKE) distclean
check: all
	cd tests && $(MAKE) check
.PHONY: all install clean config check distclean version

