# warnings, and use C99 with GNU extensions
CFLAGS = -Wall -std=gnu99

# preprocessor flags
CPPFLAGS = -Isrc/


all: depend bin/daemon lib/libnetdaemon.so

bin/daemon : lib/libnetdaemon.so build/obj/daemon/service.o
lib/libnetdaemon.so : build/obj/lib/helloworld.o

SRC_PATHS = $(wildcard src/*.c) $(wildcard src/*/*.c)
SRC_NAMES = $(patsubst src/%,%,$(SRC_PATHS))
SRC_DIRS = $(dir $(SRC_NAMES))

dirs: 
	mkdir bin lib
	mkdir -p $(SRC_DIRS:%=build/deps/%)
	mkdir -p $(SRC_DIRS:%=build/obj/%)

depend: $(SRC_NAMES:%.c=build/deps/%.d)

build/deps/%.d : src/%.c
	@set -e; rm -f $@; \
	 $(CC) -MM -MT __ $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,__[ :]*,build/obj/$*.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

include $(wildcard build/deps/*/*.d)

build/obj/%.o : src/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

build/obj/lib/%.o : src/lib/%.c
	$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) $< -o $@

bin/% : build/obj/%/main.o
	$(CC) $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

lib/lib%.so :
	$(CC) -shared $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

