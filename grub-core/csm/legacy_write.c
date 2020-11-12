/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2020  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/pci.h>

#include "legacy_region.h"
#include "bios.h"

static grub_efi_guid_t acpi_guid = GRUB_EFI_ACPI_TABLE_GUID;
static grub_efi_guid_t acpi2_guid = GRUB_EFI_ACPI_20_TABLE_GUID;
static grub_efi_guid_t smbios_guid = GRUB_EFI_SMBIOS_TABLE_GUID;
static grub_efi_guid_t smbios3_guid = GRUB_EFI_SMBIOS3_TABLE_GUID;

static grub_efi_boolean_t
write_test (grub_efi_physical_address_t addr)
{
  grub_efi_boolean_t ret;
  grub_efi_uint8_t *test;
  grub_efi_uint8_t  tmp;
  test = (grub_efi_uint8_t *) (addr);
  tmp = *test;
  *test = *test + 1;
  ret = (tmp != *test);
  *test = tmp;
  return ret;
}

void
bios_enable_rom_area (void)
{
  grub_pci_address_t addr;
  grub_pci_device_t dev = { .bus = 0, .device = 0, .function = 0};

  /* FIXME: should be macroified.  */
  addr = grub_pci_make_address (dev, 144);
  grub_pci_write_byte (addr++, 0x30);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr++, 0x33);
  grub_pci_write_byte (addr, 0);
}

void
bios_lock_rom_area (void)
{
  grub_pci_address_t addr;
  grub_pci_device_t dev = { .bus = 0, .device = 0, .function = 0};

  /* FIXME: should be macroified.  */
  addr = grub_pci_make_address (dev, 144);
  grub_pci_write_byte (addr++, 0x10);
  grub_pci_write_byte (addr++, 0x11);
  grub_pci_write_byte (addr++, 0x11);
  grub_pci_write_byte (addr++, 0x11);
  grub_pci_write_byte (addr, 0x11);
}

grub_err_t
bios_mem_lock (grub_efi_physical_address_t start,
               grub_efi_uint32_t len, grub_efi_boolean_t lock)
{
  grub_efi_status_t status = GRUB_EFI_NOT_READY;
  grub_efi_uint32_t granularity;
  legacy_region_protocol_t *legacy_region = NULL;
  legacy_region2_protocol_t *legacy_region2 = NULL;
  grub_efi_guid_t legacy_region_guid = EFI_LEGACY_REGION_PROTOCOL_GUID;
  grub_efi_guid_t legacy_region2_guid = EFI_LEGACY_REGION2_PROTOCOL_GUID;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;

  if (!lock && write_test (start))
  {
    grub_printf ("Memory at 0x%lx already unlocked\n", (unsigned long) start);
    status = GRUB_EFI_SUCCESS;
  }
  else if (lock && !write_test (start))
  {
    grub_printf ("Memory at 0x%lx already locked\n", (unsigned long)start);
    status = GRUB_EFI_SUCCESS;
  }

  if (status != GRUB_EFI_SUCCESS)
  {
    status = efi_call_3 (b->locate_protocol, &legacy_region_guid,
                         NULL, (void **)&legacy_region);
    if (status == GRUB_EFI_SUCCESS)
    {
      if (!lock)
      {
        status = legacy_region->unlock (legacy_region, (grub_uint32_t) start,
                                        len, &granularity);
        status = write_test(start) ? GRUB_EFI_SUCCESS : GRUB_EFI_DEVICE_ERROR;
      }
      else
      {
        status = legacy_region->lock (legacy_region, (grub_uint32_t) start,
                                      len, &granularity);
        status = write_test(start) ? GRUB_EFI_DEVICE_ERROR : GRUB_EFI_SUCCESS;
      }

      grub_printf ("%s %s memory at 0x%x with LegacyRegionProtocol\n",
                   status != GRUB_EFI_SUCCESS ? "Failure" : "Success",
                   lock ? "locking" : "unlocking", (grub_uint32_t) start);
    }
  }

  if (status != GRUB_EFI_SUCCESS)
  {
    status = efi_call_3 (b->locate_protocol, &legacy_region2_guid,
                         NULL, (void **)&legacy_region2);
    if (status == GRUB_EFI_SUCCESS)
    {
      if (!lock)
      {
        status = legacy_region2->unlock (legacy_region2, (grub_uint32_t) start,
                                         len, &granularity);
        status = write_test(start) ? GRUB_EFI_SUCCESS : GRUB_EFI_DEVICE_ERROR;
      }
      else
      {
        status = legacy_region2->lock (legacy_region2, (grub_uint32_t) start,
                                       len, &granularity);
        status = write_test(start) ? GRUB_EFI_DEVICE_ERROR : GRUB_EFI_SUCCESS;
      }

      grub_printf ("%s %s memory at 0x%x with LegacyRegion2Protocol\n",
                   status != GRUB_EFI_SUCCESS ? "Failure" : "Success",
                   lock ? "locking" : "unlocking", (grub_uint32_t) start);
    }
  }

  /* TODO: lock/unlock via an MTRR. */
  if (status != GRUB_EFI_SUCCESS)
  {
    if (!lock)
    {
      bios_enable_rom_area ();
      status = write_test(start) ? GRUB_EFI_SUCCESS : GRUB_EFI_DEVICE_ERROR;
    }
    else
    {
      bios_lock_rom_area ();
      status = write_test(start) ? GRUB_EFI_DEVICE_ERROR : GRUB_EFI_SUCCESS;
    }

    grub_printf ("%s %s memory at 0x%x with grub_pci_write\n",
                   status != GRUB_EFI_SUCCESS ? "Failure" : "Success",
                   lock ? "locking" : "unlocking", (grub_uint32_t) start);
  }

  if (status != GRUB_EFI_SUCCESS)
    return grub_error (GRUB_ERR_BAD_OS, "Can't %s memory at 0x%x",
                       lock ? "lock" : "unlock", (grub_uint32_t) start);
  return GRUB_ERR_NONE;
}

