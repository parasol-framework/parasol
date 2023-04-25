
//********************************************************************************************************************

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
