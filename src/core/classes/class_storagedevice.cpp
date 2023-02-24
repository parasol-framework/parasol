/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
StorageDevice: Queries the meta data of file system volumes.

The StorageDevice class returns the meta data of file system volumes.  A reference to an existing volume is required
in the #Volume field in order to make a successful analysis.  If the volume name cannot be resolved,
initialisation will fail.

Following initialisation, all meta fields describing the volume are readable for further information.
-END-

*********************************************************************************************************************/

#define PRV_FILESYSTEM
#include "../defs.h"

static ERROR STORAGE_Free(extStorageDevice *, APTR);
static ERROR STORAGE_Init(extStorageDevice *, APTR);

//********************************************************************************************************************

static ERROR STORAGE_Free(extStorageDevice *Self, APTR Void)
{
   if (Self->Volume) { FreeResource(Self->Volume); Self->Volume = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR STORAGE_Init(extStorageDevice *Self, APTR Void)
{
   pf::Log log;

   if (!Self->Volume) return log.warning(ERR_FieldNotSet);

   const virtual_drive *vd = get_fs(Self->Volume);

   if (vd->is_virtual()) Self->DeviceFlags |= DEVICE_SOFTWARE;

   Self->BytesFree  = -1;
   Self->BytesUsed  = 0;
   Self->DeviceSize = -1;

   if (vd->GetDeviceInfo) return vd->GetDeviceInfo(Self->Volume, Self);
   else return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
BytesFree: Total amount of storage space that is available, measured in bytes.

-FIELD-
BytesUsed: Total amount of storage space in use.

The total amount of storage space used by the device is indicated in this field.

Please note that storage usage is typically measured in terms of blocks.  For instance a block size of 512 bytes will
mean that this field will be a multiple of 512.  Two files of 1 byte each on such a file system would take up 1024
bytes of space and not 2.

-FIELD-
DeviceID: A unique ID for the mounted device (platform dependent, NULL if unavailable).

If a volume expresses a unique device identifier such as a factory serial number, it will be readable from this field.

*********************************************************************************************************************/

static ERROR GET_DeviceID(extStorageDevice *Self, STRING *Value)
{
   if (Self->DeviceID) {
      *Value = Self->DeviceID;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

/*********************************************************************************************************************
-FIELD-
DeviceSize: The storage size of the device in bytes, without accounting for the file system format.

This field indicates the storage size of the device.  It does not reflect the available space determined by
the device's file system, which will typically be measurably smaller than this value.

-FIELD-
DeviceFlags: These read-only flags identify the type of device and its features.
Lookup: DEVICE

-FIELD-
Volume: The volume name of the device to query.

Set the Volume field prior to initialisation for that volume to be queried by the object.  The standard volume string
format is 'name:', but omitting the colon or defining complete file system paths when writing this field is also
acceptable.  Any characters following a colon will be stripped automatically with no ongoing functional impact.
-END-

*********************************************************************************************************************/

static ERROR GET_Volume(extStorageDevice *Self, STRING *Value)
{
   if (Self->Volume) {
      *Value = Self->Volume;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Volume(extStorageDevice *Self, CSTRING Value)
{
   pf::Log log;

   if (Self->initialised()) return log.warning(ERR_Immutable);

   if ((Value) and (*Value)) {
      LONG len;
      for (len=0; (Value[len]) and (Value[len] != ':'); len++);

      if (Self->Volume) FreeResource(Self->Volume);

      if (!AllocMemory(len+2, MEM_STRING|MEM_NO_CLEAR, (APTR *)&Self->Volume, NULL)) {
         CopyMemory(Value, Self->Volume, len);
         Self->Volume[len] = ':';
         Self->Volume[len+1] = 0;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clDeviceFlags[] = {
   { "CompactDisc",  DEVICE_COMPACT_DISC },
   { "HardDisk",     DEVICE_HARD_DISK },
   { "FloppyDisk",   DEVICE_FLOPPY_DISK },
   { "Read",         DEVICE_READ },
   { "Write",        DEVICE_WRITE },
   { "Removable",    DEVICE_REMOVABLE },
   { "Software",     DEVICE_SOFTWARE },
   { "Network",      DEVICE_NETWORK },
   { "Tape",         DEVICE_TAPE },
   { "Printer",      DEVICE_PRINTER },
   { "Scanner",      DEVICE_SCANNER },
   { "Temporary",    DEVICE_TEMPORARY },
   { "Memory",       DEVICE_MEMORY },
   { "Modem",        DEVICE_MODEM },
   { "USB",          DEVICE_USB },
   { NULL, 0 }
};

static const FieldArray clFields[] = {
   { "DeviceFlags", FDF_LARGE|FDF_R, (MAXINT)&clDeviceFlags, NULL, NULL },
   { "DeviceSize",  FDF_LARGE|FDF_R, 0, NULL, NULL },
   { "BytesFree",   FDF_LARGE|FDF_R, 0, NULL, NULL },
   { "BytesUsed",   FDF_LARGE|FDF_R, 0, NULL, NULL },
   // Virtual fields
   { "DeviceID",    FDF_STRING|FDF_R, 0, (APTR)GET_DeviceID, NULL },
   { "Volume",      FDF_STRING|FDF_REQUIRED|FDF_RI, 0, (APTR)GET_Volume, (APTR)SET_Volume },
    END_FIELD
};

static const ActionArray clActions[] = {
   { AC_Free, (APTR)STORAGE_Free },
   { AC_Init, (APTR)STORAGE_Init },
   { 0, NULL }
};

//********************************************************************************************************************

extern "C" ERROR add_storage_class(void)
{
   glStorageClass = extMetaClass::create::global(
      fl::BaseClassID(ID_STORAGEDEVICE),
      fl::ClassVersion(VER_STORAGEDEVICE),
      fl::Name("StorageDevice"),
      fl::Category(CCF_SYSTEM),
      fl::Actions(clActions),
      fl::Fields(clFields),
      fl::Size(sizeof(extStorageDevice)),
      fl::Path("modules:core"));

   return glStorageClass ? ERR_Okay : ERR_AddClass;
}
