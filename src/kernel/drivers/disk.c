#include <ahci.h>
#include <disk.h>
#include <malloc.h>
#include <system.h>
#include <util.h>

// Multiple disk handler
// Copyright (C) 2022 Panagiotis

uint16_t mbr_partition_indexes[] = {MBR_PARTITION_1, MBR_PARTITION_2,
                                    MBR_PARTITION_3, MBR_PARTITION_4};

bool openDisk(uint32_t disk, uint8_t partition, mbr_partition *out) {
  uint8_t *rawArr = (uint8_t *)malloc(SECTOR_SIZE);
  getDiskBytes(rawArr, 0x0, 1);
  // *out = *(mbr_partition *)(&rawArr[mbr_partition_indexes[partition]]);
  bool ret = validateMbr(rawArr);
  if (!ret)
    return false;
  memcpy(out, (void *)((size_t)rawArr + mbr_partition_indexes[partition]),
         sizeof(mbr_partition));
  free(rawArr);
  return true;
}

bool validateMbr(uint8_t *mbrSector) {
  return mbrSector[510] == 0x55 && mbrSector[511] == 0xaa;
}

void diskBytes(uint8_t *target_address, uint32_t LBA, uint32_t sector_count,
               bool write) {
  // todo: yeah, this STILL is NOT ideal

  PCI *browse = firstPCI;
  while (browse) {
    if (browse->driver == PCI_DRIVER_AHCI && ((ahci *)browse->extra)->sata)
      break;

    browse = browse->next;
  }

  if (!browse) {
    memset(target_address, 0, sector_count * SECTOR_SIZE);
    return;
  }

  ahci *target = (ahci *)browse->extra;
  int   pos = 0;
  while (!(target->sata & (1 << pos)))
    pos++;

  (write ? ahciWrite : ahciRead)(target, pos, &target->mem->ports[pos], LBA, 0,
                                 sector_count, target_address);
}

// todo: allow concurrent stuff
force_inline void diskBytesMax(uint8_t *target_address, uint32_t LBA,
                               size_t sector_count, bool write) {
  int prdtAmnt = AHCI_PRDTS;

  if (!IS_ALIGNED((size_t)target_address, 0x1000))
    prdtAmnt--; // we later fill in for head

  // calculated by: (bytesPerPRDT * PRDTamnt) / SECTOR_SIZE
  //                (    4MiB     *     8   ) /     512
  size_t max = (AHCI_BYTES_PER_PRDT * prdtAmnt) / SECTOR_SIZE;

  size_t chunks = sector_count / max;
  size_t remainder = sector_count % max;
  if (chunks)
    for (size_t i = 0; i < chunks; i++)
      diskBytes(target_address + i * max * SECTOR_SIZE, LBA + i * max, max,
                write);

  if (remainder)
    diskBytes(target_address + chunks * max * SECTOR_SIZE, LBA + chunks * max,
              remainder, write);

  // for (int i = 0; i < sector_count; i++)
  //   diskBytes(target_address + i * 512, LBA + i, 1, false);

  // diskBytes(target_address, LBA, sector_count, false);
}

void getDiskBytes(uint8_t *target_address, uint32_t LBA, size_t sector_count) {
  return diskBytesMax(target_address, LBA, sector_count, false);
}

void setDiskBytes(const uint8_t *target_address, uint32_t LBA,
                  size_t sector_count) {
  // bad solution but idc, my code is safe
  uint8_t *rw_target_address = (uint8_t *)((size_t)target_address);
  return diskBytesMax(rw_target_address, LBA, sector_count, true);
}
