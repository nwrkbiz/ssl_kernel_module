modulename :=  sigtest
obj-m += $(modulename).o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers


deploy: all
	scp $(modulename).ko "$(DEPLOYSSH):$(DEPLOYSSHPATH)/$(modulename).ko";\
	ssh $(DEPLOYSSH) "rmmod $(modulename)";\
	ssh $(DEPLOYSSH) "insmod $(DEPLOYSSHPATH)/$(modulename).ko";