/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Compression: Compresses data into archives, supporting a variety of compression formats.

The Compression class provides the necessary means to compress and decompress data.  It provides support for file
based compression as well as memory based compression routines.  The base class uses zip algorithms to support pkzip
files, while other forms of compressed data can be supported by installing additional compression sub-classes.

The following examples demonstrate basic usage of compression objects in Fluid:

<pre>
// Create a new zip archive and compress two files.

cmp = obj.new('compression', { path='temp:result.zip', flags='!NEW' } )
err = cmp.mtCompressFile('config:defs/compression.def', '')
err = cmp.mtCompressFile('config:defs/core.def', '')

// Decompress all *.def files in the root of an archive.

cmp = obj.new('compression', { path='temp:result.zip' } )
err = cmp.mtDecompressFile('*.def', 'temp:')
</pre>

It is strongly advised that Compression objects are created for the purpose of either writing to, or reading from the
target archive.  The class is not designed for both purposes simultaneously, particularly due to considerations for
maximising operational speed.

If data is to be encrypted or decrypted, set the #Password field with a null-terminated encryption string. If the
password for an encrypted file, errors will be returned when trying to decompress the information (the source archive
may be reported as a corrupted file).

To list the contents of an archive, use the #Scan() method.

To adjust the level of compression used to pack each file, set the #CompressionLevel field to a value between 0 and
100%.

This code is based on the work of Jean-loup Gailly and Mark Adler.

-END-

*****************************************************************************/

#define ZLIB_MEM_LEVEL 8

#include "zlib.h"

#define PRV_COMPRESSION
#include "../defs.h"
#include <parasol/main.h>

static ERROR compress_folder(objCompression *, CSTRING, CSTRING);
static ERROR compress_file(objCompression *, CSTRING, CSTRING, BYTE);
static void print(objCompression *, CSTRING);
static ERROR remove_file(objCompression *, struct ZipFile **);
static ERROR scan_zip(objCompression *);
static ERROR fast_scan_zip(objCompression *);
static ERROR send_feedback(objCompression *, struct CompressionFeedback *);
static void write_eof(objCompression *Self);

static ERROR GET_Size(objCompression *, LARGE *);

static const struct MethodArray clCompressionMethods[];
static const struct ActionArray clCompressionActions[];
static const struct FieldArray clFields[];

//****************************************************************************
// Special definitions.

static const UBYTE glHeader[HEAD_LENGTH] = {
   'P', 'K', 0x03, 0x04,   // 0 Signature
   0x14, 0x00,             // 4 Version 2.0
   0x00, 0x00,             // 6 Flags
   0x08, 0x00,             // 8 Deflation method
   0x00, 0x00, 0x00, 0x00, // 10 Time stamp
   0x00, 0x00, 0x00, 0x00, // 14 CRC
   0x00, 0x00, 0x00, 0x00, // 18 Compressed Size
   0x00, 0x00, 0x00, 0x00, // 22 Original File Size
   0x00, 0x00,             // 26 Length of path/filename
   0x00, 0x00              // 28 Length of extra field.
};

static const UBYTE glList[LIST_LENGTH] = {
   'P', 'K', 0x01, 0x02,   // 00 Signature
   0x14, ZIP_PARASOL,      // 04 Version 2.0, host OS
   0x14, 0x00,             // 06 Version need to extract, OS needed to extract
   0x00, 0x00,             // 08 Flags
   0x08, 0x00,             // 10 Deflation method
   0x00, 0x00, 0x00, 0x00, // 12 Time stamp
   0x00, 0x00, 0x00, 0x00, // 16 CRC
   0x00, 0x00, 0x00, 0x00, // 20 Compressed Size
   0x00, 0x00, 0x00, 0x00, // 24 Original File Size
   0x00, 0x00,             // 28 Length of path/filename
   0x00, 0x00,             // 30 Length of extra field
   0x00, 0x00,             // 32 Length of comment
   0x00, 0x00,             // 34 Disk number start
   0x00, 0x00,             // 36 File attribute: 0 = Binary, 1 = ASCII
   0x00, 0x00, 0x00, 0x00, // 38 File permissions?
   0x00, 0x00, 0x00, 0x00, // 42 Offset of compressed data within the file
   // File name follows
   // Extra field follows
   // Comment follows
};

static const UBYTE glTail[TAIL_LENGTH] = {
   'P', 'K', 0x05, 0x06,   // 0 Signature
   0x00, 0x00,             // 4 Number of this disk
   0x00, 0x00,             // 6 Number of the disk with starting central directory
   0x00, 0x00,             // 8 Number of files in the central directory for this zip file
   0x00, 0x00,             // 10 Number of files in the archive spanning all disks
   0x00, 0x00, 0x00, 0x00, // 12 Size of file list
   0x00, 0x00, 0x00, 0x00, // 16 Offset of the file list with respect to starting disk number
   0x00, 0x00              // 20 Length of zip file comment
   // End of file comment follows
};

ERROR add_compression_class(void)
{
   return(CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&glCompressionClass,
      FID_ClassVersion|TFLOAT,  VER_COMPRESSION,
      FID_Name|TSTRING,         "Compression",
      FID_FileExtension|TSTR,   "*.zip",
      FID_FileDescription|TSTR, "ZIP File",
      FID_FileHeader|TSTR,      "[0:$504b0304]",
      FID_Category|TLONG,       CCF_DATA,
      FID_Actions|TPTR,         clCompressionActions,
      FID_Methods|TARRAY,       clCompressionMethods,
      FID_Fields|TARRAY,        clFields,
      FID_Size|TLONG,           sizeof(objCompression),
      FID_Path|TSTR,            "modules:core",
      TAGEND));
}

//****************************************************************************

