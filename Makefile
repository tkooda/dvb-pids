# Makefile

all: dvb-pids-tune

dvb-pids-tune: dvb-pids-tune-0.1.c
	gcc -Wall -O2 -o dvb-pids-tune dvb-pids-tune-0.1.c

clean:
	rm -f dvb-pids-tune *.o *~

