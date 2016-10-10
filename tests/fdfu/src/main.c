#define __private_include__
#include <unistd.h>
#include <flipper.h>
#include <platform/posix.h>
#include <platform/fvm.h>
#include <platform/atsam4s16b.h>

/* Defines the XMODEM flow control bytes. */
#define SOH 0x01
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define ETB 0x17
#define CAN 0x18
#define XLEN 128

/* Defines the layout of an XMODEM packet. */
struct __attribute__((__packed__)) _xpacket {
	uint8_t header;
	uint8_t number;
	uint8_t _number;
	uint8_t data[XLEN];
	uint16_t checksum;
};

/* See utils/copy_x.s for the source of this applet. These are the raw thumb instructions that result from the compilation of the applet. */
uint8_t applet[] = {
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x09, 0x48, 0x0A, 0x49,
	0x0B, 0x4A, 0x02, 0xE0,
	0x08, 0xC9, 0x08, 0xC0,
	0x01, 0x3A, 0x00, 0x2A,
	0xFA, 0xD1, 0x09, 0x48,
	0x0A, 0x49, 0x06, 0x4A,
	0x11, 0x43, 0x01, 0x60,
	0x70, 0x47, 0x00, 0xBF,
	0xAF, 0xF3, 0x00, 0x80,
	0xAF, 0xF3, 0x00, 0x80,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x80, 0x00, 0x00, 0x00,
	0x04, 0x0A, 0x0E, 0x40,
	0x08, 0x0A, 0x0E, 0x40,
	0x01, 0x00, 0x00, 0x5A
};

/* Place the applet in RAM somewhere far away from the region used by the SAM-BA. */
#define _APPLET IRAM_ADDR + 0x800
#define _APPLET_STACK _APPLET
#define _APPLET_ENTRY _APPLET + 0x04
#define _APPLET_DESTINATION _APPLET + 0x30
#define _APPLET_SOURCE _APPLET_DESTINATION + 0x04
#define _APPLET_PAGE _APPLET_SOURCE + 0x04
#define _APPLET_WORDS _APPLET_PAGE + 0x04
#define _PAGEBUFFER _APPLET + sizeof(applet)

#define EFC_CLB 0x09
#define EFC_SGPB 0x0B
#define EFC_GGPB 0x0D

#define REGADDR(reg) (uint32_t)&(reg)

/* Defines the number of times communication will be retried. */
#define RETRIES 4

/* Instructs the SAM-BA to jump to the given address. */
void sam_ba_jump(uint32_t address) {
	char buffer[11];
	sprintf(buffer, "G%08X#", address);
	uart0.push(buffer, sizeof(buffer) - 1);
}

/* Instructs the SAM-BA to write a word to the address provided. */
void sam_ba_write_word(uint32_t destination, uint32_t word) {
	char buffer[20];
	sprintf(buffer, "W%08X,%08X#", destination, word);
	uart0.push(buffer, sizeof(buffer) - 1);
}

/* Instructs the SAM-BA to read a byte from the address provided. */
uint8_t sam_ba_read_byte(uint32_t source) {
	char buffer[12];
	sprintf(buffer, "o%08X,#", source);
	uart0.push(buffer, sizeof(buffer) - 1);
	uint32_t retries = 0;
	while(!uart0.ready() && retries ++ < 8);
	return uart0.get();
}

/* Instructs the SAM-BA to write a byte from the address provided. */
void sam_ba_write_byte(uint32_t destination, uint8_t byte) {
	char buffer[20];
	sprintf(buffer, "O%08X,%02X#", destination, byte);
	uart0.push(buffer, sizeof(buffer) - 1);
}

/* Instructs the SAM-BA to read a word from the address provided. */
uint32_t sam_ba_read_word(uint32_t source) {
	char buffer[12];
	sprintf(buffer, "w%08X,#", source);
	uart0.push(buffer, sizeof(buffer) - 1);
	uint8_t retries = 0;
	while(!uart0.ready() && retries ++ < 8);
	uint32_t result = 0;
	uart0.pull(&result, sizeof(uint32_t));
	return result;
}

