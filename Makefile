# warnings, and use C99 with GNU extensions
CFLAGS = -Wall -std=gnu99 -g

# preprocessor flags
CPPFLAGS = -Isrc/


all: depend bin/daemon lib/libnetdaemon.so bin/client

bin/daemon : lib/libnetdaemon.so \
	build/obj/daemon/daemon.o build/obj/daemon/service.o build/obj/daemon/client.o build/obj/daemon/commands.o \
    build/obj/daemon/process.o \
	build/obj/shared/select.o build/obj/shared/log.o

lib/libnetdaemon.so : build/obj/lib/client.o build/obj/shared/proto.o

bin/client : lib/libnetdaemon.so build/obj/shared/log.o

SRC_PATHS = $(wildcard src/*/*.c)
SRC_NAMES = $(patsubst src/%,%,$(SRC_PATHS))
SRC_DIRS = $(dir $(SRC_NAMES))

dirs: 
	mkdir -p bin lib run
	mkdir -p $(SRC_DIRS:%=build/deps/%)
	mkdir -p $(SRC_DIRS:%=build/obj/%)

# .h dependencies
depend: $(SRC_NAMES:%.c=build/deps/%.d)

build/deps/%.d : src/%.c
	@set -e; rm -f $@; \
	 $(CC) -MM -MT __ $(CPPFLAGS) $< > $@.$$$$; \
	 sed 's,__[ :]*,build/obj/$*.o $@ : ,g' < $@.$$$$ > $@; \
	 rm -f $@.$$$$

include $(wildcard build/deps/*/*.d)

# build (potential) library targets with specific cflags
# XXX: just build everything with -fPIC?
build/obj/shared/%.o : src/shared/%.c
	$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) $< -o $@

build/obj/lib/%.o : src/lib/%.c
	$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) $< -o $@

# general binary objects
build/obj/%.o : src/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

# output binaries
bin/% : build/obj/%/main.o
	$(CC) $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

# output libraries
lib/lib%.so :
	$(CC) -shared $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

