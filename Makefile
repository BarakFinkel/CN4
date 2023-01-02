make all: parta partb watchdog

watchdog: watchdog.c
	gcc watchdog.c -o watchdog

partb: better_ping.c
	gcc better_ping.c -o partb

parta: ping.c
	gcc ping.c -o parta

clean:
	rm parta partb watchdog