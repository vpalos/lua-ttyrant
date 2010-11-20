#
# Compilation flags.
#
CFLAGS      = -O2
LFLAGS      = -ltokyotyrant
LUA         = 'lua5.1'
LUA_CFLAGS  = $(shell pkg-config $(LUA) --cflags)
LUA_LFLAGS  = $(shell pkg-config $(LUA) --libs)

#
# Targets.
#
all: build

build: ttyrant.so

test: build
	@ lua ttyrant-test.lua

clean:
	rm -rf *.so
	rm -rf *.o

ttyrant.so: ttyrant.c
	@ echo -n "ttyrant.so... "
	@ $(CC) $(CFLAGS) $(LUA_CFLAGS) $(LFLAGS) $(LUA_LFLAGS) -fPIC -shared -o ttyrant.so ttyrant.c
	@ echo "OK"
