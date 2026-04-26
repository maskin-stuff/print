CFLAGS	:=	$(shell pkg-config --cflags libusb-1.0) -g -Og
LDFLAGS	:=	$(shell pkg-config --libs libusb-1.0)

print: *.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(@) $(^)