static ERROR COMPRESSION_ActionNotify(objCompression *Self, struct acActionNotify *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->ActionID IS AC_Free) {
      if ((Self->Feedback.Type IS CALL_SCRIPT) AND (Self->Feedback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Feedback.Type = CALL_NONE;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
CompressBuffer: Compresses a plain memory area into an empty buffer.

This method provides a simple way of compressing a memory area into a buffer.  It requires a reference to the
source data and a buffer large enough to accept the compressed information.  Generally the destination buffer should
be no smaller than 75% of the size of the source data.  If the destination buffer is not large enough, an error of
ERR_BufferOverflow will be returned.  The size of the compressed data will be returned in the Result parameter.

To decompress the data that is output by this function, use the #DecompressBuffer() method.

The compression method used to compress the data will be identified in the first 32 bits of output, for example,
`ZLIB`.  The following 32 bits will indicate the length of the compressed data section, followed by the data itself.

-INPUT-
buf(ptr) Input: Pointer to the source data.
bufsize InputSize: Byte length of the source data.
buf(ptr) Output: Pointer to a destination buffer.
bufsize OutputSize: Available space in the destination buffer.
&int Result: The size of the compressed data will be returned in this parameter.

-ERRORS-
Okay: The data was compressed successfully.  The Result parameter indicates the size of the compressed data.
Args
NullArgs
Failed
BufferOverflow: The output buffer is not large enough.
-END-

*****************************************************************************/

static ERROR COMPRESSION_CompressBuffer(objCompression *Self, struct cmpCompressBuffer *Args)
{
   if ((!Args) OR (!Args->Input) OR (Args->InputSize <= 0) OR (!Args->Output) OR (Args->OutputSize <= 8)) {
      return PostError(ERR_Args);
   }

   Self->prvZip.next_in   = Args->Input;
   Self->prvZip.avail_in  = Args->InputSize;
   Self->prvZip.next_out  = Args->Output + 8;
   Self->prvZip.avail_out = Args->OutputSize - 8;

   LONG err, level;

   level = Self->CompressionLevel / 10;
   if (level < 0) level = 0;
   else if (level > 9) level = 9;

   if ((err = deflateInit2(&Self->prvZip, level, Z_DEFLATED, Self->WindowBits, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)) IS ERR_Okay) {
      if ((err = deflate(&Self->prvZip, Z_FINISH)) IS Z_STREAM_END) {
         Args->Result = Self->prvZip.total_out + 8;
         deflateEnd(&Self->prvZip);

         ((BYTE *)Args->Output)[0] = 'Z';
         ((BYTE *)Args->Output)[1] = 'L';
         ((BYTE *)Args->Output)[2] = 'I';
         ((BYTE *)Args->Output)[3] = 'B';
         ((LONG *)Args->Output)[1] = Self->prvZip.total_out;
         return ERR_Okay;
      }
      else {
         deflateEnd(&Self->prvZip);
         return PostError(ERR_BufferOverflow);
      }
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-METHOD-
CompressStreamStart: Initialises a new compression stream.

The level of compression is determined by the #CompressionLevel field value.

-ERRORS-
Okay
Failed: Failed to initialise the decompression process.

*****************************************************************************/

static ERROR COMPRESSION_CompressStreamStart(objCompression *Self, APTR Void)
{
   if (Self->Deflating) {
      deflateEnd(&Self->Stream);
      Self->Deflating = FALSE;
   }

   LONG level = Self->CompressionLevel / 10;
   if (level < 0) level = 0;
   else if (level > 9) level = 9;

   ClearMemory(&Self->Stream, sizeof(Self->Stream));

   Self->TotalOutput = 0;
   LONG err;
   if ((err = deflateInit2(&Self->Stream, level, Z_DEFLATED, Self->WindowBits, ZLIB_MEM_LEVEL, Z_DEFAULT_STRATEGY)) IS ERR_Okay) {
      MSG("Compression stream initialised.");
      Self->Deflating = TRUE;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-METHOD-
CompressStream: Compresses streamed data into a buffer.

Use the CompressStream method to compress incoming streams of data whilst using a minimal amount of memory.  The
compression process is handled in three phases of Start, Compress and End.  The methods provided for each phase are
#CompressStreamStart(), #CompressStream() and #CompressStreamEnd().

A compression object can manage only one compression stream at any given time.  If it is necessary to compress multiple
streams at once, create a compression object for each individual stream.

No meta-information is written to the stream, so the client will need a way to record the total number of bytes that have been
output during the compression process. This value must be stored somewhere in order to decompress the stream correctly.
There is also no header information recorded to identify the type of algorithm used to compress the stream.  We
recommend that the compression object's sub-class ID is stored for future reference.

The following C code illustrates a simple means of compressing a file to another file using a stream:

<pre>
ERROR error = mtCompressStreamStart(compress);

if (!error) {
   LONG len;
   LONG cmpsize = 0;
   UBYTE input[4096];
   while (!(error = acRead(file, input, sizeof(input), &len))) {
      if (!len) break; // No more data to read.

      error = mtCompressStream(compress, input, len, &callback, NULL, 0);
      if (error) break;

      if (result > 0) {
         cmpsize += result;
         error = acWrite(outfile, output, result, &len);
         if (error) break;
      }
   }

   if (!error) {
      if (!(error = mtCompressStreamEnd(compress, NULL, 0))) {
         cmpsize += result;
         error = acWrite(outfile, output, result, &len);
      }
   }
}
</pre>

Please note that, depending on the type of algorithm, this method will not always write data to the output buffer.  The
algorithm may store a copy of the input and wait for more data for efficiency reasons.  Any unwritten data will be
resolved when the stream is terminated with #CompressStreamEnd().  To check if data was output by this
function, either set a flag in the callback function or compare the #TotalOutput value to its original setting
before CompressStream was called.

-INPUT-
buf(ptr) Input: Pointer to the source data.
bufsize Length: Amount of data to compress, in bytes.
ptr(func) Callback: This callback function will be called with a pointer to the compressed data.
buf(ptr) Output: Optional.  Points to a buffer that will receive the compressed data.  Must be equal to or larger than the MinOutputSize field.
bufsize OutputSize: Indicates the size of the Output buffer, otherwise set to zero.

-ERRORS-
Okay
NullArgs
Args
BufferOverflow: The output buffer is not large enough to contain the compressed data.
Retry: Please recall the method using a larger output buffer.
-END-

*****************************************************************************/

static ERROR COMPRESSION_CompressStream(objCompression *Self, struct cmpCompressStream *Args)
{
   if ((!Args) OR (!Args->Input) OR (!Args->Callback)) return PostError(ERR_NullArgs);

   if (!Self->Deflating) return PostError(ERR_Failed);

   Self->Stream.next_in   = Args->Input;
   Self->Stream.avail_in  = Args->Length;

   APTR output;
   LONG err, outputsize;
   if ((output = Args->Output)) {
      outputsize = Args->OutputSize;
      if (outputsize < Self->MinOutputSize) {
         LogErrorMsg("OutputSize (%d) < MinOutputSize (%d)", outputsize, Self->MinOutputSize);
         return ERR_BufferOverflow;
      }
   }
   else if ((output = Self->OutputBuffer)) {
      outputsize = Self->OutputSize;
   }
   else {
      Self->OutputSize = 32 * 1024;
      if (AllocMemory(Self->OutputSize, MEM_DATA|MEM_NO_CLEAR, (APTR *)&Self->OutputBuffer, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
      output = Self->OutputBuffer;
      outputsize = Self->OutputSize;
   }

   MSG("Compressing Input: %p, Len: %d to buffer of size %d bytes.", Args->Input, Args->Length, outputsize);

   // If zlib succeeds but sets avail_out to zero, this means that data was written to the output buffer, but the
   // output buffer is not large enough (so keep calling until avail_out > 0).

   ERROR error;
   Self->Stream.avail_out = 0;
   while (Self->Stream.avail_out IS 0) {
      Self->Stream.next_out  = output;
      Self->Stream.avail_out = outputsize;
      if ((err = deflate(&Self->Stream, Z_NO_FLUSH))) {
         deflateEnd(&Self->Stream);
         error = ERR_BufferOverflow;
         break;
      }
      else error = ERR_Okay;

      LONG len = outputsize - Self->Stream.avail_out; // Get number of compressed bytes that were output

      if (len > 0) {
         Self->TotalOutput += len;

         MSG("%d bytes (total " PF64() ") were compressed.", len, Self->TotalOutput);

         if (Args->Callback->Type IS CALL_STDC) {
            ERROR (*routine)(objCompression *Self, APTR Output, LONG Length);
            OBJECTPTR context = SetContext(Args->Callback->StdC.Context);
               routine = Args->Callback->StdC.Routine;
               error = routine(Self, output, len);
            SetContext(context);
         }
         else if (Args->Callback->Type IS CALL_SCRIPT) {
            OBJECTPTR script = Args->Callback->Script.Script;
            if (script) {
               const struct ScriptArg args[] = {
                  { "Compression", FD_OBJECTPTR, { .Address = Self } },
                  { "Output", FD_BUFFER, { .Address = output } },
                  { "OutputLength", FD_LONG|FD_BUFSIZE, { .Long = len } }
               };
               error = scCallback(script, Args->Callback->Script.ProcedureID, args, ARRAYSIZE(args));
               if (!error) GetLong(script, FID_Error, &error);
               else error = ERR_Failed;
            }
            else error = ERR_Terminate;
         }
         else {
            LogErrorMsg("Callback function structure does not specify a recognised Type.");
            break;
         }
      }
      else {
         // deflate() may not output anything if it needs more data to fill up a compression frame.  Return ERR_Okay
         // and wait for more data, or for the developer to call CompressStreamEnd().

         //MSG("No data output on this cycle.");
         break;
      }
   }

   if (error) PostError(error);
   return error;
}

/*****************************************************************************

-METHOD-
CompressStreamEnd: Ends the compression of an open stream.

To end the compression process, this method must be called to write any final blocks of data and remove the resources
that were allocated.

The expected format of the Callback function is specified in the #CompressStream() method.

-INPUT-
ptr(func) Callback: Refers to a function that will be called for each compressed block of data.
buf(ptr) Output: Optional pointer to a buffer that will receive the compressed data.  If not set, the compression object will use its own buffer.
bufsize OutputSize: Size of the buffer specified in Output (value ignored if Output is NULL).

-ERRORS-
Okay
NullArgs
BufferOverflow: The supplied Output buffer is not large enough (check the MinOutputSize field for the minimum allowable size).

*****************************************************************************/

static ERROR COMPRESSION_CompressStreamEnd(objCompression *Self, struct cmpCompressStreamEnd *Args)
{
   if ((!Args) OR (!Args->Callback)) return PostError(ERR_NullArgs);
   if (!Self->Deflating) return ERR_Okay;

   APTR output;
   LONG outputsize;

   if ((output = Args->Output)) {
      outputsize = Args->OutputSize;
      if (outputsize < Self->MinOutputSize) return PostError(ERR_BufferOverflow);
   }
   else if ((output = Self->OutputBuffer)) {
      outputsize = Self->OutputSize;
   }
   else return PostError(ERR_FieldNotSet);

   MSG("Output Size: %d", outputsize);

   Self->Stream.next_in   = 0;
   Self->Stream.avail_in  = 0;
   Self->Stream.avail_out = 0;

   ERROR error;
   LONG err = Z_OK;
   while ((Self->Stream.avail_out IS 0) AND (err IS Z_OK)) {
      Self->Stream.next_out  = output;
      Self->Stream.avail_out = outputsize;
      if ((err = deflate(&Self->Stream, Z_FINISH)) AND (err != Z_STREAM_END)) {
         error = PostError(ERR_BufferOverflow);
         break;
      }

      Self->TotalOutput += outputsize - Self->Stream.avail_out;

      if (Args->Callback->Type IS CALL_STDC) {
         ERROR (*routine)(objCompression *, APTR Output, LONG Length);
         OBJECTPTR context = SetContext(Args->Callback->StdC.Context);
            routine = Args->Callback->StdC.Routine;
            error = routine(Self, output, outputsize - Self->Stream.avail_out);
         SetContext(context);
      }
      else if (Args->Callback->Type IS CALL_SCRIPT) {
         OBJECTPTR script = Args->Callback->Script.Script;
         const struct ScriptArg args[] = {
            { "Compression",  FD_OBJECTPTR, { .Address = Self } },
            { "Output",       FD_BUFFER, { .Address = output } },
            { "OutputLength", FD_LONG|FD_BUFSIZE, { .Long = outputsize - Self->Stream.avail_out } }
         };
         if (script) {
            error = scCallback(script, Args->Callback->Script.ProcedureID, args, ARRAYSIZE(args));
            if (!error) GetLong(script, FID_Error, &error);
            else error = ERR_Failed;
         }
         else error = ERR_Terminate;
      }
   }

   // Free the output buffer if it is quite large

   if ((Self->OutputBuffer) AND (Self->OutputSize > 64 * 1024)) {
      FreeResource(Self->OutputBuffer);
      Self->OutputBuffer = NULL;
      Self->OutputSize = 0;
   }

   deflateEnd(&Self->Stream);
   ClearMemory(&Self->Stream, sizeof(Self->Stream));
   Self->Deflating = FALSE;
   return error;
}

/*****************************************************************************

-METHOD-
DecompressStreamStart: Initialises a new decompression stream.

Use the DecompressStreamStart method to initialise a new decompression stream.  No parameters are required.

If a decompression stream is already active at the time that this method is called, all resources associated with that
stream will be deallocated so that the new stream can be initiated.

To decompress the data stream, follow this call with repeated calls to #DecompressStream() until all the data
has been processed, then call #DecompressStreamEnd().

-ERRORS-
Okay
Failed: Failed to initialise the decompression process.

*****************************************************************************/

static ERROR COMPRESSION_DecompressStreamStart(objCompression *Self, APTR Void)
{
   if (Self->Inflating) {
      inflateEnd(&Self->Stream);
      Self->Inflating = FALSE;
   }

   ClearMemory(&Self->Stream, sizeof(Self->Stream));

   Self->TotalOutput = 0;

   LONG err;
   if ((err = inflateInit2(&Self->Stream, Self->WindowBits)) IS ERR_Okay) {
      MSG("Decompression stream initialised.");
      Self->Inflating = TRUE;
      return ERR_Okay;
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-METHOD-
DecompressStream: Decompresses streamed data to an output buffer.

Call DecompressStream repeatedly to decompress a data stream and process the results in a callback routine.  The client
will need to provide a pointer to the data in the Input parameter and indicate its size in Length.  The decompression
routine will call the routine that was specified in Callback for each block that is decompressed.

The format of the Callback routine is `ERROR Function(*Compression, APTR Buffer, LONG Length)`

The Buffer will refer to the start of the decompressed data and its size will be indicated in Length.  If the Callback
routine returns an error of any kind, the decompression process will be stopped and the error code will be immediately
returned by the method.

Optionally, the client can specify an output buffer in the Output parameter.  This can be a valuable
optimisation technique, as it will eliminate the need to copy data out of the compression object's internal buffer.

When there is no more data in the decompression stream or if an error has occurred, the client must call
#DecompressStreamEnd().

-INPUT-
buf(ptr) Input: Pointer to data to decompress.
bufsize Length: Amount of data to decompress from the Input parameter.
ptr(func) Callback: Refers to a function that will be called for each decompressed block of information.
buf(ptr) Output: Optional pointer to a buffer that will receive the decompressed data.  If not set, the compression object will use its own buffer.
bufsize OutputSize: Size of the buffer specified in Output (value ignored if Output is NULL).

-ERRORS-
Okay
NullArgs
AllocMemory
BufferOverflow: The output buffer is not large enough.

*****************************************************************************/

static ERROR COMPRESSION_DecompressStream(objCompression *Self, struct cmpDecompressStream *Args)
{
   if ((!Args) OR (!Args->Input) OR (!Args->Callback)) return PostError(ERR_NullArgs);
   if (!Self->Inflating) return ERR_Okay; // Decompression is complete

   APTR output;
   LONG outputsize;

   if ((output = Args->Output)) {
      outputsize = Args->OutputSize;
      if (outputsize < Self->MinOutputSize) return PostError(ERR_BufferOverflow);
   }
   else if ((output = Self->OutputBuffer)) {
      outputsize = Self->OutputSize;
   }
   else {
      Self->OutputSize = 32 * 1024;
      if (AllocMemory(Self->OutputSize, MEM_DATA|MEM_NO_CLEAR, (APTR *)&Self->OutputBuffer, NULL) != ERR_Okay) {
         return ERR_AllocMemory;
      }
      output = Self->OutputBuffer;
      outputsize = Self->OutputSize;
   }

   Self->Stream.next_in  = Args->Input;
   Self->Stream.avail_in = Args->Length;

   // Keep looping until Z_STREAM_END or an error is returned

   ERROR error = ERR_Okay;
   LONG result = Z_OK;
   while ((result IS Z_OK) AND (Self->Stream.avail_in > 0)) {
      Self->Stream.next_out  = output;
      Self->Stream.avail_out = outputsize;
      result = inflate(&Self->Stream, Z_SYNC_FLUSH);

      if ((result) AND (result != Z_STREAM_END)) {
         if (Self->Stream.msg) LogErrorMsg("%s", Self->Stream.msg);
         else LogErrorMsg("Zip error: %d", result);

         switch(result) {
            case Z_STREAM_ERROR:  error = ERR_Failed; break;
            case Z_DATA_ERROR:    error = ERR_InvalidData; break;
            case Z_MEM_ERROR:     error = ERR_Memory; break;
            case Z_BUF_ERROR:     error = ERR_BufferOverflow; break;
            case Z_VERSION_ERROR: error = ERR_WrongVersion; break;
            default:              error = ERR_Failed;
         }
         break;
      }

      if (error) break;

      // Write out the decompressed data

      LONG len = outputsize - Self->Stream.avail_out;
      if (len > 0) {
         if (Args->Callback->Type IS CALL_STDC) {
            ERROR (*routine)(objCompression *, APTR Output, LONG Length);
            OBJECTPTR context = SetContext(Args->Callback->StdC.Context);
               routine = Args->Callback->StdC.Routine;
               error = routine(Self, output, len);
            SetContext(context);
         }
         else if (Args->Callback->Type IS CALL_SCRIPT) {
            OBJECTPTR script = Args->Callback->Script.Script;
            if (script) {
               const struct ScriptArg args[] = {
                  { "Compression",  FD_OBJECTPTR, { .Address = Self } },
                  { "Output",       FD_BUFFER, { .Address = output } },
                  { "OutputLength", FD_LONG|FD_BUFSIZE, { .Long = len } }
               };
               error = scCallback(script, Args->Callback->Script.ProcedureID, args, ARRAYSIZE(args));
               if (!error) GetLong(script, FID_Error, &error);
               else error = ERR_Failed;
            }
            else error = ERR_Terminate;
         }
         else {
            LogErrorMsg("Callback function structure does not specify a recognised Type.");
            break;
         }
      }

      if (error) break;

      if (result IS Z_STREAM_END) {
         // Decompression is complete
         Self->Inflating = FALSE;
         Self->TotalOutput = Self->Stream.total_out;
         break;
      }
   }

   if (error) PostError(error);
   return error;
}

/*****************************************************************************

-METHOD-
DecompressStreamEnd: Must be called at the end of the decompression process.

To end the decompression process, this method must be called to write any final blocks of data and remove the resources
that were allocated during decompression.

-INPUT-
ptr(func) Callback: Refers to a function that will be called for each decompressed block of information.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR COMPRESSION_DecompressStreamEnd(objCompression *Self, struct cmpDecompressStreamEnd *Args)
{
   if (Self->Inflating IS FALSE) return ERR_Okay;

   if ((!Args) OR (!Args->Callback)) return PostError(ERR_NullArgs);

   Self->TotalOutput = Self->Stream.total_out;
   inflateEnd(&Self->Stream);
   ClearMemory(&Self->Stream, sizeof(Self->Stream));
   Self->Inflating = FALSE;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
CompressFile: Add files to a compression object.

The CompressFile method is used to add new files and folders to a compression object. You need to supply the location
of the file to compress, as well as the path that is prefixed to the file name when it is in the compression object.
The Location parameter accepts wildcards, allowing you to add multiple files in a single function call if you require
this convenience.

To compress all contents of a folder, specify its path in the Location parameter and ensure that it is fully qualified
by appending a forward slash or colon character.

The Path parameter must include a slash when targeting a folder, otherwise the source file will be renamed to suit the
target path.  If the Path starts with a forward slash and the source is a folder, the name of that folder will be used
in the target path for the compressed files and folders.

-INPUT-
cstr Location: The location of the file(s) to add.
cstr Path:     The path that is prefixed to the file name when added to the compression object.  May be NULL for no path.

-ERRORS-
Okay: The file was added to the compression object.
Args:
File: An error was encountered when trying to open the source file.
NoPermission: The CMF_READ_ONLY flag has been set on the compression object.
NoSupport: The sub-class does not support this method.

*****************************************************************************/

static ERROR COMPRESSION_CompressFile(objCompression *Self, struct cmpCompressFile *Args)
{
   if ((!Args) OR (!Args->Location) OR (!*Args->Location)) return PostError(ERR_NullArgs);
   if (!Self->FileIO) return PostError(ERR_MissingPath);

   if (Self->Flags & CMF_READ_ONLY) return PostError(ERR_NoPermission);

   if (Self->Head.SubID) return PostError(ERR_NoSupport);

   if (Self->OutputID) {
      StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Compressing \"%s\" to \"%s\".\n", Args->Location, Self->Location);
      print(Self, Self->prvOutput);
   }

   LONG i;
   CSTRING path;
   BYTE incdir = FALSE;
   if (!Args->Path) path = "";
   else {
      // Accept the path by default but check it for illegal symbols just in case

      path = Args->Path;

      if (*path IS '/') {
         // Special mode: prefix Args->Location folder name to the root path
         incdir = TRUE;
         path++;
      }

      for (i=0; path[i]; i++) {
         if ((path[i] IS '*') OR (path[i] IS '?') OR (path[i] IS '"') OR
             (path[i] IS ':') OR (path[i] IS '|') OR (path[i] IS '<') OR
             (path[i] IS '>')) {
            LogErrorMsg("Illegal characters in path: %s", path);
            if (Self->OutputID) {
               StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Warning - path ignored due to illegal characters: %s\n", path);
               print(Self, Self->prvOutput);
            }
            path = "";
            break;
         }
      }
   }

   LogF("CompressFile()","Location: %s, Path: %s", Args->Location, path);

   Self->prvFileIndex = 0;

   for (i=0; Args->Location[i]; i++);
   if ((Args->Location[i-1] IS '/') OR (Args->Location[i-1] IS '\\') OR (Args->Location[i-1] IS ':')) {
      // The source is a folder

      if ((*path) OR (incdir)) {
         // This subroutine creates a path custom string if the inclusive folder name option is on, or if the path is
         // missing a terminating slash character.

         LONG inclen = 0;
         if (incdir) {
            i -= 1;
            while ((i > 0) AND (Args->Location[i-1] != '/') AND (Args->Location[i-1] != '\\') AND (Args->Location[i-1] != ':')) {
               inclen++;
               i--;
            }
         }

         LONG pathlen;
         for (pathlen=0; path[pathlen]; pathlen++);

         if ((inclen) OR ((path[pathlen-1] != '/') AND (path[pathlen-1] != '\\'))) {
            UBYTE newpath[inclen+1+pathlen+2];

            LONG j = 0;
            if (inclen > 0) {
               while (Args->Location[i]) newpath[j++] = Args->Location[i++];
            }

            for (i=0; path[i]; i++) newpath[j++] = path[i];
            if ((j > 0) AND (newpath[j-1] != '/') AND (newpath[j-1] != '\\')) newpath[j++] = '/';
            newpath[j] = 0;

            return compress_folder(Self, Args->Location, newpath);
         }
      }

      return compress_folder(Self, Args->Location, path);
   }

   ERROR error = ERR_Okay;

   // Check the location string for wildcards, * and ?

   BYTE wildcard = FALSE;
   LONG len, pathlen;
   for (len=0; Args->Location[len]; len++);
   for (pathlen=len; pathlen > 0; pathlen--) {
      if ((Args->Location[pathlen-1] IS '*') OR (Args->Location[pathlen-1] IS '?')) wildcard = TRUE;
      else if ((Args->Location[pathlen-1] IS ':') OR (Args->Location[pathlen-1] IS '/') OR (Args->Location[pathlen-1] IS '\\')) break;
   }

   if (wildcard IS FALSE) {
      return compress_file(Self, Args->Location, path, FALSE);
   }
   else {
      UBYTE filename[len-pathlen+1];
      UBYTE dirlocation[len+1];

      // Extract the file name from the location (drop the path)

      LONG j;
      for (j=0; Args->Location[pathlen+j]; j++) filename[j] = Args->Location[pathlen+j];
      filename[j] = 0;

      // Extract the path from the location (drop the file name)

      for (j=0; j < pathlen; j++) dirlocation[j] = Args->Location[j];
      dirlocation[j] = 0;

      struct DirInfo *dir;
      if (!OpenDir(dirlocation, RDF_FILE, &dir)) {
         while (!ScanDir(dir)) {
            struct FileInfo *scan = dir->Info;
            if (!StrCompare(filename, scan->Name, 0, STR_WILDCARD)) {
               for (len=0; scan->Name[len]; len++);
               UBYTE folder[pathlen+len+1];
               for (j=0; j < pathlen; j++) folder[j] = Args->Location[j];
               for (j=0; scan->Name[j]; j++) folder[pathlen+j] = scan->Name[j];
               folder[pathlen+j] = 0;
               error = compress_file(Self, folder, path, FALSE);
            }
         }

         FreeResource(dir);
      }
   }

   if (Self->OutputID) {
      LARGE size;
      GET_Size(Self, &size);
      StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "\nCompression complete.  Archive is " PF64() " bytes in size.", size);
      print(Self, Self->prvOutput);
   }

   return error;
}

/*****************************************************************************

-METHOD-
DecompressBuffer: Decompresses data originating from the CompressBuffer method.

This method is used to decompress data that has been packed using the #CompressBuffer() method.  You need
to provide a pointer to the compressed information and an output buffer large enough to accept the decompressed
information.  If the output buffer is not large enough to contain the data, the method will write out as much
information as it can and then return with an error code of ERR_BufferOverflow.

-INPUT-
buf(ptr) Input: Pointer to the compressed data.
buf(ptr) Output: Pointer to the decompression buffer.
bufsize OutputSize: Size of the decompression buffer.
&int Result: The amount of bytes decompressed will be returned in this parameter.

-ERRORS-
Okay
Args
BufferOverflow: The output buffer is not large enough to hold the decompressed information.

*****************************************************************************/

static ERROR COMPRESSION_DecompressBuffer(objCompression *Self, struct cmpDecompressBuffer *Args)
{
   if ((!Args) OR (!Args->Input) OR (!Args->Output) OR (Args->OutputSize <= 0)) {
      return PostError(ERR_NullArgs);
   }

   Self->prvZip.next_in   = Args->Input + 8;
   Self->prvZip.avail_in  = ((LONG *)Args->Input)[1];
   Self->prvZip.next_out  = Args->Output;
   Self->prvZip.avail_out = Args->OutputSize;

   if (inflateInit2(&Self->prvZip, Self->WindowBits) IS ERR_Okay) {
      LONG err;
      if ((err = inflate(&Self->prvZip, Z_FINISH)) IS Z_STREAM_END) {
         Args->Result = Self->prvZip.total_out;
         inflateEnd(&Self->prvZip);
         return ERR_Okay;
      }
      else {
         inflateEnd(&Self->prvZip);
         if (Self->prvZip.msg) LogErrorMsg("%s", Self->prvZip.msg);
         else PostError(ERR_BufferOverflow);
         return ERR_BufferOverflow;
      }
   }
   else return PostError(ERR_Failed);
}

/*****************************************************************************

-METHOD-
DecompressFile: Extracts one or more files from a compression object.

Use the DecompressFile method to decompress a file or files to a destination folder.  The exact path name of the
compressed file is required for extraction unless using wildcards.  A single asterisk in the Path parameter will
extract all files in a compression object.

When specifying the Dest parameter, it is recommended that you specify a folder location by using a forward slash at
the end of the string.  If this is omitted, the destination will be interpreted as a file name.  If the destination
file already exists, it will be overwritten by the decompressed data.

This method sends feedback at regular intervals during decompression.  For further information on receiving feedback,
please refer to the #Feedback field.

-INPUT-
cstr Path: The full path name of the file to extract from the archive.
cstr Dest: The destination to extract the file to.
int Flags: Optional flags.  Currently unused.

-ERRORS-
Okay: The file was successfully extracted.
Args
NullArgs
MissingPath
NoData
File: A destination file could not be created.
Seek
Write: Failed to write uncompressed information to a destination file.
Cancelled: The decompression process was cancelled by the feedback mechanism.
Failed

*****************************************************************************/

static ERROR COMPRESSION_DecompressFile(objCompression *Self, struct cmpDecompressFile *Args)
{
   if (!Self->prvFiles) return ERR_NoData;

   // Validate arguments

   if ((!Args) OR (!Args->Path)) {
      if (Self->OutputID) {
         StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Please supply a Path setting that refers to a compressed file archive.\n");
         print(Self, Self->prvOutput);
      }

      return PostError(ERR_NullArgs);
   }

   if (!Args->Dest) {
      if (Self->OutputID) {
         StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Please supply a Destination that refers to a folder for decompression.\n");
         print(Self, Self->prvOutput);
      }

      return PostError(ERR_NullArgs);
   }

   if ((!*Args->Path) OR (!*Args->Dest)) {
      if (Self->OutputID) {
         StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Please supply valid Path and Destination settings.\n");
         print(Self, Self->prvOutput);
      }

      return PostError(ERR_Args);
   }

   if (!Self->FileIO) {
      if (Self->OutputID) {
         StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Internal error - decompression aborted.\n");
         print(Self, Self->prvOutput);
      }

      return PostError(ERR_MissingPath);
   }

   // If the object belongs to a Compression sub-class, return ERR_NoSupport

   if (Self->Head.SubID) return ERR_NoSupport;

   // Tell the user what we are doing

   if (Self->OutputID) {
      StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Decompressing archive \"%s\" with path \"%s\" to \"%s\".\n", Self->Location, Args->Path, Args->Dest);
      print(Self, Self->prvOutput);
   }

   // Search for the file(s) in our archive that match the given name and extract them to the destination folder.

   LogF("~DecompressFile()","%s TO %s, Permissions: $%.8x", Args->Path, Args->Dest, Self->Permissions);

   UWORD i, pos;

   UBYTE location[400];
   for (pos=0; (Args->Dest[pos]) AND (pos < sizeof(location)-1); pos++) location[pos] = Args->Dest[pos];
   location[pos] = 0;

   UWORD pathend = 0;
   for (i=0; Args->Path[i]; i++) if ((Args->Path[i] IS '/') OR (Args->Path[i] IS '\\')) pathend = i + 1;

   ERROR error      = ERR_Okay;
   UWORD inflateend = FALSE;
   objFile *file    = NULL;
   Self->prvFileIndex = 0;

   struct CompressionFeedback feedback;
   ClearMemory(&feedback, sizeof(feedback));

   struct ZipFile *zf;
   for (zf=Self->prvFiles; zf; zf=(struct ZipFile *)zf->Next) {
      MSG("Found %s", zf->Name);
      if (!StrCompare(Args->Path, zf->Name, 0, STR_WILDCARD)) {
         MSG("Extracting \"%s\"", zf->Name);

         if (Self->OutputID) {
            StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "  %s", zf->Name);
            print(Self, Self->prvOutput);
         }

         // If the destination path specifies a folder, add the name of the file to the destination to generate the
         // correct file name.

         UWORD j = pos;
         if ((location[j-1] IS '/') OR (location[j-1] IS '\\') OR (location[j-1] IS ':')) {
            for (i=pathend; zf->Name[i]; i++) {
               location[j++] = zf->Name[i];
            }
            location[j] = 0;
         }

         // If the destination is a folder that already exists, skip this compression entry

         if ((location[j-1] IS '/') OR (location[j-1] IS '\\')) {
            LONG result;
            if ((!AnalysePath(location, &result)) AND (result IS LOC_DIRECTORY)) {
               Self->prvFileIndex++;
               continue;
            }
         }

         // Send compression feedback

         feedback.Year   = 1980 + ((zf->TimeStamp>>25) & 0x3f);
         feedback.Month  = (zf->TimeStamp>>21) & 0x0f;
         feedback.Day    = (zf->TimeStamp>>16) & 0x1f;
         feedback.Hour   = (zf->TimeStamp>>11) & 0x1f;
         feedback.Minute = (zf->TimeStamp>>5)  & 0x3f;
         feedback.Second = (zf->TimeStamp>>1)  & 0x0f;
         feedback.FeedbackID     = FDB_DECOMPRESS_FILE;
         feedback.Index          = Self->prvFileIndex;
         feedback.Path           = zf->Name;
         feedback.Dest           = location;
         feedback.OriginalSize   = zf->OriginalSize;
         feedback.CompressedSize = zf->CompressedSize;
         feedback.Progress       = 0;

         error = send_feedback(Self, &feedback);
         if ((error IS ERR_Terminate) OR (error IS ERR_Cancelled)) {
            error = ERR_Cancelled;
            goto exit;
         }
         else if (error IS ERR_Skip) {
            error = ERR_Okay;
            Self->prvFileIndex++; // Increase counter to show that the file was analysed
            continue;
         }
         else error = ERR_Okay;

         // Seek to the start of the compressed data

         if (acSeek(Self->FileIO, zf->Offset + HEAD_NAMELEN, SEEK_START) != ERR_Okay) {
            error = PostError(ERR_Seek);
            goto exit;
         }

         UWORD namelen = read_word(Self->FileIO);
         UWORD extralen = read_word(Self->FileIO);
         if (acSeek(Self->FileIO, namelen + extralen, SEEK_CURRENT) != ERR_Okay) {
            error = PostError(ERR_Seek);
            goto exit;
         }

         if (zf->Flags & ZIP_LINK) {
            // For symbolic links, decompress the data to get the destination link string

            Self->prvZip.next_in   = 0;
            Self->prvZip.avail_in  = 0;
            Self->prvZip.next_out  = 0;
            Self->prvZip.avail_out = 0;

            if (zf->CompressedSize > 0) {
               if (zf->DeflateMethod IS 0) {
                  // This routine is used if the file is stored rather than compressed

                  struct acRead read = { .Buffer = Self->prvInput, .Length = SIZE_COMPRESSION_BUFFER-1 };
                  if (!(error = Action(AC_Read, Self->FileIO, &read))) {
                     Self->prvInput[read.Result] = 0;
                     DeleteFile(location, NULL);
                     error = CreateLink(location, Self->prvInput);
                     if (error IS ERR_NoSupport) error = ERR_Okay;
                  }

                  if (error) goto exit;
               }
               else if ((zf->DeflateMethod IS 8) AND (inflateInit2(&Self->prvZip, -MAX_WBITS) IS ERR_Okay)) {
                  // Decompressing a link

                  inflateend = TRUE;

                  struct acRead read;
                  read.Buffer = Self->prvInput;
                  if (zf->CompressedSize < SIZE_COMPRESSION_BUFFER) read.Length = zf->CompressedSize;
                  else read.Length = SIZE_COMPRESSION_BUFFER;

                  ERROR err = ERR_Okay;
                  if ((error = Action(AC_Read, Self->FileIO, &read)) != ERR_Okay) goto exit;
                  if (read.Result <= 0) { error = ERR_Read; goto exit; }

                  Self->prvZip.next_in   = Self->prvInput;
                  Self->prvZip.avail_in  = read.Result;
                  Self->prvZip.next_out  = Self->prvOutput;
                  Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER-1;

                  err = inflate(&Self->prvZip, Z_SYNC_FLUSH);

                  if ((err != ERR_Okay) AND (err != Z_STREAM_END)) {
                     if (Self->prvZip.msg) LogF("!","%s", Self->prvZip.msg);
                     error = ERR_Failed;
                     goto exit;
                  }

                  Self->prvOutput[zf->OriginalSize] = 0; // !!! We should terminate according to the amount of data decompressed
                  DeleteFile(location, NULL);
                  error = CreateLink(location, Self->prvOutput);
                  if (error IS ERR_NoSupport) error = ERR_Okay;

                  inflateEnd(&Self->prvZip);
                  inflateend = FALSE;
               }
            }
         }
         else {
            // Create the destination file or folder

            LONG permissions;

            if (Self->Flags & CMF_APPLY_SECURITY) {
               if (zf->Flags & ZIP_SECURITY) {
                  permissions = 0;
                  if (zf->Flags & ZIP_UEXEC) permissions |= PERMIT_USER_EXEC;
                  if (zf->Flags & ZIP_GEXEC) permissions |= PERMIT_GROUP_EXEC;
                  if (zf->Flags & ZIP_OEXEC) permissions |= PERMIT_OTHERS_EXEC;

                  if (zf->Flags & ZIP_UREAD) permissions |= PERMIT_USER_READ;
                  if (zf->Flags & ZIP_GREAD) permissions |= PERMIT_GROUP_READ;
                  if (zf->Flags & ZIP_OREAD) permissions |= PERMIT_OTHERS_READ;

                  if (zf->Flags & ZIP_UWRITE) permissions |= PERMIT_USER_WRITE;
                  if (zf->Flags & ZIP_GWRITE) permissions |= PERMIT_GROUP_WRITE;
                  if (zf->Flags & ZIP_OWRITE) permissions |= PERMIT_OTHERS_WRITE;
               }
               else permissions = Self->Permissions;
            }
            else permissions = Self->Permissions;

            if ((error = CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&file,
                  FID_Location|TSTRING,  location,
                  FID_Flags|TLONG,       FL_NEW|FL_WRITE,
                  FID_Permissions|TLONG, permissions,
                  TAGEND)) != ERR_Okay) {
               LogErrorMsg("Error %d creating file \"%s\".", error, location);
               goto exit;
            }

            Self->prvZip.next_in   = 0;
            Self->prvZip.avail_in  = 0;
            Self->prvZip.next_out  = 0;
            Self->prvZip.avail_out = 0;

            if ((zf->CompressedSize > 0) AND (file->Flags & FL_FILE)) {
               if (zf->DeflateMethod IS 0) {
                  // This routine is used if the file is stored rather than compressed

                  MSG("Extracting file without compression.");

                  LONG inputlen = zf->CompressedSize;

                  struct acRead read = {
                     .Buffer = Self->prvInput,
                     .Length = (inputlen < SIZE_COMPRESSION_BUFFER) ? inputlen : SIZE_COMPRESSION_BUFFER
                  };

                  while ((!(error = Action(AC_Read, Self->FileIO, &read))) AND (read.Result > 0)) {
                     struct acWrite write = { .Buffer = Self->prvInput, .Length = read.Result };
                     if (Action(AC_Write, &file->Head, &write) != ERR_Okay) { error = PostError(ERR_Write); goto exit; }

                     inputlen -= read.Result;
                     if (inputlen <= 0) break;
                     if (inputlen < SIZE_COMPRESSION_BUFFER) read.Length = inputlen;
                     else read.Length = SIZE_COMPRESSION_BUFFER;
                  }

                  if (error) goto exit;
               }
               else if ((zf->DeflateMethod IS 8) AND (inflateInit2(&Self->prvZip, -MAX_WBITS) IS ERR_Okay)) {
                  // Decompressing a file

                  MSG("Inflating file from %d -> %d bytes @ offset %d.", zf->CompressedSize, zf->OriginalSize, zf->Offset);

                  inflateend = TRUE;

                  struct acRead read = {
                     .Buffer = Self->prvInput,
                     .Length = (zf->CompressedSize < SIZE_COMPRESSION_BUFFER) ? zf->CompressedSize : SIZE_COMPRESSION_BUFFER
                  };

                  if ((error = Action(AC_Read, Self->FileIO, &read)) != ERR_Okay) goto exit;
                  if (read.Result <= 0) { error = ERR_Read; goto exit; }
                  LONG inputlen = zf->CompressedSize - read.Result;

                  Self->prvZip.next_in   = Self->prvInput;
                  Self->prvZip.avail_in  = read.Result;
                  Self->prvZip.next_out  = Self->prvOutput;
                  Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;

                  // Keep loooping until Z_STREAM_END or an error is returned

                  ERROR err = ERR_Okay;
                  while (err IS ERR_Okay) {
                     err = inflate(&Self->prvZip, Z_SYNC_FLUSH);

                     if ((err != ERR_Okay) AND (err != Z_STREAM_END)) {
                        if (Self->prvZip.msg) LogF("!","%s", Self->prvZip.msg);
                        error = ERR_Failed;
                        goto exit;
                     }

                     // Write out the decompressed data

                     struct acWrite write = {
                        .Buffer = Self->prvOutput,
                        .Length = SIZE_COMPRESSION_BUFFER - Self->prvZip.avail_out
                     };
                     if (Action(AC_Write, &file->Head, &write) != ERR_Okay) { error = PostError(ERR_Write); goto exit; }

                     // Exit if all data has been written out

                     if (Self->prvZip.total_out IS zf->OriginalSize) break;

                     feedback.Progress = Self->prvZip.total_out;
                     send_feedback(Self, &feedback);

                     // Reset the output buffer

                     Self->prvZip.next_out  = Self->prvOutput;
                     Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;

                     // Read more data if necessary

                     if ((Self->prvZip.avail_in <= 0) AND (inputlen > 0)) {
                        if (inputlen < SIZE_COMPRESSION_BUFFER) read.Length = inputlen;
                        else read.Length = SIZE_COMPRESSION_BUFFER;

                        if ((error = Action(AC_Read, Self->FileIO, &read)) != ERR_Okay) goto exit;
                        if (read.Result <= 0) { error = ERR_Read; break; }
                        inputlen -= read.Result;

                        Self->prvZip.next_in  = Self->prvInput;
                        Self->prvZip.avail_in = read.Result;
                     }
                  }

                  // Terminate the inflation process

                  inflateEnd(&Self->prvZip);
                  inflateend = FALSE;
               }
            }

            // Give the file a date that matches the original

            flSetDate(file, feedback.Year, feedback.Month, feedback.Day, feedback.Hour, feedback.Minute, feedback.Second, 0);

            acFree(&file->Head);
            file = NULL;
         }

         if (feedback.Progress < feedback.OriginalSize) {
            feedback.Progress = feedback.OriginalSize; //100;
            send_feedback(Self, &feedback);
         }

         Self->prvFileIndex++;
      }
   }

   if (Self->OutputID) {
      StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "\nDecompression complete.");
      print(Self, Self->prvOutput);
   }

exit:
   if (inflateend) inflateEnd(&Self->prvZip);
   if (file) acFree(&file->Head);

   if ((error IS ERR_Okay) AND (Self->prvFileIndex <= 0)) {
      LogF("DecompressFile:","No files matched the path \"%s\".", Args->Path);
      error = ERR_Search;
   }

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
DecompressObject: Decompresses one file to a target object.

The DecompressObject method will decompress a file to a target object, using a series of #Write() calls.

This method sends feedback at regular intervals during decompression.  For further information on receiving feedback,
please refer to the #Feedback field.

Note that if decompressing to a @File object, the seek position will point to the end of the file after this
method returns.  Reset the seek position to zero if the decompressed data needs to be read back.

-INPUT-
cstr Path: The location of the source file within the archive.  If a wildcard is used, the first matching file is extracted.
obj Object: The target object for the decompressed source data.

-ERRORS-
Okay
NullArgs
MissingPath
Seek
Write
Failed

*****************************************************************************/

static ERROR COMPRESSION_DecompressObject(objCompression *Self, struct cmpDecompressObject *Args)
{
   if ((!Args) OR (!Args->Path) OR (!Args->Path[0])) return PostError(ERR_NullArgs);
   if (!Args->Object) return PostError(ERR_NullArgs);
   if (!Self->FileIO) return PostError(ERR_MissingPath);
   if (Self->Head.SubID) return ERR_NoSupport; // Object belongs to a Compression sub-class

   LogF("~DecompressObject()","%s TO %p, Permissions: $%.8x", Args->Path, Args->Object, Self->Permissions);

   BYTE inflateend = FALSE;
   Self->prvFileIndex = 0;

   struct CompressionFeedback fb;
   ClearMemory(&fb, sizeof(fb));

   struct ZipFile *list;
   LONG total_scanned = 0;
   for (list=Self->prvFiles; (list); list=(struct ZipFile *)list->Next) {
      total_scanned++;
      if (StrCompare(Args->Path, list->Name, 0, STR_WILDCARD)) continue;
      else break;
   }

   ERROR error = ERR_Okay;
   if (list) {
      MSG("Decompressing \"%s\"", list->Name);

      // Send compression feedback

      fb.Year   = 1980 + ((list->TimeStamp>>25) & 0x3f);
      fb.Month  = (list->TimeStamp>>21) & 0x0f;
      fb.Day    = (list->TimeStamp>>16) & 0x1f;
      fb.Hour   = (list->TimeStamp>>11) & 0x1f;
      fb.Minute = (list->TimeStamp>>5)  & 0x3f;
      fb.Second = (list->TimeStamp>>1)  & 0x0f;
      fb.FeedbackID     = FDB_DECOMPRESS_OBJECT;
      fb.Index          = Self->prvFileIndex;
      fb.Path           = list->Name;
      fb.Dest           = NULL;
      fb.OriginalSize   = list->OriginalSize;
      fb.CompressedSize = list->CompressedSize;
      fb.Progress       = 0;

      send_feedback(Self, &fb);

      // Seek to the start of the compressed data

      if (acSeek(Self->FileIO, list->Offset + HEAD_NAMELEN, SEEK_START) != ERR_Okay) {
         return LogBackError(0, ERR_Seek);
      }

      LONG namelen = read_word(Self->FileIO);
      LONG extralen = read_word(Self->FileIO);
      if (acSeek(Self->FileIO, namelen + extralen, SEEK_CURRENT) != ERR_Okay) {
         return LogBackError(0, ERR_Seek);
      }

      if (list->Flags & ZIP_LINK) { // For symbolic links, decompress the data to get the destination link string
         LogErrorMsg("Unable to unzip symbolic link %s (flags $%.8x), size %d.", list->Name, list->Flags, list->OriginalSize);
         return LogBackError(0, ERR_Failed);
      }
      else { // Create the destination file or folder
         Self->prvZip.next_in   = 0;
         Self->prvZip.avail_in  = 0;
         Self->prvZip.next_out  = 0;
         Self->prvZip.avail_out = 0;

         if (list->CompressedSize > 0) {
            if (list->DeflateMethod IS 0) {
               // This routine is used if the file is stored rather than compressed

               LONG inputlen = list->CompressedSize;

               struct acRead read;
               read.Buffer = Self->prvInput;
               if (inputlen < SIZE_COMPRESSION_BUFFER) read.Length = inputlen;
               else read.Length = SIZE_COMPRESSION_BUFFER;

               while ((!(error = Action(AC_Read, Self->FileIO, &read))) AND (read.Result > 0)) {
                  struct acWrite write;
                  write.Buffer = Self->prvInput;
                  write.Length = read.Result;
                  if (Action(AC_Write, Args->Object, &write) != ERR_Okay) { error = ERR_Write; goto exit; }

                  inputlen -= read.Result;
                  if (inputlen <= 0) break;
                  if (inputlen < SIZE_COMPRESSION_BUFFER) read.Length = inputlen;
                  else read.Length = SIZE_COMPRESSION_BUFFER;
               }

               if (error) goto exit;
            }
            else if ((list->DeflateMethod IS 8) AND (inflateInit2(&Self->prvZip, -MAX_WBITS) IS ERR_Okay)) {
               // Decompressing a file

               inflateend = TRUE;

               struct acRead read;
               read.Buffer = Self->prvInput;
               if (list->CompressedSize < SIZE_COMPRESSION_BUFFER) read.Length = list->CompressedSize;
               else read.Length = SIZE_COMPRESSION_BUFFER;

               ERROR err = ERR_Okay;
               if ((error = Action(AC_Read, Self->FileIO, &read)) != ERR_Okay) goto exit;
               if (read.Result <= 0) { error = ERR_Read; goto exit; }
               LONG inputlen = list->CompressedSize - read.Result;

               Self->prvZip.next_in   = Self->prvInput;
               Self->prvZip.avail_in  = read.Result;
               Self->prvZip.next_out  = Self->prvOutput;
               Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;

               // Keep loooping until Z_STREAM_END or an error is returned

               while (err IS ERR_Okay) {
                  err = inflate(&Self->prvZip, Z_SYNC_FLUSH);

                  if ((err != ERR_Okay) AND (err != Z_STREAM_END)) {
                     if (Self->prvZip.msg) LogF("!","%s", Self->prvZip.msg);
                     error = ERR_Decompression;
                     goto exit;
                  }

                  // Write out the decompressed data

                  struct acWrite write = { Self->prvOutput, SIZE_COMPRESSION_BUFFER - Self->prvZip.avail_out };
                  if (Action(AC_Write, Args->Object, &write) != ERR_Okay) { error = ERR_Write; goto exit; }

                  // Exit if all data has been written out

                  if (Self->prvZip.total_out IS list->OriginalSize) break;

                  fb.Progress = Self->prvZip.total_out;
                  send_feedback(Self, &fb);

                  // Reset the output buffer

                  Self->prvZip.next_out  = Self->prvOutput;
                  Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;

                  // Read more data if necessary

                  if ((Self->prvZip.avail_in <= 0) AND (inputlen > 0)) {
                     if (inputlen < SIZE_COMPRESSION_BUFFER) read.Length = inputlen;
                     else read.Length = SIZE_COMPRESSION_BUFFER;

                     if ((error = Action(AC_Read, Self->FileIO, &read)) != ERR_Okay) goto exit;
                     if (read.Result <= 0) { error = ERR_Read; break; }
                     inputlen -= read.Result;

                     Self->prvZip.next_in  = Self->prvInput;
                     Self->prvZip.avail_in = read.Result;
                  }
               }

               // Terminate the inflation process

               inflateEnd(&Self->prvZip);
               inflateend = FALSE;
            }
         }
      }

      if (fb.Progress < fb.OriginalSize) {
         fb.Progress = fb.OriginalSize; //100;
         send_feedback(Self, &fb);
      }

      Self->prvFileIndex++;
   }
   else {
      LogF("DecompressObject:","No files matched the path \"%s\" from %d files.", Args->Path, total_scanned);
      LogBack();
      return ERR_Search;
   }

exit:
   if (inflateend) inflateEnd(&Self->prvZip);
   if (error) PostError(error);
   LogBack();
   return error;
}
/*****************************************************************************

-METHOD-
Find: Find the first item that matches a given filter.

Use the Find method to search for a specific item in an archive.  The algorithm will return the first item that
matches the Path parameter string in conjunction with the options in Flags.  The options match those in the
~Core.StrCompare() function - in particular STR_CASE, STR_MATCH_LEN and STR_WILDCARD are the most
useful.

Please refer to the #Scan() method for a break-down of the CompressedItem structure that is returned by this
method.  The resulting structure is temporary and values will be discarded on the next call to this method.  If
persistent values are required, copy the resulting structure immediately after the call.

-INPUT-
cstr Path: Search for a specific item or items, using wildcards.
int(STR) Flags: String comparison flags used by StrCompare().
&struct(*CompressedItem) Item: The discovered item is returned in this parameter, or NULL if the search failed.

-ERRORS-
Okay
NoSupport
NullArgs
Search

*****************************************************************************/

static THREADVAR struct CompressedItem glFindMeta;

static ERROR COMPRESSION_Find(objCompression *Self, struct cmpFind *Args)
{
   if ((!Args) OR (!Args->Path)) return PostError(ERR_NullArgs);

   if (Self->Head.SubID) return ERR_NoSupport;

   FMSG("~","Path: %s, Flags: $%.8x", Args->Path, Args->Flags);
   for (struct ZipFile *item = Self->prvFiles; item; item = (struct ZipFile *)item->Next) {
      if (StrCompare(Args->Path, item->Name, 0, Args->Flags)) continue;

      zipfile_to_item(item, &glFindMeta);
      Args->Item = &glFindMeta;
      return ERR_Okay;
   }

   return ERR_Search;
}

/*****************************************************************************
-ACTION-
Flush: Flushes all pending actions.
-END-
*****************************************************************************/

static ERROR COMPRESSION_Flush(objCompression *Self, APTR Void)
{
   if (Self->Head.SubID) return ERR_Okay;

   Self->prvZip.avail_in = 0;

   BYTE done = FALSE;

   for (;;) {
      // Write out any bytes that are still left in the compression buffer

      LONG length, zerror;
      if ((length = SIZE_COMPRESSION_BUFFER - Self->prvZip.avail_out) > 0) {
         struct acWrite write = { Self->prvOutput, length };
         if (Action(AC_Write, Self->FileIO, &write) != ERR_Okay) return ERR_Write;
         Self->prvZip.next_out  = Self->prvOutput;
         Self->prvZip.avail_out = SIZE_COMPRESSION_BUFFER;
      }

      if (done) break;

      zerror = deflate(&Self->prvZip, Z_FINISH);

      // Ignore the second of two consecutive flushes:

      if ((!length) AND (zerror IS Z_BUF_ERROR)) zerror = ERR_Okay;

      done = ((Self->prvZip.avail_out != 0) OR (zerror IS Z_STREAM_END));

      if ((zerror != ERR_Okay) AND (zerror != Z_STREAM_END)) break;
   }

   acFlush(Self->FileIO);

   return ERR_Okay;
}

//****************************************************************************

static ERROR COMPRESSION_Free(objCompression *Self, APTR Void)
{
   // Write the end of file signature for the zip file if we have been making modifications to it.

   write_eof(Self);

   if (Self->ArchiveHash)  {
      remove_archive(Self);
      Self->ArchiveHash = 0;
   }

   // Free memory and resources

   struct ZipFile *chain, *next;
   for (chain=Self->prvFiles; chain != NULL; chain=next) {
      next = (struct ZipFile *)chain->Next;
      FreeResource(chain);
   }
   Self->prvFiles = NULL;

   if (Self->OutputBuffer) { FreeResource(Self->OutputBuffer); Self->OutputBuffer = NULL; }
   if (Self->prvInput)     { FreeResource(Self->prvInput); Self->prvInput = NULL; }
   if (Self->prvOutput)    { FreeResource(Self->prvOutput); Self->prvOutput = NULL; }
   if (Self->FileIO)       { acFree(Self->FileIO); Self->FileIO = NULL; }
   if (Self->Location)     { FreeResource(Self->Location); Self->Location = NULL; }

   return ERR_Okay;
}

//****************************************************************************

static ERROR COMPRESSION_Init(objCompression *Self, APTR Void)
{
   STRING path;
   GetString(Self, FID_Location, &path);

   if (!path) {
      // If no location has been set, assume that the developer only wants to use the buffer or stream compression routines.

      return ERR_Okay;
   }
   else if (Self->Flags & CMF_NEW) {
      // If the NEW flag is set then create a new archive, destroying any file already at that location

      if (!CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&Self->FileIO,
            FID_Location|TSTRING, path,
            FID_Flags|TLONG,      FL_READ|FL_WRITE|FL_NEW,
            TAGEND)) {

         return ERR_Okay;
      }
      else {
         if (Self->OutputID) {
            StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Failed to create file \"%s\".", path);
            print(Self, Self->prvOutput);
         }

         return PostError(ERR_CreateObject);
      }
   }
   else {
      ERROR error;
      BYTE exists;
      LONG type;

      if ((!AnalysePath(path, &type)) AND (type IS LOC_FILE)) exists = TRUE;
      else exists = FALSE;

      if (exists) {
         error = CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&Self->FileIO,
            FID_Location|TSTRING, path,
            FID_Flags|TLONG,      FL_READ|FL_APPROXIMATE|((Self->Flags & CMF_READ_ONLY) ? 0 : FL_WRITE),
            TAGEND);

         // Try switching to read-only access if we were denied permission.

         if ((error IS ERR_NoPermission) AND (!(Self->Flags & CMF_READ_ONLY))) {
            MSG("Trying read-only access...");
            if (!(error = CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&Self->FileIO,
               FID_Location|TSTRING, path,
               FID_Flags|TLONG,      FL_READ|FL_APPROXIMATE,
               TAGEND))) {
               Self->Flags |= CMF_READ_ONLY;
            }
         }
      }
      else error = ERR_DoesNotExist;

      if (!error) {
         // Test the given location to see if it matches our supported file format (pkzip).

         struct acRead read = { Self->Header, sizeof(Self->Header) };
         if (Action(AC_Read, Self->FileIO, &read) != ERR_Okay) return PostError(ERR_Read);

         // If the file is empty then we will accept it as a zip file

         if (read.Result IS 0) return ERR_Okay;

         // Check for a pkzip header

         if ((Self->Header[0] IS 0x50) AND (Self->Header[1] IS 0x4b) AND
             (Self->Header[2] IS 0x03) AND (Self->Header[3] IS 0x04)) {
            error = fast_scan_zip(Self);
            if (error != ERR_Okay) return PostError(error);
            else return error;
         }
         else return ERR_NoSupport;
      }
      else if ((!exists) AND (Self->Flags & CMF_CREATE_FILE)) {
         // Create a new file if the requested location does not exist

         LogMsg("Creating a new file because the location does not exist.");

         if (!CreateObject(ID_FILE, NF_INTEGRAL, (OBJECTPTR *)&Self->FileIO,
               FID_Path|TSTR,   path,
               FID_Flags|TLONG, FL_READ|FL_WRITE|FL_NEW,
               TAGEND)) {

            return ERR_Okay;
         }
         else {
            if (Self->OutputID) {
               StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Failed to create file \"%s\".", path);
               print(Self, Self->prvOutput);
            }

            return PostError(ERR_CreateObject);
         }
      }
      else {
         StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Failed to open \"%s\".", path);
         print(Self, Self->prvOutput);
         return PostError(ERR_File);
      }
   }
}