/* Writes the given command and argument into the EFC -> EEFC_FCR register. */
void sam_ba_write_efc_fcr(uint8_t command, uint32_t arg) {
	sam_ba_write_word(REGADDR(EFC -> EEFC_FCR), (EEFC_FCR_FKEY(0x5A) | EEFC_FCR_FARG(arg) | EEFC_FCR_FCMD(command)));
}

/* Moves data from the host to the device's RAM using the SAM-BA and XMODEM protocol. */
int sam_ba_copy(uint32_t destination, void *source, uint32_t length) {
	/* Initialize the transfer. */
	char buffer[20];
	sprintf(buffer, "S%08X,%08X#", destination, length);
retry:
	uart0.push(buffer, sizeof(buffer) - 1);
	uint8_t retries = 0;
	while(!uart0.ready() && retries ++ < 8);
	/* Check for the clear to send byte. */
	if (uart0.get() != 'C') {
		return lf_error;
	}
	retries = 0;
	/* Calculate the number of packets needed to perform the transfer. */
	int packets = lf_ceiling(length, XLEN);
	for (int packet = 0; packet < packets; packet ++) {
		uint32_t _len = XLEN;
		if (length < _len) {
			_len = length;
		}
		/* Construct the XMODEM packet. */
		struct _xpacket _packet = { SOH, (packet + 1), ~(packet + 1), { 0 }, 0x00 };
		/* Copy the chunk of data into the packet. */
		memcpy(_packet.data, (void *)(source + (packet * XLEN)), _len);
		/* Calculate the checksum of the data and write it to the packet in little endian format. */
		_packet.checksum = little(lf_checksum(_packet.data, sizeof(_packet.data)));
		/* Transfer the packet to the SAM-BA. */
		uart0.push(&_packet, sizeof(struct _xpacket));
		/* Obtain acknowledgement. */
		retries = 0;
		while(!uart0.ready() && retries ++ < 8);
		if (uart0.get() != ACK) {
			return lf_error;
		}
		/* Decrement the length appropriately. */
		length -= _len;
	}
	/* Send end of transmission. */
	uart0.put(EOT);
	/* Obtain acknowledgement. */
	retries = 0;
	while(!uart0.ready() && retries ++ < 8);
	if (uart0.get() != ACK) {
		return lf_error;
	}
	return lf_success;
}

void *load_page_data(FILE *firmware, size_t size) {
	size_t pages = lf_ceiling(size, IFLASH_PAGE_SIZE);
	uint8_t *raw = (uint8_t *)malloc(pages * IFLASH_PAGE_SIZE);
	for (int i = 0; i < pages; i ++) {
		for (int j = 0; j < IFLASH_PAGE_SIZE; j ++) {
			uint8_t c = fgetc(firmware);
			if (!feof(firmware)) {
				raw[((i * IFLASH_PAGE_SIZE) + j)] = c;
			} else {
				raw[((i * IFLASH_PAGE_SIZE) + j)] = 0;
			}
		}

	}
	/* Close the file. */
	fclose(firmware);
	return raw;
}

int main(int argc, char *argv[]) {

	/* Ensure the correct argument count. */
	if (argc < 2) {
		fprintf(stderr, "Please provide a path to the firmware.\n");
		return EXIT_FAILURE;
	}

	/* Attach to a Flipper device. */
	flipper.attach();

	/* Open the firmware image. */
	FILE *firmware = fopen(argv[1], "rb");
	if (!firmware) {
		fprintf(stderr, "The file being opened, '%s', does not exist.\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* Determine the size of the file. */
	fseek(firmware, 0L, SEEK_END);
	size_t firmware_size = ftell(firmware);
	fseek(firmware, 0L, SEEK_SET);

	printf("Entering update mode.\n");
	uint8_t retries = 0;
retry_dfu:
	/* Send the synchronization character. */
	uart0.put('#');
	char d_ack[3];
	/* Check for acknowledgement. */
	uart0.pull(d_ack, sizeof(d_ack));
	if (!memcmp(d_ack, (char []){ 0x0a, 0x0d, 0x3e }, sizeof(d_ack))) {
		fprintf(stderr, KGRN " Successfully entered update mode.\n" KNRM);
		goto connected;
	}

	if (retries > RETRIES) {
		/* If no acknowledgement was received, throw and error. */
		fprintf(stderr, KRED "Failed to enter update mode.\n");
		return EXIT_FAILURE;
	}

	/* Enter DFU mode. */
	cpu.dfu();

	for (int i = 0; i < 20; i ++) {
		printf(".");
		fflush(stdout);
		usleep(250000);
	}
	printf("\n");

	retries ++;
	goto retry_dfu;

connected:

	/* Set normal mode. */
	printf("Entering normal mode.\n");
	uart0.push("N#", 2);
	char n_ack[2];
	uart0.pull(n_ack, sizeof(n_ack));
	retries = 0;
	while(!uart0.ready() && retries ++ < 8);
	retries = 0;
	if (memcmp(n_ack, (char []){ 0x0A, 0x0D }, sizeof(n_ack))) {
		fprintf(stderr, "Failed to enter normal mode.\n");
		return EXIT_FAILURE;
	}
	printf(KGRN " Successfully entered normal mode.\n" KNRM);

	printf("Checking security bit.\n");
	sam_ba_write_efc_fcr(EFC_GGPB, 0);
	if (sam_ba_read_word(REGADDR(EFC -> EEFC_FRR)) & 0x01) {
		fprintf(stderr, KRED "The device's security bit is set. Please erase again.\n");
		return EXIT_FAILURE;
	}
	printf(KGRN " Security bit is clear.\n" KNRM);

	printf("Uploading copy applet.\n");
	/* Move the copy applet into RAM. */
	int _e = sam_ba_copy(_APPLET, applet, sizeof(applet));
	if (_e < lf_success) {
		fprintf(stderr, KRED "Failed to upload copy applet.\n" KNRM);
		return EXIT_FAILURE;
	}
	printf(KGRN " Successfully uploaded copy applet.\n" KNRM);

	/* Write the stack address into the applet. */
	sam_ba_write_word(_APPLET_STACK, IRAM_ADDR + IRAM_SIZE);
	/* Write the entry address into the applet. */
	sam_ba_write_word(_APPLET_ENTRY, _APPLET + 0x09);
	/* Write the destination of the page data into the applet. */
	sam_ba_write_word(_APPLET_DESTINATION, IFLASH_ADDR);
	/* Write the source of the page data into the applet. */
	sam_ba_write_word(_APPLET_SOURCE, _PAGEBUFFER);

	/* Obtain a linear buffer of the firmware precalculated by page. */
	void *pagedata = load_page_data(firmware, firmware_size);

	/* Calculate the number of pages to send. */
	lf_size_t pages = lf_ceiling(firmware_size, IFLASH_PAGE_SIZE);
	/* Send the firmware, page by page. */
	for (lf_size_t page = 0; page < pages; page ++) {
		/* Print the page count. */
		printf("Uploading page %i / %u. (%.2f%%)", page + 1, pages, ((float)(page + 1))/pages*100);
		fflush(stdout);
		/* Copy the page. */
		int _e = sam_ba_copy(_PAGEBUFFER, (void *)(pagedata + (page * IFLASH_PAGE_SIZE)), IFLASH_PAGE_SIZE);
		if (_e < lf_success) {
			fprintf(stderr, KRED "\nFailed to upload page %i of %i.\n" KNRM, page + 1, pages);
			goto done;
		}
		/* Write the page number into the applet. */
		sam_ba_write_word(_APPLET_PAGE, EEFC_FCR_FARG(page));
		/* Execute the applet to load the page into flash. */
		sam_ba_jump(_APPLET);
		/* Wait until the EFC has finished writing the page. */
		uint8_t retries = 0, fsr = 0;
		while(!((fsr = sam_ba_read_byte(REGADDR(EFC -> EEFC_FSR))) & 1) && retries ++ < 4) {
			if (fsr & 0xE) {
				fprintf(stderr, KRED "Flash write error on page %u.\n" KNRM, page);
			}
		}
		retries = 0;
		/* Clear the progress message. */
		if (page < pages - 1) printf("\33[2K\r");
	}

	/* Print statistics about the memory usage. */
	printf(KGRN "\n Successfully uploaded all pages. %zu bytes used. (%.2f%% of flash)\n" KNRM, firmware_size, (float)firmware_size/IFLASH_SIZE*100);

	/* Set GPNVM1 to boot from flash memory. */
	sam_ba_write_efc_fcr(EFC_SGPB, 0x01);

	printf("Checking GPNVM1 bit.\n");
	sam_ba_write_efc_fcr(EFC_GGPB, 0);
	retries = 0;
	if (!(sam_ba_read_byte(REGADDR(EFC -> EEFC_FRR)) & (1 << 1)) && retries ++ < RETRIES) {
		if (retries > RETRIES) {
			printf(KRED " GPNVM1 bit is not set.\n" KNRM);
			return EXIT_FAILURE;
		}
		/* Set GPNVM1 to boot from flash memory. */
		sam_ba_write_efc_fcr(EFC_SGPB, 0x01);
		/* Read the state of the GPNVM bits. */
		sam_ba_write_efc_fcr(EFC_GGPB, 0);
	}
	printf(KGRN "The device's GPNVM1 bit is set.\n" KNRM);

	if (argc > 2) {
		if (!strcmp(argv[2], "verify")) {
			printf("\nVerifying flash contents.\n");
			uint32_t errors = 0, perrors = 0, total = lf_ceiling(firmware_size, sizeof(uint32_t));
			uint8_t retries = 0;
			for (uint32_t i = 0; i < total; i ++) {
				uint32_t addr = IFLASH_ADDR + (i * sizeof(uint32_t));
				if ((i % (IFLASH_PAGE_SIZE/sizeof(uint32_t)) == 0)) {
					printf(" Checking address 0x%08x (page %lu) -> %s\n", addr, i / (IFLASH_PAGE_SIZE/sizeof(uint32_t)), (!perrors) ? KGRN "GOOD" KNRM : KRED "BAD" KNRM);
				}
				uint32_t word = sam_ba_read_word(addr);
				uint32_t _word = *(uint32_t *)(pagedata + (i * sizeof(uint32_t)));
				uint8_t match = ((uint16_t)word == (uint16_t)_word);
				//printf("0x%08x: 0x%08x (0x%08x) -> %s\n", addr, word, _word, (match) ? KGRN "GOOD" KNRM : KRED "BAD" KNRM);
				if (!match && retries < RETRIES) {
					if (retries == 0) {
						perrors ++;
						errors ++;
					}
					retries ++;
					continue;
				}
				retries = 0;
				perrors = 0;
			}
			printf("Verification complete. %s%i word errors detected.\n\n" KNRM, (errors) ? KRED : KGRN, errors);
		}
	}

	printf("Resetting the CPU.\n");
	/* Reset the CPU. */
	cpu.power(0);
	usleep(1000000);
	cpu.power(1);
	//cpu.reset();
	printf(KGRN " Successfully reset the CPU.\n" KNRM "----------------------");

	printf(KGRN "\nSuccessfully uploaded new firmware.\n" KNRM);

done:
	/* Free the memory allocated to hold the page data. */
	free(pagedata);

	return EXIT_SUCCESS;
}
