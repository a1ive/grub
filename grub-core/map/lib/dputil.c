/*
 *  UEFI example
 *  Copyright (C) 2019  a1ive
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>
#include <grub.h>
#include <sstdlib.h>
#include <private.h>

static UINT64
WriteUnaligned64 (
  OUT UINT64                    *Buffer,
  IN  UINT64                    Value
  )
{
  return *Buffer = Value;
}

static UINT64
ReadUnaligned64 (
  IN CONST UINT64              *Buffer
  )
{
  return *Buffer;
}

CHAR16 *
StrStr (
  IN      CONST CHAR16              *String,
  IN      CONST CHAR16              *SearchString
  )
{
  CONST CHAR16 *FirstMatch;
  CONST CHAR16 *SearchStringTmp;

  if (*SearchString == L'\0') {
    return (CHAR16 *) String;
  }

  while (*String != L'\0') {
    SearchStringTmp = SearchString;
    FirstMatch = String;

    while ((*String == *SearchStringTmp)
            && (*String != L'\0')) {
      String++;
      SearchStringTmp++;
    }

    if (*SearchStringTmp == L'\0') {
      return (CHAR16 *) FirstMatch;
    }

    if (*String == L'\0') {
      return NULL;
    }

    String = FirstMatch + 1;
  }

  return NULL;
}

EFI_GUID *
CopyGuid (
  OUT EFI_GUID       *DestinationGuid,
  IN CONST EFI_GUID  *SourceGuid
  )
{
  WriteUnaligned64 (
    (UINT64*)DestinationGuid,
    ReadUnaligned64 ((CONST UINT64*)SourceGuid)
    );
  WriteUnaligned64 (
    (UINT64*)DestinationGuid + 1,
    ReadUnaligned64 ((CONST UINT64*)SourceGuid + 1)
    );
  return DestinationGuid;
}

EFI_DEVICE_PATH_PROTOCOL *
CreateDeviceNode (
  IN UINT8                           NodeType,
  IN UINT8                           NodeSubType,
  IN UINT16                          NodeLength
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;

  if (NodeLength < sizeof (EFI_DEVICE_PATH_PROTOCOL)) {
    return NULL;
  }

  DevicePath = AllocateZeroPool (NodeLength);
  if (DevicePath != NULL) {
     DevicePath->Type    = NodeType;
     DevicePath->SubType = NodeSubType;
     SetDevicePathNodeLength (DevicePath, NodeLength);
  }

  return DevicePath;
}
