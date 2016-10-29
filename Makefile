CFLAGS=-g -Wall -Werror -std=c99 -pedantic
MICROTHREAD_SOURCES=microthreads.c microthread_arch.c switch_amd64.S

#CFLAGS=-g -Wall -Werror -std=c99 -pedantic -m32
#MICROTHREAD_SOURCES=microthreads.c microthread_arch.c switch_i386.S


all: simple_test speed_test cond_test

simple_test: simple_test.c $(MICROTHREAD_SOURCES)
	gcc -o $@ $(CFLAGS) $^

speed_test: speed_test.c $(MICROTHREAD_SOURCES)
	gcc -o $@ $(CFLAGS) $^

cond_test: cond_test.c $(MICROTHREAD_SOURCES)
	gcc -o $@ $(CFLAGS) $^

# for inspection
microthreads.s: microthreads.c
	gcc -S -o $@ $(CFLAGS) $<

clean:
	rm -f cond_test simple_test speed_test
