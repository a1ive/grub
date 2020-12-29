/* mdraid_linux.c - module to handle linux softraid.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
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
#include <grub/disk.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/raid.h>

/* Linux RAID on disk structures and constants,
   copied from include/linux/raid/md_p.h.  */

#define RESERVED_BYTES			(64 * 1024)
#define RESERVED_SECTORS		(RESERVED_BYTES / 512)

#define NEW_SIZE_SECTORS(x)		((x & ~(RESERVED_SECTORS - 1)) \
					- RESERVED_SECTORS)

#define SB_BYTES			4096
#define SB_WORDS			(SB_BYTES / 4)
#define SB_SECTORS			(SB_BYTES / 512)

/*
 * The following are counted in 32-bit words
 */
#define	SB_GENERIC_OFFSET		0

#define SB_PERSONALITY_OFFSET		64
#define SB_DISKS_OFFSET			128
#define SB_DESCRIPTOR_OFFSET		992

#define SB_GENERIC_CONSTANT_WORDS	32
#define SB_GENERIC_STATE_WORDS		32
#define SB_GENERIC_WORDS		(SB_GENERIC_CONSTANT_WORDS + \
                                         SB_GENERIC_STATE_WORDS)

#define SB_PERSONALITY_WORDS		64
#define SB_DESCRIPTOR_WORDS		32
#define SB_DISKS			27
#define SB_DISKS_WORDS			(SB_DISKS * SB_DESCRIPTOR_WORDS)

#define SB_RESERVED_WORDS		(1024 \
                                         - SB_GENERIC_WORDS \
                                         - SB_PERSONALITY_WORDS \
                                         - SB_DISKS_WORDS \
                                         - SB_DESCRIPTOR_WORDS)

#define SB_EQUAL_WORDS			(SB_GENERIC_WORDS \
                                         + SB_PERSONALITY_WORDS \
                                         + SB_DISKS_WORDS)

/*
 * Device "operational" state bits
 */
#define DISK_FAULTY			0
#define DISK_ACTIVE			1
#define DISK_SYNC			2
#define DISK_REMOVED			3

#define	DISK_WRITEMOSTLY		9

#define SB_MAGIC			0xa92b4efc

/*
 * Superblock state bits
 */
#define SB_CLEAN			0
#define SB_ERRORS			1

#define	SB_BITMAP_PRESENT		8

struct grub_raid_disk_09
{
  grub_uint32_t number;		/* Device number in the entire set.  */
  grub_uint32_t major;		/* Device major number.  */
  grub_uint32_t minor;		/* Device minor number.  */
  grub_uint32_t raid_disk;	/* The role of the device in the raid set.  */
  grub_uint32_t state;		/* Operational state.  */
  grub_uint32_t reserved[SB_DESCRIPTOR_WORDS - 5];
};

struct grub_raid_super_09
{
  /*
   * Constant generic information
   */
  grub_uint32_t md_magic;	/* MD identifier.  */
  grub_uint32_t major_version;	/* Major version.  */
  grub_uint32_t minor_version;	/* Minor version.  */
  grub_uint32_t patch_version;	/* Patchlevel version.  */
  grub_uint32_t gvalid_words;	/* Number of used words in this section.  */
  grub_uint32_t set_uuid0;	/* Raid set identifier.  */
  grub_uint32_t ctime;		/* Creation time.  */
  grub_uint32_t level;		/* Raid personality.  */
  grub_uint32_t size;		/* Apparent size of each individual disk.  */
  grub_uint32_t nr_disks;	/* Total disks in the raid set.  */
  grub_uint32_t raid_disks;	/* Disks in a fully functional raid set.  */
  grub_uint32_t md_minor;	/* Preferred MD minor device number.  */
  grub_uint32_t not_persistent;	/* Does it have a persistent superblock.  */
  grub_uint32_t set_uuid1;	/* Raid set identifier #2.  */
  grub_uint32_t set_uuid2;	/* Raid set identifier #3.  */
  grub_uint32_t set_uuid3;	/* Raid set identifier #4.  */
  grub_uint32_t gstate_creserved[SB_GENERIC_CONSTANT_WORDS - 16];

