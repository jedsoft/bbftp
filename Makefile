CONFIGOPTS = --prefix=/tmp/root
#
all:
	cd bbftp-client/bbftpc && $(MAKE)
	cd bbftp-server/bbftpd && $(MAKE)
config:
	cd bbftpd-client/bbftpc && ./configure $(CONFIGOPTS)
	cd bbftpd-server/bbftpd && ./configure $(CONFIGOPTS)
install:
	cd bbftp-client/bbftpc && $(MAKE) install
	cd bbftp-server/bbftpd && $(MAKE) install
clean:
	cd bbftp-client/bbftpc && $(MAKE) clean
	cd bbftp-server/bbftpd && $(MAKE) clean
.PHONY: all install clean config

