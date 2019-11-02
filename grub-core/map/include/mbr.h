/** @file
  Legacy Master Boot Record Format Definition.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _MBR_H_
#define _MBR_H_

#include <efi.h>

#define MBR_SIGNATURE               0xaa55

#define EXTENDED_DOS_PARTITION      0x05
#define EXTENDED_WINDOWS_PARTITION  0x0F

#define MAX_MBR_PARTITIONS          4

#define PMBR_GPT_PARTITION          0xEE

#define MBR_SIZE                    512

#define SIGNATURE_16(A, B)        ((A) | (B << 8))

#define SIGNATURE_32(A, B, C, D)  (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))

#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
    (SIGNATURE_32 (A, B, C, D) | ((UINT64) (SIGNATURE_32 (E, F, G, H)) << 32))

#define _EFI_PTAB_HEADER_ID SIGNATURE_64 ('E','F','I',' ','P','A','R','T')

#endif
