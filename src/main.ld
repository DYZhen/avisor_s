#include "util.h"

ENTRY(start)
SECTIONS
{
	. = AVISOR_BASE;
	__avisor_begin = ABSOLUTE(.);
	
	.dtb_images 0x40000000:
	{
		KEEP(*(.dtb_image*))
	}
	.vm_images 0x40100000:
	{
		KEEP(*(.vm_image*))
		. = . + 0x8000000;
	}

	.text ALIGN (PAGE_SIZE): 
	{ 
		*(vectors);
		*(.text);
	}
	.data ALIGN (PAGE_SIZE): 
	{ 
		*(.data) 
	}
	.bss ALIGN (PAGE_SIZE): 
	{ 
		*(.bss COMMON) 
	}

	.ipi_cpumsg_handlers : {
		ipi_cpumsg_handlers = .;
		*(.ipi_cpumsg_handlers)
	}

	.ipi_cpumsg_handlers_id : {
		_ipi_cpumsg_handlers_id_start = .;
		*(.ipi_cpumsg_handlers_id)
	}

	_ipi_cpumsg_handlers_size = SIZEOF(.ipi_cpumsg_handlers);

    . = ALIGN(PAGE_SIZE);

	. = CPU_BASE;
	__mem_cpu_begin = ABSOLUTE(.);

	. = VM_BASE;
	__mem_cpu_end = ABSOLUTE(.);
	__mem_vm_begin = ABSOLUTE(.);
	. = . + VM_SIZE;
	__mem_vm_end = ABSOLUTE(.);

	. = ALIGN(PAGE_SIZE);
}