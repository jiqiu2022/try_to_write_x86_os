SECTIONS
{
    . = 0x000100000;

	.text : {
		*(.text)
	} 

	.rodata : {
		*(.rodata)
	}

	.data : {
		*(.data)
	}
	.bss : {
		*(.bss)
	}
}
