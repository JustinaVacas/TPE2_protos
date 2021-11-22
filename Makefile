all:  util proxy

util:
	cd utils; make all

proxy:
	cd pop3filter; make all

clean:
	cd utils; make clean
	cd pop3filter; make clean

.PHONY: all clean