  /*
   * Generic state information
   */
  grub_uint32_t utime;		/* Superblock update time.  */
  grub_uint32_t state;		/* State bits (clean, ...).  */
  grub_uint32_t active_disks;	/* Number of currently active disks.  */
  grub_uint32_t working_disks;	/* Number of working disks.  */
  grub_uint32_t failed_disks;	/* Number of failed disks.  */
  grub_uint32_t spare_disks;	/* Number of spare disks.  */
  grub_uint32_t sb_csum;	/* Checksum of the whole superblock.  */
  grub_uint64_t events;		/* Superblock update count.  */
  grub_uint64_t cp_events;	/* Checkpoint update count.  */
  grub_uint32_t recovery_cp;	/* Recovery checkpoint sector count.  */
  grub_uint32_t gstate_sreserved[SB_GENERIC_STATE_WORDS - 12];

  /*
   * Personality information
   */
  grub_uint32_t layout;		/* The array's physical layout.  */
  grub_uint32_t chunk_size;	/* Chunk size in bytes.  */
  grub_uint32_t root_pv;	/* LV root PV.  */
  grub_uint32_t root_block;	/* LV root block.  */
  grub_uint32_t pstate_reserved[SB_PERSONALITY_WORDS - 4];

  /*
   * Disks information
   */
  struct grub_raid_disk_09 disks[SB_DISKS];

  /*
   * Reserved
   */
  grub_uint32_t reserved[SB_RESERVED_WORDS];

  /*
   * Active descriptor
   */
  struct grub_raid_disk_09 this_disk;
} __attribute__ ((packed));

struct grub_raid_super_1
{
  grub_uint32_t md_magic;	/* MD identifier.  */
  grub_uint32_t major_version;	/* Major version.  */
  grub_uint32_t feature_map;
  grub_uint32_t pad0;
  grub_uint8_t set_uuid[16];
  char set_name[32];
  grub_uint64_t ctime;          /* Creation time. low 40 bits are seconds,  */
				/* top 24 are microseconds or 0.  */
  grub_uint32_t level;		/* Raid personality.  */
  grub_uint32_t layout;		/* Only for raid5 currently.  */
  grub_uint64_t size;		/* Apparent size of each individual disk.  */
  grub_uint32_t chunk_size;	/* In 512 byte sectors.  */
  grub_uint32_t raid_disks;	/* Disks in a fully functional raid set.  */
  grub_uint32_t bitmap_offset;
  grub_uint32_t new_level;	/* New level we are reshaping to.  */
  grub_uint64_t reshape_position;
  grub_uint32_t delta_disks;
  grub_uint32_t new_layout;
  grub_uint32_t new_chunk;
  grub_uint8_t pad1[128 - 124];

  grub_uint64_t data_offset;
  grub_uint64_t data_size;
  grub_uint64_t super_offset;
  grub_uint64_t recovery_offset;
  grub_uint32_t dev_number;
  grub_uint32_t cnt_corrected_read;
  grub_uint8_t device_uuid[16];
  grub_uint8_t devflags;
  grub_uint8_t pad2[64 - 57];

  grub_uint64_t utime;
  grub_uint64_t events;
  grub_uint64_t rsync_offset;
  grub_uint32_t sb_csum;
  grub_uint32_t max_dev;
  grub_uint8_t pad3[64 - 32];

  grub_uint16_t dev_roles[0];
} __attribute__ ((packed));

static int
detect_super_1 (grub_disk_t disk, grub_disk_addr_t sector,
		struct grub_raid_super_1 *sb)
{
  if (grub_disk_read (disk, sector, 0, sizeof (*sb), sb))
    return 0;

  if (grub_le_to_cpu32 (sb->md_magic) != SB_MAGIC)
    return 0;

  if (grub_le_to_cpu32 (sb->major_version) != 1)
    return 0;

  if (grub_le_to_cpu64 (sb->super_offset) != sector)
    return 0;

  return 1;
}

