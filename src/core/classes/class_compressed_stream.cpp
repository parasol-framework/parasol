/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
CompressedStream: Acts as a proxy for decompressing and compressing data streams between objects.

Use the CompressedStream class to compress and decompress data on the fly without the need for a temporary storage
area.  The default compression algorithm is DEFLATE with gzip header data.  It is compatible with common command-line
tools such as gzip.

To decompress data, set the #Input field with a source object that supports the Read action, such as a File.
Repeatedly reading from the CompressedStream will automatically handle the decompression process for you.  If the
decompressed size of the incoming data is defined in the source header, it will be reflected in the #Size
field.

To compress data, set the #Output field with a source object that supports the #Write() action, such as a @File.
Repeatedly writing to the CompressedStream with raw data will automatically handle the compression process for you.
Once all of the data has been written, call the #Write() action with a Buffer of NULL and Length -1 to signal an end
to the streaming process.

-END-

*****************************************************************************/

#define PRV_COMPRESSION
#define PRV_COMPRESSEDSTREAM

#define ZLIB_MEM_LEVEL 8
#include "zlib.h"

#ifndef __system__
#define __system__
#endif

#include "../defs.h"
#include <parasol/modules/core.h>

static ERROR CSTREAM_Reset(objCompressedStream *, APTR);

//****************************************************************************

static ERROR CSTREAM_Free(objCompressedStream *Self, APTR Void)
{
   CSTREAM_Reset(Self, NULL);
   return ERR_Okay;
}

//****************************************************************************

