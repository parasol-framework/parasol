/****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Strings
-END-

****************************************************************************/

typedef enum { step_a, step_b, step_c, step_d } base64_decodestep;
typedef enum { step_A, step_B, step_C } base64_encodestep;

static int base64_decode_value(char value_in) __attribute__((unused));
static int base64_decode_block(const char* code_in, const int length_in, char* plaintext_out, struct rkBase64Decode * state_in) __attribute__((unused));
static void base64_init_encodestate(struct rkBase64Encode *state_in) __attribute__((unused));
static char base64_encode_value(char value_in) __attribute__((unused));
static int base64_encode_block(const char* plaintext_in, int length_in, char* code_out, struct rkBase64Encode * state_in) __attribute__((unused));
static int base64_encode_blockend(char* code_out, struct rkBase64Encode * state_in) __attribute__((unused));

/*****************************************************************************

-FUNCTION-
Base64Encode: Encodes a binary source into a base 64 string.

This function needs to be replaced with the streaming version, see base64_init_encodestate()

-INPUT-
buf(cptr) Input:     The binary data to encode.
bufsize InputSize:  The amount of data to encode.
buf(str) Output:    Destination buffer for the encoded output.
bufsize OutputSize: Size of the destination buffer.

-RESULT-
int: The total number of bytes output is returned.

*****************************************************************************/

LONG Base64Encode(const void *Input, LONG InputSize, STRING Output, LONG OutputSize)
{
   static const UBYTE enc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   UBYTE in[3], out[4];
   LONG i, len, pos, io;

   if ((!Input) or (!Output) or (OutputSize < 1)) return 0;

   UBYTE *inbuf = (UBYTE *)Input;
   for (pos=0,io=0; inbuf[pos]; ) {
      len = 0;
      for (i=0; i < 3; i++) {
         if (inbuf[pos]) {
            in[i] = inbuf[pos++];
            len++;
         }
         else in[i] = 0;
      }

      if (len) {
         out[0] = enc[in[0]>>2];
         out[1] = enc[((in[0] & 0x03)<<4) | ((in[1] & 0xf0)>>4) ];
         out[2] = (UBYTE)(len > 1 ? enc[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0)>>6)] : '=');
         out[3] = (UBYTE)(len > 2 ? enc[in[2] & 0x3f] : '=');
         for (i=0; (i < 4) and (io < OutputSize-1); i++,io++) {
            Output[io] = out[i];
         }
      }
   }

   Output[io] = 0;
   return io;
}

/*****************************************************************************

-FUNCTION-
Base64Decode: Decodes a base 64 string to its binary form.

This function will decode a base 64 string to its binary form.  It is designed to support streaming from the source
Input and gracefully handles buffer over-runs by forwarding data to the next call.

To use this function effectively, call it repeatedly in a loop until all of the input is exhausted.

-INPUT-
resource(rkBase64Decode) State: Pointer to an rkBase64Decode structure, initialised to zero.
cstr Input: A base 64 input string.  The pointer will be updated when the function returns.
bufsize InputSize: The size of the input string.
buf(ptr) Output:  The output buffer.  The size of the buffer must be greater or equal to the size of Input.
&int Written: The total number of bytes written to Output is returned here.

-ERRORS-
Okay
NullArgs
Args
-END-

*****************************************************************************/

// Output has to be >= Input's buffer size.

ERROR Base64Decode(struct rkBase64Decode *State, CSTRING Input, LONG InputSize, APTR Output, LONG *Written)
{
   if ((!State) or (!Input) or (!Output) or (!Written)) return ERR_NullArgs;
   if (InputSize < 4) return ERR_Args;

   if (!State->Initialised) {
      State->Initialised = TRUE;
      State->Step = step_a;
      State->PlainChar = 0;
   }

   *Written = base64_decode_block(Input, InputSize, (char *)Output, State);
   return ERR_Okay;
}

static int base64_decode_value(char value_in)
{
   static const char decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
   static const char decoding_size = sizeof(decoding);
   value_in -= 43;
   if (value_in < 0 || value_in > decoding_size) return -1;
   return decoding[(int)value_in];
}

static int base64_decode_block(const char* code_in, const int length_in, char* plaintext_out, struct rkBase64Decode *state_in)
{
   const char* codechar = code_in;
   char* plainchar = plaintext_out;
   char fragment;

   *plainchar = state_in->PlainChar;

   switch (state_in->Step) {
      while (1) {
         case step_a:
            do {
               if (codechar == code_in+length_in)
               {
                  state_in->Step = step_a;
                  state_in->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar    = (fragment & 0x03f) << 2;

         case step_b:
            do {
               if (codechar == code_in+length_in)
               {
                  state_in->Step = step_b;
                  state_in->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar++ |= (fragment & 0x030) >> 4;
            *plainchar    = (fragment & 0x00f) << 4;

         case step_c:
            do {
               if (codechar == code_in+length_in)
               {
                  state_in->Step = step_c;
                  state_in->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar++ |= (fragment & 0x03c) >> 2;
            *plainchar    = (fragment & 0x003) << 6;

         case step_d:
            do {
               if (codechar == code_in+length_in)
               {
                  state_in->Step = step_d;
                  state_in->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar++   |= (fragment & 0x03f);
      }
   }
   // control should not reach here
   return plainchar - plaintext_out;
}

const int CHARS_PER_LINE = 72;

void base64_init_encodestate(struct rkBase64Encode *state_in)
{
   state_in->Step = step_A;
   state_in->Result = 0;
   state_in->StepCount = 0;
}

char base64_encode_value(char value_in)
{
   static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   if (value_in > 63) return '=';
   return encoding[(int)value_in];
}

int base64_encode_block(const char* plaintext_in, int length_in, char* code_out, struct rkBase64Encode * state_in)
{
   const char* plainchar = plaintext_in;
   const char* const plaintextend = plaintext_in + length_in;
   char* codechar = code_out;
   char result;
   char fragment;

   result = state_in->Result;

   switch (state_in->Step) {
      while (1) {
         case step_A:
            if (plainchar == plaintextend) {
               state_in->Result = result;
               state_in->Step = step_A;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result = (fragment & 0x0fc) >> 2;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x003) << 4;

         case step_B:
            if (plainchar == plaintextend) {
               state_in->Result = result;
               state_in->Step = step_B;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result |= (fragment & 0x0f0) >> 4;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x00f) << 2;

         case step_C:
            if (plainchar == plaintextend) {
               state_in->Result = result;
               state_in->Step = step_C;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result |= (fragment & 0x0c0) >> 6;
            *codechar++ = base64_encode_value(result);
            result  = (fragment & 0x03f) >> 0;
            *codechar++ = base64_encode_value(result);

            ++(state_in->StepCount);
            if (state_in->StepCount == CHARS_PER_LINE/4) {
               *codechar++ = '\n';
               state_in->StepCount = 0;
            }
      }
   }
   // control should not reach here
   return codechar - code_out;
}

int base64_encode_blockend(char* code_out, struct rkBase64Encode * state_in)
{
   char* codechar = code_out;

   switch (state_in->Step) {
   case step_B:
      *codechar++ = base64_encode_value(state_in->Result);
      *codechar++ = '=';
      *codechar++ = '=';
      break;
   case step_C:
      *codechar++ = base64_encode_value(state_in->Result);
      *codechar++ = '=';
      break;
   case step_A:
      break;
   }
   *codechar++ = '\n';

   return codechar - code_out;
}
