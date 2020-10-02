
#include "ares_setup.h"
#include "ares.h"
#include "ares_dns.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

#include <parasol/system/errors.h>

extern struct ares_channeldata *glAres;

const char * net_init_ares(void)
{
   ares_library_init(ARES_LIB_INIT_ALL);

   LONG acode;
   if ((acode = ares_init(&glAres)) != ARES_SUCCESS) {
      return ares_strerror(acode);
   }
   else return 0;
}

void net_free_ares(void)
{
   if (glAres) {
      ares_cancel(glAres);
      ares_destroy(glAres);
      glAres = NULL;
   }

   ares_library_cleanup();
}

int net_ares_error(int Code, const char **Message)
{
   if (Message) *Message = ares_strerror(Code);

   switch(Code) {
      case ARES_ENODATA:   return ERR_NoData;
      case ARES_EFORMERR:  return ERR_InvalidData;
      case ARES_ESERVFAIL: return ERR_ConnectionAborted;
      case ARES_ENOTFOUND: return ERR_HostNotFound;
      case ARES_ENOTIMP:   return ERR_NoSupport;
      case ARES_EREFUSED:  return ERR_Cancelled;
   }

   return ERR_Failed;
}
