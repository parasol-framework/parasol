
//****************************************************************************

extern "C" void print(CSTRING text, ...)
{
   va_list arg;
   va_start(arg, text);
   vprintf(text, arg);
   va_end(arg);

   LONG i;
   for (i=0; text[i]; i++);
   if (text[i-1] != '\n') printf("\n");
}

//****************************************************************************

static ERROR PROGRAM_DataFeed(OBJECTPTR Task, struct acDataFeed *Args)
{
   if (Args->DataType IS DATA_TEXT) {
      STRING buffer;
      if (!AllocMemory(Args->Size+1, MEM_NO_CLEAR|MEM_STRING, &buffer, NULL)) {
         LONG i;
         for (i=0; i < Args->Size; i++) buffer[i] = ((UBYTE *)Args->Buffer)[i];
         buffer[i] = 0;

         printf("%s\n", buffer);
         FreeResource(buffer);
      }
   }

   return ERR_Okay;
}