//****************************************************************************

static ERROR COMPRESSION_NewObject(objCompression *Self, APTR Void)
{
   if (!AllocMemory(SIZE_COMPRESSION_BUFFER, MEM_DATA, (APTR *)&Self->prvOutput, NULL)) {
      if (!AllocMemory(SIZE_COMPRESSION_BUFFER, MEM_DATA, (APTR *)&Self->prvInput, NULL)) {
         Self->CompressionLevel = 60; // 60% compression by default
         Self->Permissions      = 0; // Inherit permissions by default. PERMIT_READ|PERMIT_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE;
         Self->MinOutputSize    = (32 * 1024) + 2048; // Has to at least match the minimum 'window size' of each compression block, plus extra in case of overflow.  Min window size is typically 16k
         Self->WindowBits = MAX_WBITS; // If negative then you get raw compression when dealing with buffers and stream data, i.e. no header information
         return ERR_Okay;
      }
      else return PostError(ERR_AllocMemory);
   }
   else return PostError(ERR_AllocMemory);
}

/*****************************************************************************

-METHOD-
RemoveFile: Deletes one or more files from a compression object.

This method deletes compressed files from a compression object.  If the file is in a folder then the client must
specify the complete path in conjunction with the file name.  Wild cards are accepted if you want to delete multiple
files.  A Path setting of `*` will delete an archive's entire contents, while a more conservative Path of
`documents/ *` would delete all files and directories under the documents path.  Directories can be declared using
either the back-slash or the forward-slash characters.

Depending on internal optimisation techniques, the compressed file may not shrink from deletions until the compression
object is closed or the #Flush() action is called.

-INPUT-
cstr Path: The full path name of the file to delete from the archive.

-ERRORS-
Okay: The file was successfully deleted.
NullArgs
NoSupport
-END-

*****************************************************************************/

