/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * ******************************************************************************
 * This source code is part of the coursware provided with "Linux Device Drivers"
 * training program offered by Techveda <www.techveda.org>
 *
 * Copyright (C) 2020 Techveda
 *
 * Author: Raghu Bharadwaj
 *
 * Git repository:
 *   https://gitlab.com/techveda/ldd-0920
 * ******************************************************************************
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>

#define IOAPIC_BASE 0xFEC00000
//#define IOAPIC_BASE 0xFEC01000
void *io;
int init_module(void)
{
	int i, j, maxirq, ident, versn;
	unsigned int val_lo, val_hi;
	void *ioregsel, *iowin;

	int ioredtlb[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
		0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
		0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21,
		0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D,
		0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33,
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
	};

	/* 
	   ioremap - remaps a physical address range into the 
	   processor's virtual address space(kernel's linear address), making
	   it available to the kernel
	   IOAPIC_BASE: Physical address of IOAPIC
	   SIZE: size of the resource to map 
	 */
	io = ioremap(IOAPIC_BASE, PAGE_SIZE);

	/* 
	   As per IOAPIC Datasheet 0x00 is I/O REGISTER SELECT  
	   of size 32 bits
	 */
	ioregsel = (void *)((long)io + 0x00);

	/* 
	
	   As per IOAPIC Datasheet 0x10 is I/O WINDOW REGISTER of size
	   32 bits 
	 */
	iowin = (void *)((long)io + 0x10);

	printk("\n  I/O APIC       ");
	/* Read IOAPIC IDENTIFICATION 
	   As per IOAPIC Datasheet IOAPIC IDENTIFICATION REGISTER 
	   Address Offset : 0x00 

	   IOAPIC IDENTIFICATION REGISTER

	   Bits                 Description
	   ********************************
	   31:28                Reserved        

	   27:24                This 4 bit field contains the IOAPIC 
	   			identification.      

	   23:0                 Reserved        
	 */
	iowrite32(0, ioregsel);
	ident = ioread32(iowin);
	printk("Identification: %08X\n", ident);

	/* Read IOAPIC VERSION
	   As per  Datasheet IOAPIC VERSION REGISTER 
	   Address Offset : 0x01 

	   IOAPIC VERSION REGISTER

	   Bits                 Description
	   ********************************
	   31:24                Reserved        

	   23:16                This field contains number of interrupt
	   			input pins for the IOAPIC minus one.

	   15:8                 Reserved        

	   7:0                  This 8 bit field identifies the implementation 
	   			version.
	 */
	iowrite32(1, ioregsel);
	versn = ioread32(iowin);

	/* mask rest and access bit 16-23 */
	maxirq = (versn >> 16) & 0x00FF;
	maxirq = maxirq + 1;

	printk("\n%25s", " ");
	printk("APIC version :%08x\nRedirection-Table entries:%08x \n", versn, maxirq);

	for (i = 0, j = 0; i < maxirq; i++, j++) {
		iowrite32(ioredtlb[j], ioregsel);
		val_lo = ioread32(iowin);

		j++;
		iowrite32(ioredtlb[j], ioregsel);
		val_hi = ioread32(iowin);

		if ((i % 3) == 0)
			printk("\n");
		printk("  0x%02X : ", i);
		printk("%08X%08X  ", val_hi, val_lo);
	}

	return 0;

}

void cleanup_module(void)
{
	iounmap(io);
}

MODULE_LICENSE("GPL");
