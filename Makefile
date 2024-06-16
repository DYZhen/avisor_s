ARCH := aarch64
PLATFORM := qemu-aarch64-virt

# directories
target_exec := avisor
cur_dir := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
build_dir := $(cur_dir)/build
src_dir := $(cur_dir)/src
arch_dir := $(src_dir)/arch/$(ARCH)
plat_dir := $(src_dir)/plat/$(PLATFORM)
core_dir := $(src_dir)/core
lib_dir := $(src_dir)/lib
src_dirs := $(arch_dir) $(core_dir) $(lib_dir) $(plat_dir)
inc_dirs := $(addsuffix /inc, $(src_dirs))
build_dirs := $(patsubst $(src_dir)%,$(build_dir)%,$(src_dirs))

# compiler settings
cross_prefix := aarch64-linux-gnu-
ccx := $(cross_prefix)g++
cc := $(cross_prefix)gcc
objdump := $(cross_prefix)objdump
cflags := -Wall -fno-common -O0 -g -c\
          -nostdlib -nostartfiles -ffreestanding \
          -march=armv8-a \
		  $(addprefix -I, $(inc_dirs))\
		#   -DSCHEDULE
		  
rm := rm

# qemu
qemu := qemu-system-aarch64
gic_version := 2
memory := 4g
device := virt,virtualization=on
cpu := cortex-a57
num_cpu := 2
iommu := none

# wildcards
# sources := $(shell find $(src_dirs) -name '*.c' -o -name '*.S')
sources := $(shell find $(src_dirs) \( \( -name '*.c' -o -name '*.S' \) \
				 -a ! -name '*gicv*' \
				 -a ! -name 'smmuv*' \))
sources += $(src_dir)/main.c
sources += $(src_dir)/config.c

# select gic version
cflags += -DGIC_VERSION=$(gic_version)
ifeq ($(gic_version), 3)
	sources += $(arch_dir)/gicv3.c
	sources += $(arch_dir)/vgicv3.c
else ifeq ($(gic_version), 2)
	sources += $(arch_dir)/gicv2.c
	sources += $(arch_dir)/vgicv2.c
else
	$(error "Invalid GIC version: $(gic_version). Valid options are 2 or 3.")
endif

ifeq ($(iommu), smmuv3)
	sources += $(arch_dir)/smmuv3.c
	cflags += -DSMMU_VERSION=3
else ifeq ($(iommu), smmuv2)
	sources += $(arch_dir)/smmuv2.c
	cflags += -DSMMU_VERSION=2
else ifneq ($(iommu), none)
	$(error "Invalid SMMU version: $(gic_version). Valid options are 2 or 3.")
endif

objs := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(sources)))
objs := $(patsubst $(src_dir)%, $(build_dir)%, $(objs))
deps := $(patsubst %.o,%.d,$(objs))

ld_script := $(src_dir)/main.ld
ld_script_temp := $(build_dir)/temp.ld
deps += $(ld_script_temp).d

.PHONY: all
all: $(target_exec)

-include $(deps)

.PHONY: clean
clean:
	$(rm) -r ./build

debug: qemu-debug

qemu: qemu-start

qemu-start: $(target_exec)
	$(qemu) -M $(device) -machine gic-version=$(gic_version),iommu=$(iommu) \
		-cpu $(cpu) -smp $(num_cpu) -m $(memory) -nographic \
		-kernel $(build_dir)/$(target_exec).elf

qemu-debug: $(target_exec)
	$(qemu) -s -S -M $(device) -machine gic-version=$(gic_version),iommu=$(iommu) \
		-cpu $(cpu) -smp $(num_cpu) -m $(memory) \
		-nographic -kernel $(build_dir)/$(target_exec).elf

telnet:
	gdb-multiarch -q -ex 'file build/avisor.elf' -ex 'target remote localhost:1234'

$(target_exec): $(objs) $(ld_script_temp)
	@echo "Linking			$(patsubst $(cur_dir)/%, %, $@)"
	@$(cross_prefix)ld -T $(ld_script_temp) $(objs) -o $(build_dir)/$@.elf

$(build_dir)/%.d : $(src_dir)/%.[c,S]
	@echo "Creating dependency	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) -MM -MG -MT "$(patsubst %.d, %.o, $@) $@" $(cflags) $< > $@

$(objs):
	@echo "Compiling source	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) $(cflags) -c $< -o $@

$(ld_script_temp).d: $(ld_script)
	@echo "Creating dependency	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) -x assembler-with-cpp -MM -MT "$(ld_script_temp) $@" \
		$(addprefix -I, $(inc_dirs))  $< > $@

$(ld_script_temp): $(ld_script)
	@echo "Compiling source	$(patsubst $(cur_dir)/%, %, $<)"
	@$(cc) -E -x assembler-with-cpp $(cflags) $< | grep -v '^\#' > $(ld_script_temp)

$(deps): | $(build_dirs)

$(build_dirs):
	mkdir -p $@