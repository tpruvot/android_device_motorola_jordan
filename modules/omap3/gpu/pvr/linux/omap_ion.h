/*
 * arch/arm/mach-omap2/omap_ion.h
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _OMAP_ION_H
#define _OMAP_ION_H


#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 90)
#define OMAP4_ION_HEAP_TILER_SIZE		(SZ_128M - SZ_32M)
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	SZ_32M

#define OMAP3_ION_HEAP_CARVEOUT_INPUT_SIZE       (SZ_1M * 100)

#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM	(0x80c00000 + SZ_256M + SZ_128M - PHYS_ADDR_SMC_SIZE)
#define PHYS_ADDR_DUCATI_SIZE	(SZ_1M * 105)
#define PHYS_ADDR_DUCATI_MEM	(PHYS_ADDR_SMC_MEM - PHYS_ADDR_DUCATI_SIZE - \
				OMAP4_ION_HEAP_SECURE_INPUT_SIZE)


#include <linux/types.h>
//#include <linux/dsscomp.h>

/**
 * struct omap_ion_tiler_alloc_data - metadata passed from userspace for allocations
 * @w:          width of the allocation
 * @h:          height of the allocation
 * @fmt:        format of the data (8, 16, 32bit or page)
 * @flags:      flags passed to heap
 * @stride:     stride of the allocation, returned to caller from kernel
 * @handle:     pointer that will be populated with a cookie to use to refer
 *              to this allocation
 *
 * Provided by userspace as an argument to the ioctl
 */
struct omap_ion_tiler_alloc_data {
        size_t w;
        size_t h;
        int fmt;
        unsigned int flags;
        struct ion_handle *handle;
        size_t stride;
        size_t offset;
};

#ifdef __KERNEL__
int omap_ion_tiler_alloc(struct ion_client *client,
                         struct omap_ion_tiler_alloc_data *data);
int omap_ion_nonsecure_tiler_alloc(struct ion_client *client,
                         struct omap_ion_tiler_alloc_data *data);
/* given a handle in the tiler, return a list of tiler pages that back it */
int omap_tiler_pages(struct ion_client *client, struct ion_handle *handle,
                     int *n, u32 ** tiler_pages);
#endif /* __KERNEL__ */

#if defined(CONFIG_ION_OMAP)
int omap_ion_mem_alloc(struct ion_client *client,
                        struct omap_ion_tiler_alloc_data *data);
int omap_ion_get_pages(struct ion_client *client, struct ion_handle *handle,
                        int *n, unsigned long *ion_addr,
                        struct omap_ion_tiler_alloc_data *sAllocData);
#ifdef CONFIG_OMAP2_VRFB
void omap_get_vrfb_buffer(__u32 paddr);
void omap_free_vrfb_buffer(__u32 paddr);
//int omap_setup_vrfb_buffer(struct dss2_ovl_info *ovl_info);
#endif /* CONFIG_OMAP2_VRFB */

#else
static int omap_ion_mem_alloc(struct ion_client *client,
                        struct omap_ion_tiler_alloc_data *data)
{
	return -1;
}
int omap_ion_get_pages(struct ion_client *client, struct ion_handle *handle,
                        int *n, unsigned long *ion_addr,
                        struct omap_ion_tiler_alloc_data *sAllocData)
{
	return -1;
}
#endif

/* additional heaps used only on omap */
enum {
    OMAP_ION_HEAP_TYPE_TILER = ION_HEAP_TYPE_CUSTOM + 1,
};

#define OMAP_ION_HEAP_TILER_MASK (1 << OMAP_ION_HEAP_TYPE_TILER)

enum {
    OMAP_ION_TILER_ALLOC,
};

/**
 * These should match the defines in the tiler driver
 */
enum {
    TILER_PIXEL_FMT_MIN   = 0,
    TILER_PIXEL_FMT_8BIT  = 0,
    TILER_PIXEL_FMT_16BIT = 1,
    TILER_PIXEL_FMT_32BIT = 2,
    TILER_PIXEL_FMT_PAGE  = 3,
    TILER_PIXEL_FMT_MAX   = 3
};

/**
 * List of heaps in the system
 */
enum {
    OMAP_ION_HEAP_LARGE_SURFACES,
    OMAP_ION_HEAP_TILER,
    OMAP_ION_HEAP_SECURE_INPUT,
    OMAP_ION_HEAP_NONSECURE_TILER,
};

//#ifdef CONFIG_ION_OMAP
//int omap_ion_init(void);
//int omap_register_ion(void);
//#else
//static inline void omap_ion_init(void) { return 0; }
//static inline void omap_register_ion(void) { return 0; }
//#endif

#endif
