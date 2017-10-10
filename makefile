# Directory where build products are stored.
BUILD := build

# List of all target types
TARGETS := ARM AVR X86

# ARM target variables
ARM_TARGET	 := atsam4s

ARM_PREFIX	 := arm-none-eabi-

# Directories that need to be included for this target.
ARM_INC_DIRS := carbon/include 										\
				kernel/include 										\
				runtime/include

ARM_SRC_DIRS := carbon/atsam4s										\
				kernel/src											\
				kernel/arch/armv7									\
				runtime/arch/armv7									\
				runtime/src

ARM_CFLAGS	 := -std=c99											\
				-Os													\
				-mcpu=cortex-m4										\
				-mthumb												\
				-nostartfiles										\
				-D__disable_error_side_effects__					\
				-D__ATSAM4SB__										\
				-DPLATFORM_HEADER="<flipper/atsam4s/atsam4s.h>"		\
				$(foreach inc,$(ARM_INC_DIRS),-I$(inc))

ARM_LDFLAGS  := -Wl,-T carbon/atsam4s/sam4s16.ld					\
				-Wl,--gc-sections

atsam4s: atsam4s.bin

# AVR target variables
AVR_TARGET	 := atmegau2

AVR_PREFIX	 := avr-

# Directories that need to be included for this target.
AVR_INC_DIRS := carbon/include 										\
				kernel/include 										\
				runtime/include

AVR_SRC_DIRS := carbon/atmegau2 									\
				kernel/src 											\
				runtime/arch/avr8									\
				runtime/src

AVR_CFLAGS 	 := -std=c99											\
				-Os													\
				-mmcu=atmega16u2									\
				-DARCH=ARCH_AVR8									\
				-D__AVR_AT90USB162__								\
				-DF_CPU=16000000UL									\
				-D__disable_error_side_effects__					\
				-D__ATMEGAU2__										\
				-DPLATFORM_HEADER="<flipper/atmegau2/atmegau2.h>"	\
				$(foreach inc,$(AVR_INC_DIRS),-I$(inc))

AVR_LDFLAGS  := -mmcu=atmega16u2									\
				-Wl,--gc-sections

atmega16u2: atmega16u2.hex

install-atmega16u2: atmega16u2
	dfu-programmer at90usb162 erase
	dfu-programmer at90usb162 flash $(TARGET_HEX)
	dfu-programmer at90usb162 launch --no-reset

# x86 target variables
X86_TARGET	 := libflipper

X86_PREFIX	 :=

# Directories that need to be included for this target.
X86_INC_DIRS := carbon/include 										\
				library/include 									\
				runtime/include

X86_SRC_DIRS := carbon/hal											\
				library/src											\
				library/arch/x86_64									\
				library/platforms/posix								\
				runtime/src

X86_CFLAGS	 := -std=c99											\
				-g													\
				-fpic												\
				-DPLATFORM_HEADER="<flipper/posix/posix.h>"			\
				$(foreach inc,$(X86_INC_DIRS),-I$(inc))

X86_LDFLAGS  := -lusb-1.0

libflipper: $(X86_TARGET).so

