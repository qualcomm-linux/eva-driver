SRC := $(shell pwd)
KBUILD_OPTIONS+= EVA_ROOT=$(SRC)
# TODO: need disable now, due to SYNX is not ready.
# KBUILD_EXTRA_SYMBOLS=$(INCLUDEDIR)/synx-kernel/Module.symvers

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) M=$(SRC) -C $(KERNEL_SRC) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
