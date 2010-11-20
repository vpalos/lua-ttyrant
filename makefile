#
# Compilation flags.
#
CFLAGS = -O2 -fPIC -shared
LFLAGS = -ltokyotyrant $(shell pkg-config 'lua5.1' --cflags --libs)

#
# Targets.
#
all: build

build: ttyrant.so

test: build
	@ lua ttyrant-test.lua

clean:
	rm -rf *.so

ttyrant.so: ttyrant.c
	@ echo -n "ttyrant.so... "
	@ $(CC) $(CFLAGS) $(LFLAGS) -o $@ ttyrant.c
	@ echo "OK"
