TARGET = map.efi

export TOPDIR	:= $(shell pwd)/

include Make.rules

all: $(TARGET) bin2c
	./bin2c map.efi ../../include/grub/$(GRUBARCH)/efi/map.h map_bin

lib/lib.a lib/lib-efi.a: FORCE
	$(MAKE) -C lib $(notdir $@)

map.so: lib/lib-efi.a

bin2c : util/bin2c.c
	$(CC) $< -o $@

clean:
	rm -f *.o *.so *.efi bin2c
	$(MAKE) -C lib clean

FORCE:
