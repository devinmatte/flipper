#define __private_include__
#include <flipper/spi.h>
#include <flipper/fmr.h>

int spi_configure() {
	return lf_success;
}

void spi_enable(void) {

}

void spi_disable(void) {

}

uint8_t spi_ready(void) {
	return 0;
}

void spi_put(uint8_t byte) {

}

uint8_t spi_get(void) {
	return 0;
}

void spi_push(void *source, uint32_t length) {

}

void spi_pull(void *destination, uint32_t length) {

}