install-libflipper: libflipper
	$(_v)mkdir -p $(BUILD)/include/flipper
	$(_v)cp -r carbon/include/flipper/* $(BUILD)/include/flipper
	$(_v)cp -r library/include/flipper/* $(BUILD)/include/flipper
	$(_v)cp -r runtime/include/flipper/* $(BUILD)/include/flipper
	$(_v)cp library/include/flipper.h $(BUILD)/include
	$(_v)cp $(BUILD)/$(X86_TARGET)/$(X86_TARGET).so /usr/local/lib/
	$(_v)cp -r $(BUILD)/include/* /usr/local/include/

uninstall-libflipper:
	$(_v)rm -r /usr/local/include/flipper.h
	$(_v)rm -rf /usr/local/include/flipper
	$(_v)rm -r /usr/local/lib/$(X86_TARGET).so

# Print all commands executed when VERBOSE is defined
ifdef VERBOSE
_v :=
else #VERBOSE
_v := @
endif #VERBOSE

# Pre-declare double colon rules
all::

clean::

.PHONY: all clean


# Make sure that the .dir files aren't automatically deleted after building
.SECONDARY:

%/.dir:
	$(_v)mkdir -p $* && touch $@

# Disable built-in rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:


#####
# find_srcs($1: source directories, $2: source file extensions)
#####
find_srcs = $(foreach sd,$1,$(foreach ext,$2,$(wildcard $(sd)/*.$(ext))))
#####

# All supported source file extensions.
SRC_EXTS := c S


#####
# generate_target($1: target prefix)
#
# Generate all of the target-specific build rules for the given target.
#####
define _generate_target
# Generate remaining variables
$1_BUILD := $$(BUILD)/$$($1_TARGET)
$1_ELF :=  $$($1_TARGET).elf
$1_HEX := $$($1_TARGET).hex
$1_BIN := $$($1_TARGET).bin
$1_SO := $$($1_TARGET).so
$1_SRCS := $$(call find_srcs,$$($1_SRC_DIRS),$$(SRC_EXTS))
$1_OBJS := $$(patsubst %,$$($1_BUILD)/%.o,$$($1_SRCS))
$1_DEPS := $$($1_OBJS:.o=.d)
$1_BUILD_DIRS := $$($1_BUILD) $$(addprefix $$($1_BUILD)/,$$($1_SRC_DIRS))
$1_BUILD_DIR_FILES := $$(addsuffix /.dir,$$($1_BUILD_DIRS))
$1_CC := $$($1_PREFIX)gcc
$1_AS := $$($1_PREFIX)gcc
$1_LD := $$($1_PREFIX)gcc
$1_OBJCOPY := $$($1_PREFIX)objcopy
$1_OBJDUMP := $$($1_PREFIX)objdump

# Add target to the all rule
all:: $$($1_TARGET)

.PHONY: $$($1_TARGET)

# Linking rule
$$($1_ELF): $$($1_OBJS)
	$$(_v)$$($1_LD) $$($1_LDFLAGS) -o $$($1_BUILD)/$$@ $$^

# Objcopy-ing rule
$$($1_HEX): $$($1_ELF)
	$$(_v)$$($1_OBJCOPY) -O ihex $$($1_BUILD)/$$< $$($1_BUILD)/$$@

# Objcopy-ing rule
$$($1_BIN): $$($1_ELF)
	$$(_v)$$($1_OBJCOPY) -O binary $$($1_BUILD)/$$< $$($1_BUILD)/$$@

# Linking rule
$$($1_SO): $$($1_OBJS)
	$$(_v)$$($1_LD) -shared $$($1_LDFLAGS) -o $$($1_BUILD)/$$@ $$^

# Compiling rule for C sources
$$($1_BUILD)/%.c.o: %.c | $$($1_BUILD_DIR_FILES)
	$$(_v)$$($1_CC) $$($1_CFLAGS) -I$$(<D) -MD -MP -MF $$($1_BUILD)/$$*.c.d -c -o $$@ $$<

# Compiling rule for S sources
$$($1_BUILD)/%.S.o: %.S | $$($1_BUILD_DIR_FILES)
	$$(_v)$$($1_AS) $$($1_ASFLAGS) $$($1_CFLAGS) -I$$(<D) -MD -MP -MF $$($1_BUILD)/$$*.S.d -c -o $$@ $$<

# Build dependency rules
-include $$($1_DEPS)

# Add to the clean rule
clean::
	$$(_v)rm -rf $$($1_BUILD)

endef
generate_target = $(eval $(call _generate_target,$1))
#####

# Generate all of the rules for every target
$(foreach target,$(TARGETS),$(call generate_target,$(target)))
