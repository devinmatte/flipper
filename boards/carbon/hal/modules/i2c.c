#define __private_include__
#include <flipper/libflipper.h>
#include <flipper/carbon/modules/i2c.h>

int i2c_configure(void) {
	return lf_invoke(&_i2c, _i2c_configure, NULL);
}