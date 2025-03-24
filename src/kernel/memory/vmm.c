#include <bootloader.h>
#include <paging.h>
#include <pmm.h>
#include <system.h>
#include <task.h>
#include <util.h>
#include <vmm.h>

// Virtual memory space manager/allocator
// Copyright (C) 2024 Panagiotis

#define VMM_DEBUG 0

// HHDM might be mapped as a full-GB entry, so we have to be careful!
#define VMM_POS_ENSURE 0x40000000

void initiateVMM() {
  size_t targetPosition =
      DivRoundUp(bootloader.hhdmOffset - bootloader.mmTotal - VMM_POS_ENSURE,
                 PAGE_SIZE) *
      PAGE_SIZE;

  virtual.ready = false;
  virtual.mem_start = targetPosition;
  virtual.BitmapSizeInBlocks = DivRoundUp(bootloader.mmTotal, BLOCK_SIZE);
  virtual.BitmapSizeInBytes = DivRoundUp(virtual.BitmapSizeInBlocks, 8);

  uint64_t pagesRequired = DivRoundUp(virtual.BitmapSizeInBytes, BLOCK_SIZE);
  virtual.Bitmap = (uint8_t *)VirtualAllocate(pagesRequired);
  memset(virtual.Bitmap, 0, virtual.BitmapSizeInBytes);

  // should NEVER get put inside (since it's on the HHDM region)
  // MarkRegion(&virtual, virtual.Bitmap, virtual.BitmapSizeInBytes, 1);

  virtual.ready = true;
}

void *VirtualAllocate(int pages) {
  size_t phys = PhysicalAllocate(pages);

  uint64_t output = phys + bootloader.hhdmOffset;
#if VMM_DEBUG
  debugf("[vmm::alloc] Found region: out{%lx} phys{%lx}\n", output, phys);
#endif
  return (void *)(output);
}

// it's all contiguous already!
void *VirtualAllocatePhysicallyContiguous(int pages) {
  return VirtualAllocate(pages);
}

bool VirtualFree(void *ptr, int pages) {
  size_t phys = VirtualToPhysical((size_t)ptr);
  if (!phys) {
    debugf("[vmm::free] Could not find physical address! virt{%lx}\n", ptr);
    panic();
  }

  PhysicalFree(phys, pages);
  return true;
}