static int
grub_mdraid_detect_1 (grub_disk_t disk, struct grub_raid_array *array)
{
  struct grub_raid_super_1 sb;
  grub_disk_addr_t sector, best_offset;
  grub_uint64_t best_ctime;
  int i, best_ver;

  sector = grub_disk_get_size (disk);
  if (sector < 24)
    return 0;

  best_offset = 0;
  best_ctime = 0;
  best_ver = -1;
  for (i = 0; i <= 2; i++)
    {
      grub_disk_addr_t offset;

      if (i == 0)
	offset = (sector - 16) & ~7ULL;
      else if (i == 1)
	offset = 0;
      else
	offset = 8;

      if (detect_super_1 (disk, offset, &sb))
	{
	  if ((best_ver < 0) ||
	      (best_ctime < grub_le_to_cpu64 (sb.ctime)))
	    {
	      best_ver = i;
	      best_ctime = grub_le_to_cpu64 (sb.ctime);
	      best_offset = offset;
	    }
	}

      if (grub_errno)
	return 0;
    }

  if (best_ver < 0)
    return 0;

  if (grub_disk_read (disk, best_offset, 0, sizeof (sb), &sb))
    return 0;

  array->number = 0;
  array->level = grub_le_to_cpu32 (sb.level);
  array->layout = grub_le_to_cpu32 (sb.layout);
  array->total_devs = grub_le_to_cpu32 (sb.raid_disks);
  array->disk_size = grub_le_to_cpu64 (sb.data_size);
  array->disk_offset = grub_le_to_cpu64 (sb.data_offset);
  array->chunk_size = grub_le_to_cpu32 (sb.chunk_size);
  array->index = grub_le_to_cpu32 (sb.dev_number);
  array->uuid_len = sizeof (sb.set_uuid);
  array->uuid = grub_malloc (sizeof (sb.set_uuid));
  if (!array->uuid)
    return 0;

  grub_memcpy (array->uuid, &sb.set_uuid, sizeof (sb.set_uuid));

  return 1;
}

static grub_err_t
grub_mdraid_detect_09 (grub_disk_t disk, struct grub_raid_array *array)
{
  grub_disk_addr_t sector;
  grub_uint64_t size;
  struct grub_raid_super_09 sb;
  grub_uint32_t *uuid;

  /* The sector where the RAID superblock is stored, if available. */
  size = grub_disk_get_size (disk);
  sector = NEW_SIZE_SECTORS (size);

  if (grub_disk_read (disk, sector, 0, SB_BYTES, &sb))
    return grub_errno;

  /* Look whether there is a RAID superblock. */
  if (sb.md_magic != SB_MAGIC)
    return grub_error (GRUB_ERR_OUT_OF_RANGE, "not raid");

  /* FIXME: Also support version 1.0. */
  if (sb.major_version != 0 || sb.minor_version != 90)
    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		       "unsupported RAID version: %d.%d",
		       sb.major_version, sb.minor_version);

  /* FIXME: Check the checksum. */

  /* Multipath.  */
  if ((int) sb.level == -4)
    sb.level = 1;

  if (sb.level != 0 && sb.level != 1 && sb.level != 4 &&
      sb.level != 5 && sb.level != 6 && sb.level != 10)
    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		       "unsupported RAID level: %d", sb.level);

  array->number = sb.md_minor;
  array->level = sb.level;
  array->layout = sb.layout;
  array->total_devs = sb.raid_disks;
  array->disk_size = (sb.size) ? sb.size * 2 : sector;
  array->disk_offset = 0;
  array->chunk_size = sb.chunk_size >> 9;
  array->index = sb.this_disk.number;
  array->uuid_len = 16;
  array->uuid = grub_malloc (16);
  if (!array->uuid)
    return grub_errno;

  uuid = (grub_uint32_t *) array->uuid;
  uuid[0] = sb.set_uuid0;
  uuid[1] = sb.set_uuid1;
  uuid[2] = sb.set_uuid2;
  uuid[3] = sb.set_uuid3;

  return 0;
}

static grub_err_t
grub_mdraid_detect (grub_disk_t disk, struct grub_raid_array *array)
{
  if (grub_mdraid_detect_1 (disk, array))
    return 0;

  if (grub_errno)
    return grub_errno;

  return grub_mdraid_detect_09 (disk, array);
}

static struct grub_raid grub_mdraid_dev = {
  .name = "mdraid",
  .detect = grub_mdraid_detect,
  .next = 0
};

GRUB_MOD_INIT (mdraid)
{
  grub_raid_register (&grub_mdraid_dev);
}

GRUB_MOD_FINI (mdraid)
{
  grub_raid_unregister (&grub_mdraid_dev);
}
