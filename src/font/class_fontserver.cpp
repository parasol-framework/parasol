
// OBSOLETE CODE

static ERROR FONTSERVER_GetVar(struct FontServer *Self, struct acGetVar *Args)
{
   struct mtGetConfigSectionFromIndex getsection;
   struct mtReadConfig read;
   OBJECTPTR config;
   LONG index, i, pos, stylecount;

   if (!AccessObject(Self->prvConfigID, 3000, &config)) {
      if (!StrCompare("FontName(", Args->Field, 0, 0)) {
         getsection.Index = StrToInt(Args->Field);
         if (!Action(MT_GetConfigSectionFromIndex, config, &getsection)) {
            read.Section = getsection.Result;
            read.Item = "Name";
            if (!Action(MT_ReadConfig, config, &read)) {
               StrCopy(read.Data, Args->Buffer, Args->Size);
            }
         }
         else {
            ReleaseObject(config);
            return PostError(ERR_OutOfRange);
         }
      }
      else if (!StrCompare("FontStyle(", Args->Field, 0, 0)) {
         getsection.Index = StrToInt(Args->Field);
         if (!Action(MT_GetConfigSectionFromIndex, config, &getsection)) {
            read.Section = getsection.Result;
            read.Item = "Styles";
            if (!Action(MT_ReadConfig, config, &read)) {
               for (i=10; Args->Field[i]; i++) if ((Args->Field[i] >= '0') AND (Args->Field[i] <= '9')) break;
               while ((Args->Field[i] >= '0') AND (Args->Field[i] <= '9')) i++;
               index = StrToInt(Args->Field+i);

               for (i=0; (read.Data[i]) AND (index > 0); i++) {
                  if (read.Data[i] IS ',') index--;
               }

               pos = 0;
               while ((read.Data[i]) AND (read.Data[i] != ',') AND (pos < Args->Size-1)) Args->Buffer[pos++] = read.Data[i++];
               Args->Buffer[pos] = 0;
            }
         }
         else {
            ReleaseObject(config);
            return PostError(ERR_OutOfRange);
         }
      }
      else if (!StrCompare("StyleCount(", Args->Field, 0, 0)) {
         getsection.Index = StrToInt(Args->Field);
         if (!Action(MT_GetConfigSectionFromIndex, config, &getsection)) {
            read.Section = getsection.Result;
            read.Item = "Styles";
            if (!Action(MT_ReadConfig, config, &read)) {
               stylecount = 1;
               for (i=0; read.Data[i]; i++) if (read.Data[i] IS ',') stylecount++;
               IntToStr(stylecount, Args->Buffer, Args->Size);
            }
            else IntToStr(0, Args->Buffer, Args->Size);
         }
         else {
            ReleaseObject(config);
            return PostError(ERR_OutOfRange);
         }
      }
      else {
         LogErrorMsg("Unrecognised field name \"%s\".", Args->Field);
         ReleaseObject(config);
         return ERR_Failed;
      }

      ReleaseObject(config);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessObject);
}

static ERROR GET_TotalFonts(struct FontServer *Self, LONG *Value)
{
   OBJECTPTR config;

   if (!AccessObject(Self->prvConfigID, 3000, &config)) {
      GetLong(config, FID_TotalSections, Value);
      ReleaseObject(config);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessObject);
}