static ERROR CSTREAM_Init(objCompressedStream *Self, APTR Void)
{
   parasol::Log log(__FUNCTION__);

   if ((!Self->Input) and (!Self->Output)) return log.warning(ERR_FieldNotSet);

   if ((Self->Input) and (Self->Output)) {
      log.warning("A CompressedStream can operate in either read or write mode, not both.");
      return ERR_Failed;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR CSTREAM_NewObject(objCompressedStream *Self, APTR Void) {
   Self->Format = CF_GZIP;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Read: Decompress data from the input stream and write it to the supplied buffer.
-END-
*****************************************************************************/

#define MIN_OUTPUT_SIZE ((32 * 1024) + 2048)

static ERROR CSTREAM_Read(objCompressedStream *Self, struct acRead *Args)
{
   parasol::Log log(__FUNCTION__);

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);
   if (!(Self->Head::Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   Args->Result = 0;
   if (Args->Length <= 0) return ERR_Okay;

   UBYTE inputstream[2048];
   LONG length;

   if (acRead(Self->Input, inputstream, sizeof(inputstream), &length)) return ERR_Read;

   if (length <= 0) return ERR_Okay;

   if (!Self->Inflating) {
      log.trace("Initialising decompression of the stream.");
      ClearMemory(&Self->Stream, sizeof(Self->Stream));
      switch (Self->Format) {
         case CF_ZLIB:
            if (inflateInit2(&Self->Stream, MAX_WBITS) != ERR_Okay) return log.warning(ERR_Decompression);
            break;

         case CF_DEFLATE:
            if (inflateInit2(&Self->Stream, -MAX_WBITS) != ERR_Okay) return log.warning(ERR_Decompression);
            break;

         case CF_GZIP:
         default:
            if (inflateInit2(&Self->Stream, 15 + 32) != ERR_Okay) return log.warning(ERR_Decompression);
            // Read the uncompressed size from the gzip header
            if (inflateGetHeader(&Self->Stream, &Self->Header) != Z_OK) {
               return log.warning(ERR_InvalidData);
            }
      }

      Self->Inflating = TRUE;
   }

   APTR output = Args->Buffer;
   LONG outputsize = Args->Length;
   if (outputsize < MIN_OUTPUT_SIZE) {
      // An internal buffer will need to be allocated if the one supplied to Read() is not large enough.
      outputsize = MIN_OUTPUT_SIZE;
      if (!(output = Self->OutputBuffer)) {
         if (AllocMemory(MIN_OUTPUT_SIZE, MEM_DATA|MEM_NO_CLEAR, (APTR *)&Self->OutputBuffer, NULL)) return ERR_AllocMemory;
         output = Self->OutputBuffer;
      }
   }

   Self->Stream.next_in  = inputstream;
   Self->Stream.avail_in = length;

   ERROR error = ERR_Okay;
   LONG result = Z_OK;
   while ((result IS Z_OK) and (Self->Stream.avail_in > 0) and (outputsize > 0)) {
      Self->Stream.next_out  = (Bytef *)output;
      Self->Stream.avail_out = outputsize;
      result = inflate(&Self->Stream, Z_SYNC_FLUSH);

      if ((result) and (result != Z_STREAM_END)) {
         error = convert_zip_error(&Self->Stream, result);
         break;
      }

      if (error) break;

      Args->Result += outputsize - Self->Stream.avail_out;
      output = (Bytef *)output + outputsize - Self->Stream.avail_out;

      if (result IS Z_STREAM_END) { // Decompression is complete
         Self->Inflating = FALSE;
         Self->TotalOutput = Self->Stream.total_out;
         return ERR_Okay;
      }
   }

   return error;
}

/*****************************************************************************
-ACTION-
Reset: Reset the state of the stream.

Resetting a CompressedStream returns it to the same state as that when first initialised.  Note that this does not
affect the state of the object referenced via #Input or #Output, so it may be necessary for the client to reset
referenced objects separately.

*****************************************************************************/

static ERROR CSTREAM_Reset(objCompressedStream *Self, APTR Void)
{
   Self->TotalOutput = 0;

   if (Self->Inflating) {
      inflateEnd(&Self->Stream);
      Self->Inflating = FALSE;
   }

   if (Self->Deflating) {
      deflateEnd(&Self->Stream);
      Self->Deflating = FALSE;
   }

   if (Self->OutputBuffer) { FreeResource(Self->OutputBuffer); Self->OutputBuffer = NULL; }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Seek: For use in decompressing streams only.  Seeks to a position within the stream.
-END-
*****************************************************************************/

static ERROR CSTREAM_Seek(objCompressedStream *Self, struct acSeek *Args)
{
   parasol::Log log(__FUNCTION__);

   if (!Args) return ERR_NullArgs;

   if (Self->Output) { // Seeking in write mode isn't possible (violates the streaming process).
      return log.warning(ERR_NoSupport);
   }

   if (!Self->Input) return log.warning(ERR_FieldNotSet);

   // Seeking results in a reset of the compression object's state.  It then needs to decompress the stream up to the
   // position requested by the client.

   CSTREAM_Reset(Self, NULL);

   LARGE pos = 0;
   if (Args->Position IS SEEK_START) pos = F2T(Args->Offset);
   else if (Args->Position IS SEEK_CURRENT) pos = Self->TotalOutput + F2T(Args->Offset);
   else return log.warning(ERR_Args);

   if (pos < 0) return log.warning(ERR_OutOfRange);

   UBYTE buffer[1024];
   while (pos > 0) {
      struct acRead read = { .Buffer = buffer, .Length = (LONG)pos };
      if ((size_t)read.Length > sizeof(buffer)) read.Length = sizeof(buffer);
      if (Action(AC_Read, Self, &read)) return ERR_Decompression;
      pos -= read.Result;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Write: Compress raw data in a buffer and write it to the Output object.
-END-
*****************************************************************************/

static ERROR CSTREAM_Write(objCompressedStream *Self, struct acWrite *Args)
{
   parasol::Log log(__FUNCTION__);

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);
   if (!(Self->Head::Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   if (!Self->Deflating) {
      ClearMemory(&Self->Stream, sizeof(Self->Stream));

      switch (Self->Format) {
         case CF_ZLIB:
            if (deflateInit2(&Self->Stream, 9, Z_DEFLATED, MAX_WBITS, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)) {
               return log.warning(ERR_Compression);
            }
            break;

         case CF_DEFLATE:
            if (deflateInit2(&Self->Stream, 9, Z_DEFLATED, -MAX_WBITS, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)) {
               return log.warning(ERR_Compression);
            }
            break;

         case CF_GZIP:
         default:
            if (deflateInit2(&Self->Stream, 9, Z_DEFLATED, 15 + 32, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)) {
               return log.warning(ERR_Compression);
            }
      }

      Self->TotalOutput = 0;
      Self->Deflating = TRUE;
   }

   if (!Self->OutputBuffer) {
      if (AllocMemory(MIN_OUTPUT_SIZE, MEM_DATA|MEM_NO_CLEAR, (APTR *)&Self->OutputBuffer, NULL)) return ERR_AllocMemory;
   }

   Args->Result = 0;
   LONG mode;
   if (Args->Length IS -1) { // A length of -1 is a signal to complete the compression process.
      mode = Z_FINISH;
      Self->Stream.next_in  = Self->OutputBuffer;
      Self->Stream.avail_in = 0;
   }
   else {
      mode = Z_NO_FLUSH;
      Self->Stream.next_in  = (Bytef *)Args->Buffer;
      Self->Stream.avail_in = Args->Length;
   }

   // If zlib succeeds but sets avail_out to zero, this means that data was written to the output buffer, but the
   // output buffer is not large enough (so keep calling until avail_out > 0).

   Self->Stream.avail_out = 0;
   while (Self->Stream.avail_out IS 0) {
      Self->Stream.next_out  = Self->OutputBuffer;
      Self->Stream.avail_out = MIN_OUTPUT_SIZE;

      if ((deflate(&Self->Stream, mode))) {
         deflateEnd(&Self->Stream);
         Self->Deflating = FALSE;
         return ERR_BufferOverflow;
      }

      const LONG len = MIN_OUTPUT_SIZE - Self->Stream.avail_out; // Get number of compressed bytes that were output

      if (len > 0) {
         Self->TotalOutput += len;
         log.trace("%d bytes (total " PF64() ") were compressed.", len, Self->TotalOutput);
         acWrite(Self->Output, Self->OutputBuffer, len, NULL);
      }
      else {
         // deflate() may not output anything if it needs more data to fill up a compression frame.  Return ERR_Okay
         // and wait for more data, or for the developer to end the stream.

         //log.trace("No data output on this cycle.");
         break;
      }
   }

   if (mode IS Z_FINISH) {
      deflateEnd(&Self->Stream);
      Self->Deflating = FALSE;
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Format: The format of the compressed stream.  The default is GZIP.

-FIELD-
Input: An input object that will supply data for decompression.

To create a stream that decompresses data from a compressed source, set the Input field with a reference to an object
that will provide the source data.  It is most common for the source object to be a @File type, however any
class that supports the Read action is permitted.

The source object must be in a readable state.  The Input field is mutually exclusive to the #Output field.

-FIELD-
Output: A target object that will receive data compressed by the stream.

To create a stream that compresses data to a target object, set the Output field with an object reference.  It is
most common for the target object to be a @File type, however any class that supports the Write action is
permitted.

The target object must be in a writeable state.  The Output field is mutually exclusive to the #Input field.

-FIELD-
Size: The uncompressed size of the input source, if known.

The Size field will reflect the uncompressed size of the input source, if this can be determined from the header.
In the case of GZIP decompression, the size will not be known until the parser has consumed the header.
This means that at least one call to the #Read() action is required before the Size is known.

If the size is unknown, a value of -1 is returned.

*****************************************************************************/

static ERROR CSTREAM_GET_Size(objCompressedStream *Self, LARGE *Value)
{
   *Value = -1;
   if (Self->Input) {
      if (Self->Header.done) {
         if (Self->Header.extra) *Value = Self->Header.extra_len;
      }

      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
TotalOutput: A live counter of total bytes that have been output by the stream.
-END-

*****************************************************************************/

#include "class_compressed_stream_def.c"

//****************************************************************************

static const FieldArray clStreamFields[] = {
   { "TotalOutput", FDF_LARGE|FDF_R,   0, NULL, NULL },
   { "Input",       FDF_OBJECT|FDF_RI, 0, NULL, NULL },
   { "Output",      FDF_OBJECT|FDF_RI, 0, NULL, NULL },
   { "Format",      FDF_LONG|FDF_LOOKUP|FD_RI, (MAXINT)&clCompressedStreamFormat, NULL, NULL },
   // Virtual fields
   { "Size",        FDF_LARGE|FDF_R,   0, (APTR)CSTREAM_GET_Size, NULL },
   END_FIELD
};

static const ActionArray clStreamActions[] = {
   { AC_Free,      (APTR)CSTREAM_Free },
   { AC_Init,      (APTR)CSTREAM_Init },
   { AC_NewObject, (APTR)CSTREAM_NewObject },
   { AC_Read,      (APTR)CSTREAM_Read },
   { AC_Reset,     (APTR)CSTREAM_Reset },
   { AC_Seek,      (APTR)CSTREAM_Seek },
   { AC_Write,     (APTR)CSTREAM_Write },
   { 0, NULL }
};

extern "C" ERROR add_compressed_stream_class(void)
{
   return(CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&glCompressedStreamClass,
      FID_BaseClassID|TLONG,    ID_COMPRESSEDSTREAM,
      FID_ClassVersion|TFLOAT,  1.0,
      FID_Name|TSTRING,         "CompressedStream",
      FID_FileDescription|TSTR, "GZip File",
      FID_Category|TLONG,       CCF_DATA,
      FID_Actions|TPTR,         clStreamActions,
      FID_Fields|TARRAY,        clStreamFields,
      FID_Size|TLONG,           sizeof(objCompressedStream),
      FID_Path|TSTR,            "modules:core",
      TAGEND));
}