static ERROR COMPRESSION_RemoveFile(objCompression *Self, struct cmpRemoveFile *Args)
{
   if ((!Args) OR (!Args->Path)) return PostError(ERR_NullArgs);

   if (Self->Head.SubID) return ERR_NoSupport;

   // Search for the file(s) in our archive that match the given name and delete them.

   LogMsg("%s", Args->Path);

   struct ZipFile *filelist = Self->prvFiles;
   while (filelist) {
      if (!StrCompare(Args->Path, filelist->Name, 0, STR_WILDCARD)) {
         // Delete the file from the archive

         if (Self->OutputID) {
            StrFormat(Self->prvOutput, SIZE_COMPRESSION_BUFFER, "Removing file \"%s\".", filelist->Name);
            print(Self, Self->prvOutput);
         }

         ERROR error;
         if ((error = remove_file(Self, &filelist)) != ERR_Okay) return error;
      }
      else filelist = (struct ZipFile *)filelist->Next;
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Scan: Scan the archive's index of compressed data.

Use the Scan method to search an archive's list of items.  Optional filtering can be applied using the Folder parameter
to limit results to those within a folder, and Filter parameter to apply wildcard matching to item names.  Each item
that is discovered during the scan will be passed to the function referenced in the Callback parameter.  If the
Callback function returns ERR_Terminate, the scan will stop immediately.  The synopsis of the callback function is
`ERROR Function(*Compression, *CompressedItem)`.

The &CompressedItem structure consists of the following fields:

&CompressedItem

To search for a single item with a path and name already known, please use the #Find() method instead.

-INPUT-
cstr Folder: If defined, only items within the specified folder are returned.  Use an empty string for files in the root folder.
cstr Filter: Search for a specific item or items by name, using wildcards.  If NULL or an empty string, all items will be scanned.
ptr(func) Callback: This callback function will be called with a pointer to a CompressedItem structure.

-ERRORS-
Okay
NoSupport
NullArgs
-END-

*****************************************************************************/

static ERROR COMPRESSION_Scan(objCompression *Self, struct cmpScan *Args)
{
   if ((!Args) OR (!Args->Callback)) return PostError(ERR_NullArgs);

   if (Self->Head.SubID) return ERR_NoSupport;

   FMSG("~","Folder: \"%s\", Filter: \"%s\"", Args->Folder, Args->Filter);

   LONG folder_len = 0;
   if (Args->Folder) {
      folder_len = StrLength(Args->Folder);
      if ((folder_len > 0) AND (Args->Folder[folder_len-1] IS '/')) folder_len--;
   }

   ERROR error = ERR_Okay;

   for (struct ZipFile *item = Self->prvFiles; item; item = (struct ZipFile *)item->Next) {
      MSG("Item: %s", item->Name);

      if (Args->Folder) {
         LONG name_len = StrLength(item->Name);
         if (name_len > folder_len) {
            if (!StrCompare(Args->Folder, item->Name, 0, 0)) {
               if ((folder_len > 0) AND (item->Name[folder_len] != '/')) continue;
               if ((item->Name[folder_len] IS '/') AND (!item->Name[folder_len+1])) continue;

               // Skip this item if it is within other sub-folders.

               LONG i;
               for (i=folder_len+1; item->Name[i]; i++) {
                  if (item->Name[i] IS '/') break;
               }
               if (item->Name[i] IS '/') continue;
            }
            else continue;
         }
         else continue;
      }

      if ((Args->Filter) AND (Args->Filter[0])) {
         if (!StrCompare(Args->Filter, item->Name, 0, STR_WILDCARD)) break;
         else continue;
      }

      struct CompressedItem meta;
      zipfile_to_item(item, &meta);

      {
         if (Args->Callback->Type IS CALL_STDC) {
            ERROR (*routine)(objCompression *, struct CompressedItem *);
            OBJECTPTR context = SetContext(Args->Callback->StdC.Context);
               routine = Args->Callback->StdC.Routine;
               error = routine(Self, &meta);
            SetContext(context);
         }
         else if (Args->Callback->Type IS CALL_SCRIPT) {
            OBJECTPTR script = Args->Callback->Script.Script;
            if (script) {
               const struct ScriptArg args[] = {
                  { "Compression", FD_OBJECTPTR, { .Address = Self } },
                  { "CompressedItem:Item", FD_STRUCT|FD_PTR, { .Address = &meta } }
               };
               error = scCallback(script, Args->Callback->Script.ProcedureID, args, ARRAYSIZE(args));
               if (!error) GetLong(script, FID_Error, &error);
               else error = ERR_Failed;
            }
            else error = ERR_Terminate;
         }
         else error = PostError(ERR_WrongType);

         if (error) break; // Break the scanning loop.
      }
   }

   STEP();
   return error;
}

//****************************************************************************

#include "compression_fields.c"
#include "compression_func.c"

static const struct FieldDef clPermissionFlags[] = {
   { "Read",         PERMIT_READ },
   { "Write",        PERMIT_WRITE },
   { "Exec",         PERMIT_EXEC },
   { "Executable",   PERMIT_EXEC },
   { "Delete",       PERMIT_DELETE },
   { "Hidden",       PERMIT_HIDDEN },
   { "Archive",      PERMIT_ARCHIVE },
   { "Password",     PERMIT_PASSWORD },
   { "UserID",       PERMIT_USERID },
   { "GroupID",      PERMIT_GROUPID },
   { "OthersRead",   PERMIT_OTHERS_READ },
   { "OthersWrite",  PERMIT_OTHERS_WRITE },
   { "OthersExec",   PERMIT_OTHERS_EXEC },
   { "OthersDelete", PERMIT_OTHERS_DELETE },
   { "GroupRead",    PERMIT_GROUP_READ },
   { "GroupWrite",   PERMIT_GROUP_WRITE },
   { "GroupExec",    PERMIT_GROUP_EXEC },
   { "GroupDelete",  PERMIT_GROUP_DELETE },
   { "AllRead",      PERMIT_ALL_READ },
   { "AllWrite",     PERMIT_ALL_WRITE },
   { "AllExec",      PERMIT_ALL_EXEC },
   { NULL, 0 }
};

#include "class_compression_def.c"

static const struct FieldArray clFields[] = {
   { "TotalOutput",        FDF_LARGE|FDF_R,            0, NULL, NULL },
   { "Output",             FDF_OBJECTID|FDF_RI,        0, NULL, NULL },
   { "CompressionLevel",   FDF_LONG|FDF_RW,            0, NULL, SET_CompressionLevel },
   { "Flags",              FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clCompressionFlags, NULL, NULL },
   { "SegmentSize",        FDF_LONG|FDF_SYSTEM|FDF_RW, 0, NULL, NULL },
   { "Permissions",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPermissionFlags, NULL, NULL },
   { "MinOutputSize",      FDF_LONG|FDF_R,             0, NULL, NULL },
   { "WindowBits",         FDF_LONG|FDF_RW,            0, NULL, SET_WindowBits },
   // Virtual fields
   { "ArchiveName",      FDF_STRING|FDF_W,       0, NULL, SET_ArchiveName },
   { "Location",         FDF_STRING|FDF_RW,      0, GET_Location, SET_Location },
   { "Feedback",         FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "FeedbackInfo",     FDF_POINTER|FDF_STRUCT|FDF_R, (MAXINT)"CompressionFeedback", GET_FeedbackInfo, NULL },
   { "Header",           FDF_POINTER|FDF_R,      0, GET_Header,   NULL },
   { "Password",         FDF_STRING|FDF_RW,      0, GET_Password, SET_Password },
   { "Size",             FDF_LARGE|FDF_R,        0, GET_Size, NULL },
   { "Src",              FDF_SYNONYM|FDF_STRING|FDF_RW, 0, GET_Location, SET_Location },
   { "UncompressedSize", FDF_LARGE|FDF_R,        0, GET_UncompressedSize, NULL },
   END_FIELD
};
