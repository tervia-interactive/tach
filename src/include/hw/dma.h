/* hw/dma.h - DMA controller abstraction
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HW_DMA_H
#define _HW_DMA_H

#include <kernel/types.h>

struct dma_channel {
    uint8_t channel;
    uint32_t base_address;
    uint32_t count;
    uint32_t flags;
};

void dma_init(void);
int dma_request_channel(uint32_t flags);
void dma_free_channel(int channel);
int dma_setup_transfer(int channel, void* buffer, size_t size, int direction);
void dma_start(int channel);
void dma_stop(int channel);
int dma_is_complete(int channel);
phys_addr_t dma_map_buffer(void* buffer, size_t size);
void dma_unmap_buffer(phys_addr_t phys, size_t size);

#endif /* _HW_DMA_H */
