#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <getopt.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#define TIMEOUT   1000
#define ESC       27
#define PC850     2

int vendor = -1;
int product = -1;
int endpoint = -1;
int interface = -1;
int (*transfer)(libusb_device_handle *,
                unsigned char,
                unsigned char *,
                int,
                int *,
                unsigned int) = NULL;

static iconv_t ic = NULL;
static libusb_context *ctx = NULL;
static libusb_device_handle *handle = NULL;
static int claimed = 0;

static void cleanup(void)
{
	if (ic)
	{
		iconv_close(ic);
	}
	if (claimed)
	{
		libusb_release_interface(handle, interface);
	}
	if (handle)
	{
		libusb_close(handle);
	}
	if (ctx)
	{
		libusb_exit(ctx);
	}
}

static void print(const char *str)
{
	int err = transfer(
        	handle,
        	endpoint,
        	(unsigned char *) str,
        	strlen(str),
        	NULL,
		TIMEOUT
	);
	if (err != 0)
	{
		fprintf(stderr, "failed to transfer: %s\n", libusb_strerror(err));
		exit(EXIT_FAILURE);
	}
}

static int parse_int(const char *str)
{
	const char *ptr = str;
	int base = 10;
	int res;

	if (strncmp(ptr, "0x", 2) == 0)
	{
		base = 16;
		ptr += 2;
	}

	errno = 0;
	res = strtol(ptr, NULL, base);

	if (res < 0 || errno != 0)
	{
		fprintf(stderr, "Invalid number: %s\n", str);
		exit(EXIT_FAILURE);
	}

	return res;
}

static void usage(FILE *f, const char *program)
{
	fprintf(
		stderr,
		"Usage: %s -v VENDOR -p PRODUCT -e ENDPOINT -i INTERFACE -t TRANSFER\n",
		program
	);
}

#define ASSERT_ARG(x, n)\
	if (x == n)\
	{\
		fprintf(stderr, "Missing %s argument\n", #x);\
		usage(stderr, program);\
		exit(EXIT_FAILURE);\
	}

static void parse_args(int argc, char **argv)
{
	const char *program = argv[0];

	int c;
	const char *opts = "v:p:e:i:t:";

	while ((c = getopt(argc, argv, opts)) != -1)
	{
		switch (c)
		{
		case 'v':
			vendor = parse_int(optarg);
			break;
		case 'p':
			product = parse_int(optarg);
			break;
		case 'i':
			interface = parse_int(optarg);
			break;
		case 'e':
			endpoint = parse_int(optarg);
			break;
		case 't':
			if (strcmp(optarg, "bulk") == 0)
			{
				transfer = libusb_bulk_transfer;
			}
			else if (strcmp(optarg, "interrupt") == 0)
			{
				transfer = libusb_interrupt_transfer;
			}
			else
			{
				usage(stderr, program);
				fprintf(stderr, "Invalid transfer type %s\n", optarg);
				exit(EXIT_FAILURE);
			}
		}
	}

	ASSERT_ARG(vendor, -1);
	ASSERT_ARG(product, -1);
	ASSERT_ARG(endpoint, -1);
	ASSERT_ARG(interface, -1);
	ASSERT_ARG(transfer, NULL);
}

int main(int argc, char **argv)
{
	parse_args(argc, argv);

	atexit(cleanup);

	int err;
	if ((err = libusb_init_context(&ctx, NULL, 0)) != 0)
	{
		fprintf(stderr, "failed to initialize libusb\n");
		exit(EXIT_FAILURE);
	}
	if ((handle = libusb_open_device_with_vid_pid(ctx, vendor, product)) == NULL)
	{
		fprintf(stderr, "failed to open device\n");
		exit(EXIT_FAILURE);
	}
	if (libusb_kernel_driver_active(handle, interface) == 1)
	{
		libusb_detach_kernel_driver(handle, interface);
	}
	if ((err = libusb_claim_interface(handle, interface)) != 0)
	{
		fprintf(stderr, "failed to claim interface: %s\n", libusb_strerror(err));
		exit(EXIT_FAILURE);
	}

	claimed = 1;

	/* Select PC850 character code table */
	const char cmd[] = { ESC, 't', PC850, 0, };
	print(cmd);

	ic = iconv_open("CP850", "UTF-8");
	FILE *f = stdin;
	char *line = NULL;
	char *cp850 = NULL;
	size_t size;

	while (getline(&line, &size, f) >= 0)
	{
		cp850 = realloc(cp850, size);

		char *iptr = line, *optr = cp850;
		size_t isize, osize;
		isize = osize = size;

		iconv(ic, &iptr, &isize, &optr, &osize);
		print(cp850);
	}
}
