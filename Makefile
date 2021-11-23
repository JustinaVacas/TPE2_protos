all:  util proxy pop3client

util:
	cd utils; make all

proxy:
	cd pop3filter; make all

pop3client:
	cd pop3client; make all

clean:
	cd utils; make clean
	cd pop3filter; make clean
	cd pop3client; make clean

.PHONY: all clean pop3filter pop3client