grub_efi_boolean_t
bios_check_int10h (void)
{
  const grub_uint8_t PROTECTIVE_OPCODE_1 = 0xff;
  const grub_uint8_t PROTECTIVE_OPCODE_2 = 0x00;
  bios_ivt_entry_t *int10h;
  grub_efi_physical_address_t int10h_handler;
  grub_uint8_t opcode;

  int10h = (bios_ivt_entry_t *)(grub_addr_t) BIOS_IVT_ADDR + 0x10;
  int10h_handler = (int10h->segment << 4) + int10h->offset;

  if (int10h_handler >= BIOS_VGA_ROM_ADDR &&
      int10h_handler < (BIOS_VGA_ROM_ADDR + BIOS_VGA_ROM_SIZE))
  {
    grub_printf ("Int10h IVT entry points at location "
                 "within VGA ROM memory area (%04x:%04x)\n",
                 int10h->segment, int10h->offset);

    opcode = *((grub_uint8_t *) int10h_handler);
    if (opcode == PROTECTIVE_OPCODE_1 || opcode == PROTECTIVE_OPCODE_2)
    {
      grub_printf ("First Int10h handler instruction at %04x:%04x (%02x) "
                   "not valid, rejecting handler\n",
                   int10h->segment, int10h->offset, opcode);
      return FALSE;
    }
    else
    {
      grub_printf ("First Int10h handler instruction at %04x:%04x (%02x) "
                   "valid, accepting handler\n",
                   int10h->segment, int10h->offset, opcode);
      return TRUE;
    }
  }
  else
  {
    grub_printf ("Int10h IVT entry points at location (%04x:%04x) "
                 "outside VGA ROM memory area, rejecting handler\n",
                 int10h->segment, int10h->offset);
    return FALSE;
  }
}

void
bios_fake_data (int use_rom)
{
  unsigned i;
  void *acpi, *smbios, *smbios3;
  grub_uint16_t *ebda_seg_ptr, *low_mem_ptr;

  ebda_seg_ptr = (grub_uint16_t *) EBDA_SEG_ADDR;
  low_mem_ptr = (grub_uint16_t *) LOW_MEM_ADDR;
  if ((*ebda_seg_ptr) || (*low_mem_ptr))
    return;

  acpi = 0;
  smbios = 0;
  smbios3 = 0;
  for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
  {
    grub_efi_packed_guid_t *guid =
      &grub_efi_system_table->configuration_table[i].vendor_guid;

    if (! grub_memcmp (guid, &acpi2_guid, sizeof (grub_efi_guid_t)))
    {
      acpi = grub_efi_system_table->configuration_table[i].vendor_table;
      grub_dprintf ("efi", "ACPI2: %p\n", acpi);
    }
    else if (! grub_memcmp (guid, &acpi_guid, sizeof (grub_efi_guid_t)))
    {
      void *t;

      t = grub_efi_system_table->configuration_table[i].vendor_table;
      if (! acpi)
        acpi = t;
      grub_dprintf ("efi", "ACPI: %p\n", t);
    }
    else if (! grub_memcmp (guid, &smbios_guid, sizeof (grub_efi_guid_t)))
    {
      smbios = grub_efi_system_table->configuration_table[i].vendor_table;
      grub_dprintf ("efi", "SMBIOS: %p\n", smbios);
    }
    else if (! grub_memcmp (guid, &smbios3_guid, sizeof (grub_efi_guid_t)))
    {
      smbios3 = grub_efi_system_table->configuration_table[i].vendor_table;
      grub_dprintf ("efi", "SMBIOS3: %p\n", smbios3);
    }
  }

  *ebda_seg_ptr = FAKE_EBDA_SEG;
  *low_mem_ptr = (FAKE_EBDA_SEG >> 6);

  *((grub_uint16_t *) (FAKE_EBDA_SEG << 4)) = 640 - *low_mem_ptr;

  if (acpi)
    grub_memcpy ((char *) ((FAKE_EBDA_SEG << 4) + 16), acpi, 1024 - 16);

  if (use_rom)
  {
    if (smbios)
      grub_memcpy ((char *) SBIOS_ADDR, (char *) smbios, 31);
    if (smbios3)
      grub_memcpy ((char *) SBIOS_ADDR + 32, (char *) smbios3, 24);
  }
}
