#ifndef CONFIG_H
#define CONFIG_H

#include "util.h"
#include "vm.h"
#include "platform.h"
#include "rq.h"

#define VM_IMAGE(img_name, img_path)                                         \
    extern uint8_t _##img_name##_vm_size;                                    \
    extern uint8_t _##img_name##_vm_beg;                                     \
    asm(".pushsection .vm_image_" XSTR(img_name) ", \"a\"\n\t"               \
        ".global _" XSTR(img_name) "_vm_beg\n\t"                             \
        "_" XSTR(img_name) "_vm_beg:\n\t"                                    \
        ".incbin " XSTR(img_path) "\n\t"                                     \
        "_" XSTR(img_name) "_vm_end:\n\t"                                    \
        ".global _" XSTR(img_name) "_vm_size\n\t"                            \
        ".set _" XSTR(img_name) "_vm_size,  (_" XSTR(img_name) "_vm_end - _" \
        #img_name "_vm_beg)\n\t"                                             \
        ".popsection");

#define VM_IMAGE_OFFSET(img_name) ((uint64_t)&_##img_name##_vm_beg)
#define VM_IMAGE_SIZE(img_name) ((uint64_t)&_##img_name##_vm_size)

#define DTB_IMAGE(dtb_name, dtb_path)                                         \
    extern uint8_t _##dtb_name##_dtb_size;                                    \
    extern uint8_t _##dtb_name##_dtb_beg;                                     \
    asm(".pushsection .dtb_image_" XSTR(dtb_name) ", \"a\"\n\t"               \
        ".global _" XSTR(dtb_name) "_dtb_beg\n\t"                             \
        "_" XSTR(dtb_name) "_dtb_beg:\n\t"                                    \
        ".incbin " XSTR(dtb_path) "\n\t"                                     \
        "_" XSTR(dtb_name) "_dtb_end:\n\t"                                    \
        ".global _" XSTR(dtb_name) "_dtb_size\n\t"                            \
        ".set _" XSTR(dtb_name) "_dtb_size,  (_" XSTR(dtb_name) "_dtb_end - _" \
        #dtb_name "_dtb_beg)\n\t"                                             \
        ".popsection");

#define DTB_IMAGE_OFFSET(dtb_name) ((uint64_t)&_##dtb_name##_dtb_beg)
#define DTB_IMAGE_SIZE(dtb_name) ((uint64_t)&_##dtb_name##_dtb_size)

struct hyp_config {
    size_t nr_cpus;
};

struct dtb_config {
    vaddr_t base_addr;
    paddr_t load_addr;
    size_t size;
};

struct vm_config {
    vaddr_t base_addr;
    paddr_t load_addr;
    size_t size;
    vaddr_t entry;
    size_t dmem_size;
    size_t nr_cpus;

    size_t nr_devs;
    struct vm_dev_region* devs; 

    struct rq_config_vm rq_vm;

    struct arch_vm_platform arch;
};

extern struct config {
    struct hyp_config hyp;
    size_t nr_vms;
    struct vm_config* vm;
    struct dtb_config dtb;
} config;

#endif /* CONFIG_H */