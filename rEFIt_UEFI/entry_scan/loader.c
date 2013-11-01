/*
 * refit/scan/loader.c
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "entry_scan.h"

#ifndef DEBUG_ALL
#define DEBUG_SCAN_LOADER 1
#else
#define DEBUG_SCAN_LOADER DEBUG_ALL
#endif

#if DEBUG_SCAN_LOADER == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SCAN_LOADER, __VA_ARGS__)
#endif

#define MACOSX_LOADER_PATH L"\\System\\Library\\CoreServices\\boot.efi"

#define LINUX_LOADER_PATH L"\\boot\\vmlinuz"
#define LINUX_DEFAULT_OPTIONS L"quiet splash"

#if defined(MDE_CPU_X64)
#define BOOT_LOADER_PATH L"\\EFI\\BOOT\\BOOTX64.efi"
#else
#define BOOT_LOADER_PATH L"\\EFI\\BOOT\\BOOTIA32.efi"
#endif

// Linux loader path data
typedef struct LINUX_PATH_DATA
{
   CHAR16 *Path;
   CHAR16 *Title;
   CHAR16 *Icon;
} LINUX_PATH_DATA;

STATIC LINUX_PATH_DATA LinuxEntryData[] = {
#if defined(MDE_CPU_X64)
  { L"\\EFI\\grub\\grubx64.efi", L"Grub EFI boot menu", L"grub,linux" },
  { L"\\EFI\\Gentoo\\grubx64.efi", L"Gentoo EFI boot menu", L"gentoo,linux" },
  { L"\\EFI\\Gentoo\\kernelx64.efi", L"Gentoo EFI kernel", L"gentoo,linux" },
  { L"\\EFI\\RedHat\\grubx64.efi", L"RedHat EFI boot menu", L"rehat,linux" },
  { L"\\EFI\\ubuntu\\grubx64.efi", L"Ubuntu EFI boot menu", L"ubuntu,linux" },
  { L"\\EFI\\kubuntu\\grubx64.efi", L"kubuntu EFI boot menu", L"kubuntu,linux" },
  { L"\\EFI\\LinuxMint\\grubx64.efi", L"Linux Mint EFI boot menu", L"mint,linux" },
  { L"\\EFI\\Fedora\\grubx64.efi", L"Fedora EFI boot menu", L"fedora,linux" },
  { L"\\EFI\\opensuse\\grubx64.efi", L"OpenSuse EFI boot menu", L"suse,linux" },
  { L"\\EFI\\arch\\grubx64.efi", L"ArchLinux EFI boot menu", L"arch,linux" },
  { L"\\EFI\\arch_grub\\grubx64.efi", L"ArchLinux EFI boot menu", L"arch,linux" },
#else
  { L"\\EFI\\grub\\grub.efi", L"Grub EFI boot menu", L"grub,linux" },
  { L"\\EFI\\Gentoo\\grub.efi", L"Gentoo EFI boot menu", L"gentoo,linux" },
  { L"\\EFI\\Gentoo\\kernel.efi", L"Gentoo EFI kernel", L"gentoo,linux" },
  { L"\\EFI\\RedHat\\grub.efi", L"RedHat EFI boot menu", L"rehat,linux" },
  { L"\\EFI\\ubuntu\\grub.efi", L"Ubuntu EFI boot menu", L"ubuntu,linux" },
  { L"\\EFI\\kubuntu\\grub.efi", L"kubuntu EFI boot menu", L"kubuntu,linux" },
  { L"\\EFI\\LinuxMint\\grub.efi", L"Linux Mint EFI boot menu", L"mint,linux" },
  { L"\\EFI\\Fedora\\grub.efi", L"Fedora EFI boot menu", L"fedora,linux" },
  { L"\\EFI\\opensuse\\grub.efi", L"OpenSuse EFI boot menu", L"suse,linux" },
  { L"\\EFI\\arch\\grub.efi", L"ArchLinux EFI boot menu", L"arch,linux" },
  { L"\\EFI\\arch_grub\\grub.efi", L"ArchLinux EFI boot menu", L"arch,linux" },
#endif
  { L"\\EFI\\SuSe\\elilo.efi", L"OpenSuse EFI boot menu", L"suse,linux" },
};
STATIC CONST UINTN LinuxEntryDataCount = (sizeof(LinuxEntryData) / sizeof(LINUX_PATH_DATA));

// OS X installer paths
STATIC CHAR16 *OSXInstallerPaths[] = {
  L"\\Mac OS X Install Data\\boot.efi",
  L"\\OS X Install Data\\boot.efi",
  L"\\.IABootFiles\\boot.efi"
};
STATIC CONST UINTN OSXInstallerPathsCount = (sizeof(OSXInstallerPaths) / sizeof(CHAR16 *));

#define TO_LOWER(ch) (((ch >= L'A') || (ch <= L'Z')) ? (ch - L'A' + L'a') : ch)
STATIC INTN StrniCmp(IN CHAR16 *Str1,
                     IN CHAR16 *Str2,
                     IN UINTN   Count)
{
  CHAR16 Ch1, Ch2;
  if (Count == 0) {
    return 0;
  }
  if (Str1 == NULL) {
    if (Str2 == NULL) {
      return 0;
    } else {
      return -1;
    }
  } else  if (Str2 == NULL) {
    return 1;
  }
  do {
    Ch1 = TO_LOWER(*Str1);
    Ch2 = TO_LOWER(*Str2);
    Str1++;
    Str2++;
  } while ((--Count > 0) && (Ch1 == Ch2) && (Ch1 != 0));
  return (Ch1 - Ch2);
}

UINT8 GetOSTypeFromPath(IN CHAR16 *Path)
{
  if (Path == NULL) {
    return OSTYPE_OTHER;
  }
  if (StriCmp(Path, MACOSX_LOADER_PATH) == 0) {
    return OSTYPE_OSX;
  } else if ((StriCmp(Path, L"\\OS X Install Data\\boot.efi") == 0) ||
             (StriCmp(Path, L"\\Mac OS X Install Data\\boot.efi") == 0) ||
             (StriCmp(Path, L"\\.IABootFiles\\boot.efi") == 0)) {
    return OSTYPE_OSX_INSTALLER;
  } else if (StriCmp(Path, L"\\com.apple.recovery.boot\\boot.efi") == 0) {
    return OSTYPE_RECOVERY;
  } else if ((StriCmp(Path, L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi") == 0) ||
             (StriCmp(Path, L"\\EFI\\Microsoft\\Boot\\bootmgfw-orig.efi") == 0) ||
             (StriCmp(Path, L"\\bootmgr.efi") == 0) ||
             (StriCmp(Path, L"\\EFI\\MICROSOFT\\BOOT\\cdboot.efi") == 0)) {
    return OSTYPE_WINEFI;
  } else if (StrniCmp(Path, LINUX_LOADER_PATH, StrLen(LINUX_LOADER_PATH)) == 0) {
    return OSTYPE_LINEFI;
  } else {
    UINTN Index = 0;
    while (Index < LinuxEntryDataCount) {
      if (StriCmp(Path, LinuxEntryData[Index].Path) == 0) {
        return OSTYPE_LIN;
      }
      ++Index;
    }
  }
  return OSTYPE_OTHER;
}

STATIC CHAR16 *LinuxIconNameFromPath(IN CHAR16 *Path)
{
  UINTN Index = 0;
  while (Index < LinuxEntryDataCount) {
    if (StriCmp(Path, LinuxEntryData[Index].Path) == 0) {
      return LinuxEntryData[Index].Icon;
    }
    ++Index;
  }
  return L"linux";
}

STATIC CHAR16 *LinuxInitImagePath[] = {
   L"initrd%s",
   L"initrd.img%s",
   L"initrd%s.img",
   L"initramfs%s",
   L"initramfs.img%s",
   L"initramfs%s.img",
};
STATIC CONST UINTN LinuxInitImagePathCount = (sizeof(LinuxInitImagePath) / sizeof(CHAR16 *));

STATIC CHAR16 *LinuxMatchInitImage(IN EFI_FILE_PROTOCOL *Dir,
                                   IN CHAR16            *Version)
{
   UINTN Index = 0;
   while (Index < LinuxInitImagePathCount) {
     CHAR16 *InitRd = PoolPrint(LinuxInitImagePath[Index++], Version);
     if (InitRd != NULL) {
       if (FileExists(Dir, InitRd)) {
         return InitRd;
       }
       FreePool(InitRd);
     }
   }
   return NULL;
}

STATIC BOOLEAN isFirstRootUUID(REFIT_VOLUME *Volume)
{
  UINTN         VolumeIndex;
  REFIT_VOLUME *scanedVolume;
  
  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    scanedVolume = Volumes[VolumeIndex];
    
    if ( scanedVolume == Volume)
      return TRUE;
    
    if (CompareGuid(&scanedVolume->RootUUID, &Volume->RootUUID))
      return FALSE;
    
  }
  return TRUE;
}

//Set Entry->VolName to .disk_label.contentDetails if it exists
STATIC EFI_STATUS GetOSXVolumeName(LOADER_ENTRY *Entry)
{
  EFI_STATUS	Status = EFI_NOT_FOUND;
  CHAR16* targetNameFile = L"System\\Library\\CoreServices\\.disk_label.contentDetails";
  CHAR8* 	fileBuffer;
  CHAR8*  targetString;
  UINTN   fileLen = 0;
  if(FileExists(Entry->Volume->RootDir, targetNameFile)) {
    Status = egLoadFile(Entry->Volume->RootDir, targetNameFile, (UINT8 **)&fileBuffer, &fileLen);
    if(!EFI_ERROR(Status)) {
      CHAR16  *tmpName;
      //Create null terminated string
      targetString = (CHAR8*) AllocateZeroPool(fileLen+1);
      CopyMem( (VOID*)targetString, (VOID*)fileBuffer, fileLen);
      
      //      NOTE: Sothor - This was never run. If we need this correct it and uncomment it.
      //      if (Entry->LoaderType == OSTYPE_OSX) {
      //        INTN i;
      //        //remove occurence number. eg: "vol_name 2" --> "vol_name"
      //        i=fileLen-1;
      //        while ((i>0) && (targetString[i]>='0') && (targetString[i]<='9')) {
      //          i--;
      //        }
      //        if (targetString[i] == ' ') {
      //          targetString[i] = 0;
      //        }
      //      }
      
      //Convert to Unicode
      tmpName = (CHAR16*)AllocateZeroPool((fileLen+1)*2);
      tmpName = AsciiStrToUnicodeStr(targetString, tmpName);
      
      Entry->VolName = EfiStrDuplicate(tmpName);
      
      FreePool(tmpName);
      FreePool(fileBuffer);
      FreePool(targetString);
    }
  }
  return Status;
}


STATIC LOADER_ENTRY *CreateLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderOptions, IN CHAR16 *FullTitle, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume,
                                       IN EG_IMAGE *Image, IN EG_IMAGE *DriveImage, IN UINT8 OSType, IN UINT8 Flags, IN CHAR16 Hotkey, IN BOOLEAN CustomEntry)
{
  EFI_DEVICE_PATH *LoaderDevicePath;
  CHAR16          *LoaderDevicePathString;
  CHAR16          *FilePathAsString;
  CHAR16          *OSIconName;
  //CHAR16           IconFileName[256]; Sothor - Unused?
  CHAR16           ShortcutLetter;
  LOADER_ENTRY    *Entry;
  INTN             i;
  
  // Check parameters are valid
  if ((LoaderPath == NULL) || (Volume == NULL)) {
    return NULL;
  }
  
  // Get the loader device path
  LoaderDevicePath = FileDevicePath(Volume->DeviceHandle, LoaderPath);
  if (LoaderDevicePath == NULL) {
    return NULL;
  }
  LoaderDevicePathString = FileDevicePathToStr(LoaderDevicePath);
  if (LoaderDevicePathString == NULL) {
    return NULL;
  }
  
  // Ignore this loader if it's self path
  FilePathAsString = FileDevicePathToStr(SelfLoadedImage->FilePath);
  if (FilePathAsString) {
    INTN Comparison = StriCmp(FilePathAsString, LoaderDevicePathString);
    FreePool(FilePathAsString);
    if (Comparison == 0) {
      DBG("skipped because path `%s` is self path!\n", LoaderDevicePathString);
      FreePool(LoaderDevicePathString);
      return NULL;
    }
  }
  
  if (!CustomEntry) {
    CUSTOM_LOADER_ENTRY *Custom;
    UINTN                CustomIndex = 0;
    
    // Ignore this loader if it's device path is already present in another loader
    if (MainMenu.Entries) {
      for (i = 0; i < MainMenu.EntryCount; ++i) {
        REFIT_MENU_ENTRY *MainEntry = MainMenu.Entries[i];
        // Only want loaders
        if (MainEntry && (MainEntry->Tag == TAG_LOADER)) {
          LOADER_ENTRY *Loader = (LOADER_ENTRY *)MainEntry;
          if (StriCmp(Loader->DevicePathString, LoaderDevicePathString) == 0) {
            DBG("skipped because path `%s` already exists for another entry!\n", LoaderDevicePathString);
            FreePool(LoaderDevicePathString);
            return NULL;
          }
        }
      }
    }
    // If this isn't a custom entry make sure it's not hidden by a custom entry
    Custom = gSettings.CustomEntries;
    while (Custom) {
      // Check if the custom entry is hidden or disabled
      if (OSFLAG_ISSET(Custom->Flags, OSFLAG_DISABLED) ||
          (OSFLAG_ISSET(Custom->Flags, OSFLAG_HIDDEN) && !gSettings.ShowHiddenEntries)) {
        // Check if there needs to be a volume match
        if (Custom->Volume != NULL) {
          // Check if the string matches the volume
          if ((StrStr(Volume->DevicePathString, Custom->Volume) != NULL) ||
              ((Volume->VolName != NULL) && (StrStr(Volume->VolName, Custom->Volume) != NULL))) {
            if (Custom->VolumeType != 0) {
              if (((Volume->DiskKind == DISK_KIND_INTERNAL) && (Custom->VolumeType & DISABLE_FLAG_INTERNAL)) ||
                  ((Volume->DiskKind == DISK_KIND_EXTERNAL) && (Custom->VolumeType & DISABLE_FLAG_EXTERNAL)) ||
                  ((Volume->DiskKind == DISK_KIND_OPTICAL) && (Custom->VolumeType & DISABLE_FLAG_OPTICAL)) ||
                  ((Volume->DiskKind == DISK_KIND_FIREWIRE) && (Custom->VolumeType & DISABLE_FLAG_FIREWIRE))) {
                if (Custom->Path != NULL) {
                  // Try to match the loader paths and types
                  if (StriCmp(Custom->Path, LoaderPath) == 0) {
                    if (Custom->Type != 0) {
                      if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                        DBG("skipped path `%s` because it is a volume, volumetype, path and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                        FreePool(LoaderDevicePathString);
                        return NULL;
                      } else {
                        DBG("partial volume, volumetype, and path match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
                      }
                    } else {
                      DBG("skipped path `%s` because it is a volume, volumetype and path match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                      FreePool(LoaderDevicePathString);
                      return NULL;
                    }
                  } else {
                    DBG("partial volume, and volumetype match for path `%s` and custom entry %d, path did not match\n", LoaderDevicePathString, CustomIndex);
                  }
                } else if (Custom->Type != 0) {
                  if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                    // Only match the loader type
                    DBG("skipped path `%s` because it is a volume, volumetype and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                    FreePool(LoaderDevicePathString);
                    return NULL;
                  } else {
                    DBG("partial volume, and volumetype match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
                  }
                } else {
                  DBG("skipped path `%s` because it is a volume and volumetype match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                  FreePool(LoaderDevicePathString);
                  return NULL;
                }
              } else {
                DBG("partial volume match for path `%s` and custom entry %d, volumetype did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } if (Custom->Path != NULL) {
              // Check if there needs to be a path match also
              if (StriCmp(Custom->Path, LoaderPath) == 0) {
                if (Custom->Type != 0) {
                  if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                    DBG("skipped path `%s` because it is a volume, path and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                    FreePool(LoaderDevicePathString);
                    return NULL;
                  } else {
                    DBG("partial volume, and path match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
                  }
                } else {
                  DBG("skipped path `%s` because it is a volume and path match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                  FreePool(LoaderDevicePathString);
                  return NULL;
                }
              } else {
                DBG("partial volume match for path `%s` and custom entry %d, path did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } else if (Custom->Type != 0) {
              if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                DBG("skipped path `%s` because it is a volume and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                FreePool(LoaderDevicePathString);
                return NULL;
              } else {
                DBG("partial volume match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } else {
              DBG("skipped path `%s` because it is a volume match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
              FreePool(LoaderDevicePathString);
              return NULL;
            }
          } else {
            DBG("volume did not match for path `%s` and custom entry %d\n", LoaderDevicePathString, CustomIndex);
          }
        } else if (Custom->VolumeType != 0) {
          if (((Volume->DiskKind == DISK_KIND_INTERNAL) && (Custom->VolumeType & DISABLE_FLAG_INTERNAL)) ||
              ((Volume->DiskKind == DISK_KIND_EXTERNAL) && (Custom->VolumeType & DISABLE_FLAG_EXTERNAL)) ||
              ((Volume->DiskKind == DISK_KIND_OPTICAL) && (Custom->VolumeType & DISABLE_FLAG_OPTICAL)) ||
              ((Volume->DiskKind == DISK_KIND_FIREWIRE) && (Custom->VolumeType & DISABLE_FLAG_FIREWIRE))) {
            if (Custom->Path != NULL) {
              // Try to match the loader paths and types
              if (StriCmp(Custom->Path, LoaderPath) == 0) {
                if (Custom->Type != 0) {
                  if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                    DBG("skipped path `%s` because it is a volumetype, path and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                    FreePool(LoaderDevicePathString);
                    return NULL;
                  } else {
                    DBG("partial volumetype, and path match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
                  }
                } else {
                  DBG("skipped path `%s` because it is a volumetype and path match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                  FreePool(LoaderDevicePathString);
                  return NULL;
                }
              } else {
                DBG("partial volumetype match for path `%s` and custom entry %d, path did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } else if (Custom->Type != 0) {
              if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                // Only match the loader type
                DBG("skipped path `%s` because it is a volumetype and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                FreePool(LoaderDevicePathString);
                return NULL;
              } else {
                DBG("partial volumetype match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } else {
              DBG("skipped path `%s` because it is a volumetype match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
              FreePool(LoaderDevicePathString);
              return NULL;
            }
          } else {
            DBG("did not match volumetype for path `%s` and custom entry %d\n", LoaderDevicePathString, CustomIndex);
          }
        } else if (Custom->Path != NULL) {
          // Try to match the loader paths and types
          if (StriCmp(Custom->Path, LoaderPath) == 0) {
            if (Custom->Type != 0) {
              if (OSTYPE_COMPARE(Custom->Type, OSType)) {
                DBG("skipped path `%s` because it is a path and type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
                FreePool(LoaderDevicePathString);
                return NULL;
              } else {
                DBG("partial path match for path `%s` and custom entry %d, type did not match\n", LoaderDevicePathString, CustomIndex);
              }
            } else {
              DBG("skipped path `%s` because it is a path match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
              FreePool(LoaderDevicePathString);
              return NULL;
            }
          } else {
            DBG("did not match path for path `%s` and custom entry %d\n", LoaderDevicePathString, CustomIndex);
          }
        } else if (Custom->Type != 0) {
          if (OSTYPE_COMPARE(Custom->Type, OSType)) {
            // Only match the loader type
            DBG("skipped path `%s` because it is a type match for custom entry %d!\n", LoaderDevicePathString, CustomIndex);
            FreePool(LoaderDevicePathString);
            return NULL;
          } else {
            DBG("did not match type for path `%s` and custom entry %d!\n", LoaderDevicePathString, CustomIndex);
          }
        }
      }
      Custom = Custom->Next;
      ++CustomIndex;
    }
  }
  
  // prepare the menu entry
  Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));
  Entry->me.Tag = TAG_LOADER;
  Entry->me.Row = 0;
  Entry->Volume = Volume;
  
  Entry->LoaderPath       = EfiStrDuplicate(LoaderPath);
  Entry->VolName          = Volume->VolName;
  Entry->DevicePath       = LoaderDevicePath;
  Entry->DevicePathString = LoaderDevicePathString;
  Entry->Flags            = Flags;
  if (LoaderOptions) {
    if (OSFLAG_ISSET(Flags, OSFLAG_NODEFAULTARGS)) {
      Entry->LoadOptions  = EfiStrDuplicate(LoaderOptions);
    } else {
      Entry->LoadOptions  = PoolPrint(L"%a %s", gSettings.BootArgs, LoaderOptions);
      Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_NODEFAULTARGS);// Sothor - Once we add arguments once we cannot do it again. So prevent being updated with boot arguments from options menu (unless reinit?).
    }
  } else if ((AsciiStrLen(gSettings.BootArgs) > 0) && OSFLAG_ISUNSET(Flags, OSFLAG_NODEFAULTARGS)) {
    Entry->LoadOptions    = PoolPrint(L"%a", gSettings.BootArgs);
  }
  
  // locate a custom icon for the loader
  //StrCpy(IconFileName, Volume->OSIconName); Sothor - Unused?
  //actions
  Entry->me.AtClick = ActionSelect;
  Entry->me.AtDoubleClick = ActionEnter;
  Entry->me.AtRightClick = ActionDetails;
  
  Entry->LoaderType = OSType;
  Entry->OSVersion = GetOSVersion(Entry);
  
  // detect specific loaders
  OSIconName = NULL;
  ShortcutLetter = 0;
  
  switch (OSType) {
    case OSTYPE_OSX:
    case OSTYPE_RECOVERY:
    case OSTYPE_OSX_INSTALLER:
      OSIconName = GetOSIconName(Entry->OSVersion);// Sothor - Get OSIcon name using OSVersion
      if (Entry->LoadOptions == NULL || (StrStr(Entry->LoadOptions, L"-v") == NULL && StrStr(Entry->LoadOptions, L"-V") == NULL)) {
        // OSX is not booting verbose, so we can set console to graphics mode
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_USEGRAPHICS);
      }
      if (gSettings.WithKexts) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_WITHKEXTS);
      }
      if (gSettings.NoCaches) {
        Entry->Flags = OSFLAG_SET(Entry->Flags, OSFLAG_NOCACHES);
      }
      ShortcutLetter = 'M';
      GetOSXVolumeName(Entry);
      break;
    case OSTYPE_WIN:
      OSIconName = L"win";
      ShortcutLetter = 'W';
      break;
    case OSTYPE_WINEFI:
      OSIconName = L"vista";
      ShortcutLetter = 'V';
      break;
    case OSTYPE_LIN:
      OSIconName = LinuxIconNameFromPath(LoaderPath);
      ShortcutLetter = 'L';
      break;
    case OSTYPE_OTHER:
    case OSTYPE_EFI:
      OSIconName = L"clover";
      ShortcutLetter = 'U';
      Entry->LoaderType = OSTYPE_OTHER;
      break;
    default:
      OSIconName = L"unknown";
      Entry->LoaderType = OSTYPE_OTHER;
      break;
  }
  
  if (FullTitle) {
    Entry->me.Title = EfiStrDuplicate(FullTitle);
  } else {
    Entry->me.Title = PoolPrint(L"Boot %s from %s", (LoaderTitle != NULL) ? LoaderTitle : LoaderPath + 1, Entry->VolName);
  }
  Entry->me.ShortcutLetter = (Hotkey == 0) ? ShortcutLetter : Hotkey;
  
  // get custom volume icon if present

    if (GlobalConfig.CustomIcons && FileExists(Volume->RootDir, L"\\.VolumeIcon.icns")){
      Entry->me.Image = LoadIcns(Volume->RootDir, L"\\.VolumeIcon.icns", 128);
      DBG("using VolumeIcon.icns image from Volume\n");
    } else if (Image) {
      Entry->me.Image = Image;
    } else {      
      Entry->me.Image = LoadOSIcon(OSIconName, L"unknown", 128, FALSE, TRUE);
    }

  // Load DriveImage
  Entry->me.DriveImage = (DriveImage != NULL) ? DriveImage : ScanVolumeDefaultIcon(Volume, Entry->LoaderType);
  
  // DBG("HideBadges=%d Volume=%s ", GlobalConfig.HideBadges, Volume->VolName);
  if (GlobalConfig.HideBadges & HDBADGES_SHOW) {
    if (GlobalConfig.HideBadges & HDBADGES_SWAP) {
      Entry->me.BadgeImage = egCopyScaledImage(Entry->me.DriveImage, 4);
      // DBG(" Show badge as Drive.");
    } else {
      Entry->me.BadgeImage = egCopyScaledImage(Entry->me.Image, 8);
      // DBG(" Show badge as OSImage.");
    }
  }
  return Entry;
}

STATIC VOID AddDefaultMenu(IN LOADER_ENTRY *Entry)
{
  CHAR16            *FileName, *TempOptions;
  CHAR16            DiagsFileName[256];
  LOADER_ENTRY      *SubEntry;
  REFIT_MENU_SCREEN *SubScreen;
  REFIT_VOLUME      *Volume;
  UINT64            VolumeSize;
  EFI_GUID          *Guid = NULL;
  BOOLEAN           KernelIs64BitOnly;

  if (Entry == NULL) {
    return;
  }
  Volume = Entry->Volume;
  if (Volume == NULL) {
    return;
  }

  // Only kernels up to 10.7 have 32-bit mode
  KernelIs64BitOnly = (Entry->OSVersion == NULL || AsciiStrnCmp(Entry->OSVersion,"10.",3) != 0 || Entry->OSVersion[3] > '7');
  
  FileName = Basename(Entry->LoaderPath);
  
  // create the submenu
  SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
  SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", Entry->me.Title, Entry->VolName);
  SubScreen->TitleImage = Entry->me.Image;
  SubScreen->ID = Entry->LoaderType + 20;
  //  DBG("get anime for os=%d\n", SubScreen->ID);
  SubScreen->AnimeRun = GetAnime(SubScreen);
  VolumeSize = RShiftU64(MultU64x32(Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);
  AddMenuInfoLine(SubScreen, PoolPrint(L"Volume size: %dMb", VolumeSize));
  AddMenuInfoLine(SubScreen, DevicePathToStr(Entry->DevicePath));
  Guid = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);
  if (Guid) {
    CHAR8 *GuidStr = AllocateZeroPool(50);
    AsciiSPrint(GuidStr, 50, "%g", Guid);
    AddMenuInfoLine(SubScreen, PoolPrint(L"UUID: %a", GuidStr));
    FreePool(GuidStr);
  }
  
  
  // default entry
  SubEntry = DuplicateLoaderEntry(Entry);
  if (SubEntry) {
    SubEntry->me.Title = (Entry->LoaderType == OSTYPE_OSX ||
                          Entry->LoaderType == OSTYPE_OSX_INSTALLER ||
                          Entry->LoaderType == OSTYPE_RECOVERY) ?
    L"Boot Mac OS X" : PoolPrint(L"Run %s", FileName);
    AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
  }
  // loader-specific submenu entries
  if (Entry->LoaderType == OSTYPE_OSX || Entry->LoaderType == OSTYPE_OSX_INSTALLER || Entry->LoaderType == OSTYPE_RECOVERY) {          // entries for Mac OS X
#if defined(MDE_CPU_X64)
    if (!KernelIs64BitOnly) {
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"arch=x86_64");
        SubEntry->me.Title        = L"Boot Mac OS X (64-bit)";
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
    }
#endif
    if (!KernelIs64BitOnly) {
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->me.Title        = L"Boot Mac OS X (32-bit)";
        SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"arch=i386");
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
    }
    
    if (!(GlobalConfig.DisableFlags & DISABLE_FLAG_SINGLEUSER)) {
      
#if defined(MDE_CPU_X64)
      if (KernelIs64BitOnly) {
        SubEntry = DuplicateLoaderEntry(Entry);
        if (SubEntry) {
          SubEntry->me.Title        = L"Boot Mac OS X in verbose mode";
          SubEntry->Flags           = OSFLAG_UNSET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
          SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
          AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
      } else {
        SubEntry = DuplicateLoaderEntry(Entry);
        if (SubEntry) {
          SubEntry->me.Title        = L"Boot Mac OS X in verbose mode (64bit)";
          SubEntry->Flags           = OSFLAG_UNSET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
          TempOptions = AddLoadOption(SubEntry->LoadOptions, L"-v");
          SubEntry->LoadOptions     = AddLoadOption(TempOptions, L"arch=x86_64");
          FreePool(TempOptions);
          AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
      }
      
#endif
      
      if (!KernelIs64BitOnly) {
        SubEntry = DuplicateLoaderEntry(Entry);
        if (SubEntry) {
          SubEntry->me.Title        = L"Boot Mac OS X in verbose mode (32-bit)";
          SubEntry->Flags           = OSFLAG_SET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
          TempOptions = AddLoadOption(SubEntry->LoadOptions, L"-v");
          SubEntry->LoadOptions     = AddLoadOption(TempOptions, L"arch=i386");
          FreePool(TempOptions);
          AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
      }
      
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->me.Title        = L"Boot Mac OS X in safe mode";
        SubEntry->Flags           = OSFLAG_UNSET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
        TempOptions = AddLoadOption(SubEntry->LoadOptions, L"-v");
        SubEntry->LoadOptions     = AddLoadOption(TempOptions, L"-x");
        FreePool(TempOptions);
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
      
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->me.Title        = L"Boot Mac OS X in single user verbose mode";
        SubEntry->Flags           = OSFLAG_UNSET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
        TempOptions = AddLoadOption(SubEntry->LoadOptions, L"-v");
        SubEntry->LoadOptions     = AddLoadOption(TempOptions, L"-s");
        FreePool(TempOptions);
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
      
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->me.Title        = OSFLAG_ISSET(SubEntry->Flags, OSFLAG_NOCACHES) ?
        L"Boot Mac OS X with caches" :
        L"Boot Mac OS X without caches";
        SubEntry->Flags           = OSFLAG_TOGGLE(SubEntry->Flags, OSFLAG_NOCACHES);
        SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
      
      SubEntry = DuplicateLoaderEntry(Entry);
      if (SubEntry) {
        SubEntry->me.Title        = OSFLAG_ISSET(SubEntry->Flags, OSFLAG_WITHKEXTS) ?
        L"Boot Mac OS X without injected kexts" :
        L"Boot Mac OS X with injected kexts";
        SubEntry->Flags           = OSFLAG_TOGGLE(SubEntry->Flags, OSFLAG_WITHKEXTS);
        SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
        AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
      }
      
      if (OSFLAG_ISSET(Entry->Flags, OSFLAG_WITHKEXTS))
      {
        if (OSFLAG_ISSET(Entry->Flags, OSFLAG_NOCACHES))
        {
          SubEntry = DuplicateLoaderEntry(Entry);
          if (SubEntry) {
            SubEntry->me.Title        = L"Boot Mac OS X with caches and without injected kexts";
            SubEntry->Flags           = OSFLAG_UNSET(OSFLAG_UNSET(SubEntry->Flags, OSFLAG_NOCACHES), OSFLAG_WITHKEXTS);
            SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
          }
        }
        else
        {
          SubEntry = DuplicateLoaderEntry(Entry);
          if (SubEntry) {
            SubEntry->me.Title        = L"Boot Mac OS X without caches and without injected kexts";
            SubEntry->Flags           = OSFLAG_UNSET(OSFLAG_SET(SubEntry->Flags, OSFLAG_NOCACHES), OSFLAG_WITHKEXTS);
            SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
            AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
          }
        }
      }
      else if (OSFLAG_ISSET(Entry->Flags, OSFLAG_NOCACHES))
      {
        SubEntry = DuplicateLoaderEntry(Entry);
        if (SubEntry) {
          SubEntry->me.Title        = L"Boot Mac OS X with caches and with injected kexts";
          SubEntry->Flags           = OSFLAG_SET(OSFLAG_UNSET(SubEntry->Flags, OSFLAG_NOCACHES), OSFLAG_WITHKEXTS);
          SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
          AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
      }
      else
      {
        SubEntry = DuplicateLoaderEntry(Entry);
        if (SubEntry) {
          SubEntry->me.Title        = L"Boot Mac OS X without caches and with injected kexts";
          SubEntry->Flags           = OSFLAG_SET(OSFLAG_SET(SubEntry->Flags, OSFLAG_NOCACHES), OSFLAG_WITHKEXTS);
          SubEntry->LoadOptions     = AddLoadOption(SubEntry->LoadOptions, L"-v");
          AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
        }
      }
    }
    
    // check for Apple hardware diagnostics
    StrCpy(DiagsFileName, L"\\System\\Library\\CoreServices\\.diagnostics\\diags.efi");
    if (FileExists(Volume->RootDir, DiagsFileName) && !(GlobalConfig.DisableFlags & DISABLE_FLAG_HWTEST)) {
      DBG("  - Apple Hardware Test found\n");
      
      // NOTE: Sothor - I'm not sure if to duplicate parent entry here.
      SubEntry = AllocateZeroPool(sizeof(LOADER_ENTRY));
      SubEntry->me.Title        = L"Run Apple Hardware Test";
      SubEntry->me.Tag          = TAG_LOADER;
      SubEntry->LoaderPath      = EfiStrDuplicate(DiagsFileName);
      SubEntry->Volume          = Volume;
      SubEntry->VolName         = Entry->VolName;
      SubEntry->DevicePath      = FileDevicePath(Volume->DeviceHandle, SubEntry->LoaderPath);
      SubEntry->DevicePathString = Entry->DevicePathString;
      SubEntry->Flags           = OSFLAG_SET(Entry->Flags, OSFLAG_USEGRAPHICS);
      SubEntry->me.AtClick      = ActionEnter;
      AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    }
    
  } else if (Entry->LoaderType == OSTYPE_LINEFI) {
    BOOLEAN Quiet = (StrStr(Entry->LoadOptions, L"quiet") != NULL);
    BOOLEAN WithSplash = (StrStr(Entry->LoadOptions, L"splash") != NULL);
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      FreePool(SubEntry->LoadOptions);
      if (Quiet) {
        SubEntry->me.Title    = PoolPrint(L"%s verbose", Entry->me.Title);
        SubEntry->LoadOptions = RemoveLoadOption(Entry->LoadOptions, L"quiet");
      } else {
        SubEntry->me.Title    = PoolPrint(L"%s quiet", Entry->me.Title);
        SubEntry->LoadOptions = AddLoadOption(Entry->LoadOptions, L"quiet");
      }
    }
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      FreePool(SubEntry->LoadOptions);
      if (WithSplash) {
        SubEntry->me.Title    = PoolPrint(L"%s without splash", Entry->me.Title);
        SubEntry->LoadOptions = RemoveLoadOption(Entry->LoadOptions, L"splash");
      } else {
        SubEntry->me.Title    = PoolPrint(L"%s with splash", Entry->me.Title);
        SubEntry->LoadOptions = AddLoadOption(Entry->LoadOptions, L"splash");
      }
    }
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      FreePool(SubEntry->LoadOptions);
      if (WithSplash) {
        if (Quiet) {
          CHAR16 *TempOptions = RemoveLoadOption(Entry->LoadOptions, L"splash");
          SubEntry->me.Title    = PoolPrint(L"%s verbose without splash", Entry->me.Title);
          SubEntry->LoadOptions = RemoveLoadOption(Entry->LoadOptions, L"quiet");
          FreePool(TempOptions);
        } else {
          CHAR16 *TempOptions = RemoveLoadOption(Entry->LoadOptions, L"splash");
          SubEntry->me.Title    = PoolPrint(L"%s quiet without splash", Entry->me.Title);
          SubEntry->LoadOptions = AddLoadOption(Entry->LoadOptions, L"quiet");
          FreePool(TempOptions);
        }
      } else if (Quiet) {
        CHAR16 *TempOptions = RemoveLoadOption(Entry->LoadOptions, L"quiet");
        SubEntry->me.Title    = PoolPrint(L"%s verbose with splash", Entry->me.Title);
        SubEntry->LoadOptions = AddLoadOption(Entry->LoadOptions, L"splash");
        FreePool(TempOptions);
      } else {
        SubEntry->me.Title    = PoolPrint(L"%s quiet with splash", Entry->me.Title);
        SubEntry->LoadOptions = AddLoadOption(Entry->LoadOptions, L"quiet splash");
      }
    }
  } else if ((Entry->LoaderType == OSTYPE_WIN) || (Entry->LoaderType == OSTYPE_WINEFI)) {
    // by default, skip the built-in selection and boot from hard disk only
    Entry->LoadOptions = L"-s -h";
    
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      SubEntry->me.Title        = L"Boot Windows from Hard Disk";
      AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    }
    
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      SubEntry->me.Title        = L"Boot Windows from CD-ROM";
      SubEntry->LoadOptions     = L"-s -c";
      AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    }
    
    SubEntry = DuplicateLoaderEntry(Entry);
    if (SubEntry) {
      SubEntry->me.Title        = PoolPrint(L"Run %s in text mode", FileName);
      SubEntry->Flags           = OSFLAG_UNSET(SubEntry->Flags, OSFLAG_USEGRAPHICS);
      SubEntry->LoadOptions     = L"-v";
      SubEntry->LoaderType      = OSTYPE_OTHER; // Sothor - Why are we using OSTYPE_OTHER here?
      AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    }
    
  }
  
  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->me.SubScreen = SubScreen;
  // DBG("    Added '%s': OSType='%d', OSVersion='%a'\n", Entry->me.Title, Entry->LoaderType, Entry->OSVersion);
}

STATIC VOID AddLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderOptions, IN CHAR16 *LoaderTitle,
                           IN REFIT_VOLUME *Volume, IN EG_IMAGE *Image, IN UINT8 OSType, IN UINT8 Flags)
{
  LOADER_ENTRY *Entry;
  if ((LoaderPath == NULL) || (Volume == NULL) || (Volume->RootDir == NULL) || !FileExists(Volume->RootDir, LoaderPath)) {
    return;
  }
  Entry = CreateLoaderEntry(LoaderPath, LoaderOptions, NULL, LoaderTitle, Volume, Image, NULL, OSType, Flags, 0, FALSE);
  if (Entry != NULL) {
    AddDefaultMenu(Entry);
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
  }
}

VOID ScanLoader(VOID)
{
  UINTN         VolumeIndex, Index;
  REFIT_VOLUME *Volume;
  
  DBG("Scanning loaders...\n");
  
  for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
    Volume = Volumes[VolumeIndex];
    DBG("%2d: '%s'", VolumeIndex, Volume->VolName);
    if (Volume->RootDir == NULL) { // || Volume->VolName == NULL)
      DBG(" no file system\n", VolumeIndex);
      continue;
    }
    if (Volume->VolName == NULL) {
      Volume->VolName = L"Unknown";
    }
    
    // skip volume if its kind is configured as disabled
    if ((Volume->DiskKind == DISK_KIND_OPTICAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_OPTICAL)) ||
        (Volume->DiskKind == DISK_KIND_EXTERNAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_EXTERNAL)) ||
        (Volume->DiskKind == DISK_KIND_INTERNAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_INTERNAL)) ||
        (Volume->DiskKind == DISK_KIND_FIREWIRE && (GlobalConfig.DisableFlags & DISABLE_FLAG_FIREWIRE)))
    {
      DBG(" hidden\n");
      continue;
    }
    
    if (Volume->Hidden) {
      DBG(" hidden\n");
      continue;
    }
    DBG("\n");
    
    // Use standard location for boot.efi, unless the file /.IAPhysicalMedia is present
    // That file indentifies a 2nd-stage Install Media, so when present, skip standard path to avoid entry duplication
    if (!FileExists(Volume->RootDir, L"\\.IAPhysicalMedia")) {
      if(!EFI_ERROR(GetRootUUID(Volume)) || isFirstRootUUID(Volume)) {
        AddLoaderEntry(MACOSX_LOADER_PATH, NULL, L"Mac OS X", Volume, NULL, OSTYPE_OSX, 0);
      }
    }
    // check for Mac OS X Install Data
    AddLoaderEntry(L"\\OS X Install Data\\boot.efi", NULL, L"OS X Install", Volume, NULL, OSTYPE_OSX_INSTALLER, 0);
    AddLoaderEntry(L"\\Mac OS X Install Data\\boot.efi", NULL, L"Mac OS X Install", Volume, NULL, OSTYPE_OSX_INSTALLER, 0);
    AddLoaderEntry(L"\\.IABootFiles\\boot.efi", NULL, L"OS X Install", Volume, NULL, OSTYPE_OSX_INSTALLER, 0);
    // check for Mac OS X Recovery Boot
    AddLoaderEntry(L"\\com.apple.recovery.boot\\boot.efi", NULL, L"Recovery", Volume, NULL, OSTYPE_RECOVERY, 0);

    // Sometimes, on some systems (HP UEFI, if Win is installed first)
    // it is needed to get rid of bootmgfw.efi to allow starting of
    // Clover as /efi/boot/bootx64.efi from HD. We can do that by renaming
    // bootmgfw.efi to bootmgfw-orig.efi
    AddLoaderEntry(L"\\EFI\\Microsoft\\Boot\\bootmgfw-orig.efi", L"", L"Microsoft EFI boot menu", Volume, NULL, OSTYPE_WINEFI, 0);
    // check for Microsoft boot loader/menu
    // If there is bootmgfw-orig.efi, then do not check for bootmgfw.efi
    // since on some systems this will actually be CloverX64.efi
    // renamed to bootmgfw.efi
    AddLoaderEntry(L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi", L"", L"Microsoft EFI boot menu", Volume, NULL, OSTYPE_WINEFI, 0);
    // check for Microsoft boot loader/menu
    AddLoaderEntry(L"\\bootmgr.efi", L"", L"Microsoft EFI boot menu", Volume, NULL, OSTYPE_WINEFI, 0);
    // check for Microsoft boot loader/menu on CDROM
    AddLoaderEntry(L"\\EFI\\MICROSOFT\\BOOT\\cdboot.efi", L"", L"Microsoft EFI boot menu", Volume, NULL, OSTYPE_WINEFI, 0);
    
    // check for linux loaders
    for (Index = 0; Index < LinuxEntryDataCount; ++Index) {
      AddLoaderEntry(LinuxEntryData[Index].Path, L"", LinuxEntryData[Index].Title, Volume,
                     LoadOSIcon(LinuxEntryData[Index].Icon, L"unknown", 128, FALSE, TRUE), OSTYPE_LIN, OSFLAG_NODEFAULTARGS);
    }
    // check for linux kernels
    if (Volume->RootDir != NULL) {
      REFIT_DIR_ITER  Iter;
      EFI_FILE_INFO  *FileInfo = NULL;
      // open the /boot directory (or whatever directory path)
      CHAR16         *Path = LINUX_LOADER_PATH;
      CHAR16         *FileName = Basename(Path);
      CHAR16         *BootDir = NULL;
      UINTN           Length = (FileName - Path);
      if (Length > 1) {
        BootDir = AllocateZeroPool(Length--);
        StrnCpy(BootDir, FileName, Length);
        DirIterOpen(Volume->RootDir, BootDir, &Iter);
        FreePool(BootDir);
      } else {
        DirIterOpen(Volume->RootDir, NULL, &Iter);
      }
      // get all the filename matches
      while (DirIterNext(&Iter, 2, FileName, &FileInfo)) {
        if (FileInfo != NULL) {
          // get the kernel file path
          CHAR16 *Path = PoolPrint(L"%s\\%s", (BootDir == NULL) ? L"" : BootDir, FileInfo->FileName);
          CHAR16 *Options = NULL;
          // Find the init ram image
          CHAR16 *InitRd = LinuxMatchInitImage(Iter.DirHandle, FileInfo->FileName + StrLen(FileName));
          if (InitRd != NULL) {
            Options = PoolPrint(L"initrd=%s %s", InitRd, LINUX_DEFAULT_OPTIONS);
            FreePool(InitRd);
          }
          // Add the entry
          AddLoaderEntry(Path, (Options == NULL) ? LINUX_DEFAULT_OPTIONS : Options, NULL, Volume, NULL, OSTYPE_LINEFI, OSFLAG_NODEFAULTARGS);
          if (Options != NULL) {
            FreePool(Options);
          }
          // free the file info
          FreePool(FileInfo);
          FileInfo = NULL;
        }
      }
      //close the directory
      DirIterClose(&Iter);
      if (BootDir != NULL) {
        FreePool(BootDir);
      }
    }

    //     DBG("search for  optical UEFI\n");
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
      AddLoaderEntry(BOOT_LOADER_PATH, L"", L"UEFI optical", Volume, NULL, OSTYPE_OTHER, 0);
    }
    //     DBG("search for internal UEFI\n");
    if (Volume->DiskKind == DISK_KIND_INTERNAL) {
      AddLoaderEntry(BOOT_LOADER_PATH, L"", L"UEFI internal", Volume, NULL, OSTYPE_OTHER, 0);
    }
    //    DBG("search for external UEFI\n");
    if (Volume->DiskKind == DISK_KIND_EXTERNAL) {
      AddLoaderEntry(BOOT_LOADER_PATH, L"", L"UEFI external", Volume, NULL, OSTYPE_OTHER, 0);
    }
  }
}

STATIC VOID AddCustomEntry(IN UINTN                CustomIndex,
                           IN CHAR16              *CustomPath,
                           IN CUSTOM_LOADER_ENTRY *Custom,
                           IN REFIT_MENU_SCREEN   *SubMenu)
{
  UINTN         VolumeIndex;
  REFIT_VOLUME *Volume;
  BOOLEAN       IsSubEntry = (SubMenu != NULL);

  if (CustomPath == NULL) {
    DBG("Custom %sentry %d skipped because it didn't have a path.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }
  if (OSFLAG_ISSET(Custom->Flags, OSFLAG_DISABLED)) {
    DBG("Custom %sentry %d skipped because it is disabled.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }
  if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET(Custom->Flags, OSFLAG_HIDDEN)) {
    DBG("Custom %sentry %d skipped because it is hidden.\n", IsSubEntry ? L"sub " : L"", CustomIndex);
    return;
  }
  if (Custom->Volume) {
    if (Custom->Title) {
      if (CustomPath) {
        DBG("Custom %sentry %d \"%s\" \"%s\" \"%s\" (%d) 0x%X matching \"%s\" ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, Custom->Title, CustomPath, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags, Custom->Volume);
      } else {
        DBG("Custom %sentry %d \"%s\" \"%s\" (%d) 0x%X matching \"%s\" ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, Custom->Title, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags, Custom->Volume);
      }
    } else if (CustomPath) {
      DBG("Custom %sentry %d \"%s\" \"%s\" (%d) 0x%X matching \"%s\" ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, CustomPath, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags, Custom->Volume);
    } else {
      DBG("Custom %sentry %d \"%s\" (%d) 0x%X matching \"%s\" ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags, Custom->Volume);
    }
  } else if (CustomPath) {
    DBG("Custom %sentry %d \"%s\" \"%s\" (%d) 0x%X matching all volumes ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, CustomPath, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags);
  } else {
    DBG("Custom %sentry %d \"%s\" (%d) 0x%X matching all volumes ...\n", IsSubEntry ? L"sub " : L"", CustomIndex, ((Custom->Options != NULL) ? Custom->Options : L""), Custom->Type, Custom->Flags);
  }
  for (VolumeIndex = 0; VolumeIndex < VolumesCount; ++VolumeIndex) {
    CUSTOM_LOADER_ENTRY *CustomSubEntry;
    LOADER_ENTRY        *Entry = NULL;
    EG_IMAGE            *Image, *DriveImage;
    EFI_GUID            *Guid = NULL;
    UINT64               VolumeSize;

    Volume = Volumes[VolumeIndex];
    if ((Volume == NULL) || (Volume->RootDir == NULL)) {
      continue;
    }
    if (Volume->VolName == NULL) {
      Volume->VolName = L"Unknown";
    }
    
    DBG("   Checking volume \"%s\" (%s) ... ", Volume->VolName, Volume->DevicePathString);
    
    // skip volume if its kind is configured as disabled
    if ((Volume->DiskKind == DISK_KIND_OPTICAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_OPTICAL)) ||
        (Volume->DiskKind == DISK_KIND_EXTERNAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_EXTERNAL)) ||
        (Volume->DiskKind == DISK_KIND_INTERNAL && (GlobalConfig.DisableFlags & DISABLE_FLAG_INTERNAL)) ||
        (Volume->DiskKind == DISK_KIND_FIREWIRE && (GlobalConfig.DisableFlags & DISABLE_FLAG_FIREWIRE)))
    {
      DBG("skipped because media is disabled\n");
      continue;
    }
    
    if (Custom->VolumeType != 0) {
      if ((Volume->DiskKind == DISK_KIND_OPTICAL && ((Custom->VolumeType & DISABLE_FLAG_OPTICAL) == 0)) ||
          (Volume->DiskKind == DISK_KIND_EXTERNAL && ((Custom->VolumeType & DISABLE_FLAG_EXTERNAL) == 0)) ||
          (Volume->DiskKind == DISK_KIND_INTERNAL && ((Custom->VolumeType & DISABLE_FLAG_INTERNAL) == 0)) ||
          (Volume->DiskKind == DISK_KIND_FIREWIRE && ((Custom->VolumeType & DISABLE_FLAG_FIREWIRE) == 0))) {
        DBG("skipped because media is ignored\n");
        continue;
      }
    }
    
    if (Volume->Hidden) {
      DBG("skipped because volume is hidden\n");
      continue;
    }
    // Check for exact volume matches
    if (Custom->Volume) {
      
      if ((StrStr(Volume->DevicePathString, Custom->Volume) == NULL) &&
          ((Volume->VolName == NULL) || (StrStr(Volume->VolName, Custom->Volume) == NULL))) {
        DBG("skipped\n");
        continue;
      }
      // NOTE: Sothor - We dont care about legacy OS type // Check if the volume should be of certain os type
      //if ((Custom->Type != 0) && (Volume->OSType != 0) && !OSTYPE_COMPARE(OSType, Volume->OSType)) {
      //  DBG("skipped because wrong type (%d != %d)\n", OSType, Volume->OSType);
      //  continue;
      //}
      //} else if ((Custom->Type != 0) && (Volume->OSType != 0) && !OSTYPE_COMPARE(OSType, Volume->OSType)) {
      //DBG("skipped because wrong type (%d != %d)\n", OSType, Volume->OSType);
      //continue;
    }
    // Check the volume is readable and the entry exists on the volume
    if (Volume->RootDir == NULL) {
      DBG("skipped because filesystem is not readable\n");
      continue;
    }
    if (!FileExists(Volume->RootDir, CustomPath)) {
      DBG("skipped because path does not exist\n");
      continue;
    }
    if (StriCmp(CustomPath, MACOSX_LOADER_PATH) == 0 && FileExists(Volume->RootDir, L"\\.IAPhysicalMedia")) {
      DBG("skipped standard OSX path because volume is 2nd stage Install Media\n");
      continue;
    }
    // Change to custom image if needed
    Image = Custom->Image;
    if ((Image == NULL) && Custom->ImagePath) {
      Image = egLoadImage(Volume->RootDir, Custom->ImagePath, TRUE);
      if (Image == NULL) {
        Image = egLoadImage(ThemeDir, Custom->ImagePath, TRUE);
        if (Image == NULL) {
          Image = egLoadImage(SelfDir, Custom->ImagePath, TRUE);
          if (Image == NULL) {
            Image = egLoadImage(SelfRootDir, Custom->ImagePath, TRUE);
            if (Image == NULL) {
              Image = LoadOSIcon(Custom->ImagePath, L"unknown", 128, FALSE, FALSE);
            }
          }
        }
      }
    }
    // Change to custom drive image if needed
    DriveImage = Custom->DriveImage;
    if ((DriveImage == NULL) && Custom->DriveImagePath) {
      DriveImage = egLoadImage(Volume->RootDir, Custom->DriveImagePath, TRUE);
      if (DriveImage == NULL) {
        DriveImage = egLoadImage(ThemeDir, Custom->DriveImagePath, TRUE);
        if (DriveImage == NULL) {
          DriveImage = egLoadImage(SelfDir, Custom->DriveImagePath, TRUE);
          if (DriveImage == NULL) {
            DriveImage = egLoadImage(SelfRootDir, Custom->DriveImagePath, TRUE);
            if (DriveImage == NULL) {
              DriveImage = LoadBuiltinIcon(Custom->DriveImagePath);
            }
          }
        }
      }
    }
    // Change custom drive image if needed
    // Update volume boot type
    Volume->BootType = BOOTING_BY_EFI;
    DBG("match!\n");
    // Create a entry for this volume
    Entry = CreateLoaderEntry(CustomPath, Custom->Options, Custom->FullTitle, Custom->Title, Volume, Image, DriveImage, Custom->Type, Custom->Flags, Custom->Hotkey, TRUE);
    if (Entry != NULL) {
      if (OSFLAG_ISUNSET(Custom->Flags, OSFLAG_NODEFAULTMENU)) {
        AddDefaultMenu(Entry);
      } else if (Custom->SubEntries != NULL) {
        UINTN CustomSubIndex = 0;
        // Add subscreen
        REFIT_MENU_SCREEN *SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
        if (SubScreen) {
          SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", (Custom->Title != NULL) ? Custom->Title : CustomPath, Entry->VolName);
          SubScreen->TitleImage = Entry->me.Image;
          SubScreen->ID = Custom->Type + 20;
          SubScreen->AnimeRun = GetAnime(SubScreen);
          VolumeSize = RShiftU64(MultU64x32(Volume->BlockIO->Media->LastBlock, Volume->BlockIO->Media->BlockSize), 20);
          AddMenuInfoLine(SubScreen, PoolPrint(L"Volume size: %dMb", VolumeSize));
          AddMenuInfoLine(SubScreen, DevicePathToStr(Entry->DevicePath));
          Guid = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);
          if (Guid) {
            CHAR8 *GuidStr = AllocateZeroPool(50);
            AsciiSPrint(GuidStr, 50, "%g", Guid);
            AddMenuInfoLine(SubScreen, PoolPrint(L"UUID: %a", GuidStr));
            FreePool(GuidStr);
          }
          // Create sub entries
          for (CustomSubEntry = Custom->SubEntries; CustomSubEntry; CustomSubEntry = CustomSubEntry->Next) {
            AddCustomEntry(CustomSubIndex++, (CustomSubEntry->Path != NULL) ? CustomSubEntry->Path : CustomPath, CustomSubEntry, SubScreen);
          }
          AddMenuEntry(SubScreen, &MenuEntryReturn);
          Entry->me.SubScreen = SubScreen;
        }
      }
      AddMenuEntry(IsSubEntry ? SubMenu : &MainMenu, (REFIT_MENU_ENTRY *)Entry);
    }
  }
}

// Add custom entries
VOID AddCustomEntries(VOID)
{
  CUSTOM_LOADER_ENTRY *Custom;
  UINTN                i = 0;
  
  DBG("Custom entries start\n");
  // Traverse the custom entries
  for (Custom = gSettings.CustomEntries; Custom; ++i, Custom = Custom->Next) {
    if ((Custom->Path == NULL) && (Custom->Type != 0)) {
      if (OSTYPE_IS_OSX(Custom->Type)) {
        AddCustomEntry(i, MACOSX_LOADER_PATH, Custom, NULL);
      } else if (OSTYPE_IS_OSX_RECOVERY(Custom->Type)) {
        AddCustomEntry(i, L"\\com.apple.recovery.boot\\boot.efi", Custom, NULL);
      } else if (OSTYPE_IS_OSX_INSTALLER(Custom->Type)) {
        UINTN Index = 0;
        while (Index < OSXInstallerPathsCount) {
          AddCustomEntry(i, OSXInstallerPaths[Index++], Custom, NULL);
        }
      } else if (OSTYPE_IS_WINDOWS(Custom->Type)) {
        AddCustomEntry(i, L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi", Custom, NULL);
      } else if (OSTYPE_IS_LINUX(Custom->Type)) {
        UINTN Index = 0;
        while (Index < LinuxEntryDataCount) {
          AddCustomEntry(i, LinuxEntryData[Index++].Path, Custom, NULL);
        }
      } else {
        AddCustomEntry(i, BOOT_LOADER_PATH, Custom, NULL);
      }
    } else {
      AddCustomEntry(i, Custom->Path, Custom, NULL);
    }
  }
  DBG("Custom entries finish\n");
}