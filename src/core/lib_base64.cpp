/*********************************************************************************************************************

-CATEGORY-
Name: Strings
-END-

**********************************************************************************************************************/

typedef enum { step_a=0, step_b, step_c, step_d } base64_decodestep;
typedef enum { step_A=0, step_B, step_C } base64_encodestep;

static const char * encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};

const LONG CHARS_PER_LINE = 72;

static int base64_decode_block(CSTRING, LONG, char *, pfBase64Decode *);
static int base64_encode_block(CSTRING, LONG, char *, pfBase64Encode *);

inline LONG base64_decode_value(LONG value_in)
{
   value_in -= 43;
   if ((value_in < 0) or (value_in > (LONG)sizeof(decoding))) return -1;
   return decoding[(LONG)value_in];
}

inline char base64_encode_value(char value_in)
{
   if (value_in > 63) return '=';
   return encoding[(int)value_in];
}

/*********************************************************************************************************************

-FUNCTION-
Base64Encode: Encodes a binary source into a base 64 string.

This is a state-based function that will encode raw data and output it as base-64 encoded text.  To use, the `State`
structure must initially be set to zero (automatic if using C++).  Call this function repeatedly with new Input data
and it will be written to the supplied `Output` pointer.  Once all incoming data has been consumed, call this function
a final time with an `Input` of `NULL` and InputSize of zero.

It is required that the `Output` is sized to at least `(4 / 3) + 1` of `InputSize` when encoding.  For the final output,
the size must be at least 6 bytes.

-INPUT-
resource(pfBase64Encode) State: Pointer to an pfBase64Decode structure, initialised to zero.
buf(cptr) Input:    The binary data to encode.
bufsize InputSize:  The amount of data to encode.  Set to zero to finalise the output.
buf(str) Output:    Destination buffer for the encoded output.
bufsize OutputSize: Size of the destination buffer.  Must be at least (InputSize * 4 / 3) + 1.

-RESULT-
int: The total number of bytes output is returned.

**********************************************************************************************************************/

LONG Base64Encode(pfBase64Encode *State, const void *Input, LONG InputSize, STRING Output, LONG OutputSize)
{
   if ((!State) or (!Input) or (!Output) or (OutputSize < 1)) return 0;

   if (InputSize > 0) {
		return base64_encode_block((CSTRING)Input, InputSize, Output, State);
	}
   else { // Final output once all input consumed.
      if (OutputSize >= 6) {
         auto codechar = Output;

         switch (State->Step) {
            case step_B: // 3 bytes out
               *codechar++ = base64_encode_value(State->Result);
               *codechar++ = '=';
               *codechar++ = '=';
               break;
            case step_C: // 2 bytes out
               *codechar++ = base64_encode_value(State->Result);
               *codechar++ = '=';
               break;
            case step_A:
               break;
         }
         *codechar++ = '\n';
         *codechar++ = 0;

         return codechar - Output;
      }
      else return 0;
   }
}

static int base64_encode_block(CSTRING plaintext_in, LONG length_in, char *code_out, pfBase64Encode *State)
{
   const char *plainchar = plaintext_in;
   const char *const plaintextend = plaintext_in + length_in;
   char *codechar = code_out;
   char fragment;

   char result = State->Result;

   switch (State->Step) {
      while (true) {
         case step_A:
            if (plainchar == plaintextend) {
               State->Result = result;
               State->Step = step_A;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result = (fragment & 0x0fc) >> 2;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x003) << 4;

         case step_B:
            if (plainchar == plaintextend) {
               State->Result = result;
               State->Step = step_B;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result |= (fragment & 0x0f0) >> 4;
            *codechar++ = base64_encode_value(result);
            result = (fragment & 0x00f) << 2;

         case step_C:
            if (plainchar == plaintextend) {
               State->Result = result;
               State->Step = step_C;
               return codechar - code_out;
            }
            fragment = *plainchar++;
            result |= (fragment & 0x0c0) >> 6;
            *codechar++ = base64_encode_value(result);
            result  = (fragment & 0x03f) >> 0;
            *codechar++ = base64_encode_value(result);

            ++(State->StepCount);
            if (State->StepCount == CHARS_PER_LINE/4) {
               *codechar++ = '\n';
               State->StepCount = 0;
            }
      }
   }
   // control should not reach here
   return codechar - code_out;
}

/*********************************************************************************************************************

-FUNCTION-
Base64Decode: Decodes a base 64 string to its binary form.

This function will decode a base 64 string to its binary form.  It is designed to support streaming from the source
`Input` and gracefully handles buffer over-runs by forwarding data to the next call.

To use this function effectively, call it repeatedly in a loop until all of the input is exhausted.

-INPUT-
resource(pfBase64Decode) State: Pointer to an pfBase64Decode structure, initialised to zero.
cstr Input: A base 64 input string.  The pointer will be updated when the function returns.
bufsize InputSize: The size of the `Input` string.
buf(ptr) Output:  The output buffer.  The size of the buffer must be greater or equal to the size of Input.
&int Written: The total number of bytes written to `Output` is returned here.

-ERRORS-
Okay
NullArgs
Args
-END-

**********************************************************************************************************************/

ERR Base64Decode(pfBase64Decode *State, CSTRING Input, LONG InputSize, APTR Output, LONG *Written)
{
   if ((!State) or (!Input) or (!Output) or (!Written)) return ERR::NullArgs;
   if (InputSize < 4) return ERR::Args;

   if (!State->Initialised) {
      State->Initialised = TRUE;
      State->Step        = step_a;
      State->PlainChar   = 0;
   }

   *Written = base64_decode_block(Input, InputSize, (char *)Output, State);
   return ERR::Okay;
}

static LONG base64_decode_block(CSTRING code_in, LONG length_in, char * plaintext_out, pfBase64Decode *State)
{
   const char* codechar = code_in;
   char* plainchar = plaintext_out;
   char fragment;

   *plainchar = State->PlainChar;

   switch (State->Step) {
      while (1) {
         case step_a:
            do {
               if (codechar == code_in+length_in) {
                  State->Step = step_a;
                  State->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar    = (fragment & 0x03f) << 2;

         case step_b:
            do {
               if (codechar == code_in+length_in) {
                  State->Step = step_b;
                  State->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar++ |= (fragment & 0x030) >> 4;
            *plainchar    = (fragment & 0x00f) << 4;

         case step_c:
            do {
               if (codechar == code_in+length_in) {
                  State->Step = step_c;
                  State->PlainChar = *plainchar;
                  return plainchar - plaintext_out;
               }
               fragment = (char)base64_decode_value(*codechar++);
            } while (fragment < 0);
            *plainchar++ |= (fragment & 0x03c) >> 2;
            *plainchar    = (fragment & 0x003) << 6;

         case step_d:
            do {
               if (codechar == code_in+length_in) {
                  State->Step = step_d;
                  State->PlainChar = *plainchar;
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
