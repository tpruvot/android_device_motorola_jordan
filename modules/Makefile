# Makefile for Android Defy kernel-omap tree
# "make device_modules" in repo root (not here)


# list of modules to build :
obj-m += symsearch/
obj-m += overclock/
obj-m += backlight/
obj-m += logcap/
obj-m += klogger/

# mb525/mb526 specific
obj-m += proximity/

# SGX ###############################

ifneq ($(CONFIG_SGX),y)

# force some config for the defy
CONFIG_SGX_REV125 ?= y
CONFIG_SGX_530_BUILD_RELEASE ?= y
CONFIG_ION_OMAP ?= y
# CONFIG_DSSCOMP ?= y

obj-m += omap3/gpu/pvr/

endif

#####################################

# experimental
obj-m += camera_misc/

EXTRA_CFLAGS += -I$(KERNEL_DIR)

#####################################
# 2nd-init only modules
#####################################
ifneq ($(CONFIG_MAPPHONE_2NDBOOT),y)

EXTRA_CFLAGS += -DUTS_RELEASE=\\\"2.6.32.9\\\"

# Only required for stock kernel
obj-m += multitouch/
obj-m += mmcfix/
obj-m += gpiofix/
obj-m += clockfix/
obj-m += netfilter/
obj-m += thumbee/
obj-m += usbled/

endif
#####################################

PWD := $(shell pwd)

.PHONY: clean
#######################################################################################
clean:
	@find $(PWD) | grep "\\.o$$" | xargs rm -f
	@find $(PWD) | grep "\\.ko$$" | xargs rm -f
	@find $(PWD) | grep "\\.cmd$$" | xargs rm -f
	@find $(PWD) | grep "\\.mod.c$$" | xargs rm -f
	@find $(PWD) | grep "\\.order$$" | xargs rm -f
	@find $(PWD) | grep "\\.tmp_versions$$" | xargs rm -rf
	@cd $(PWD) && git checkout *.ko

.PHONY: all
#######################################################################################
# Build the modules
#######################################################################################
all: modules
modules:
	pwd
	@echo $@ EXTRA_CFLAGS = $(EXTRA_CFLAGS)
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" ARCH=$(ARCH) -C $(KERNEL_DIR) M=$(PWD) modules
	@find $(PWD) | grep "\\.o$$" | xargs rm -f
	@find $(PWD) | grep "\\.cmd$$" | xargs rm -f
	@find $(PWD) | grep "\\.mod.c$$" | xargs rm -f
	@find $(PWD) | grep "\\.order$$" | xargs rm -f
	@find $(PWD) | grep "\\.tmp_versions$$" | xargs rm -rf

device_modules:
	@echo YOU ARE NOT AT THE GOOD PLACE TO DO THAT ! go to repo root and copy/paste that :
	@echo
	@echo . build/envsetup.sh
	@echo bib jordan
	@echo make device_modules
