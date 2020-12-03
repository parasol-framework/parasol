
//****************************************************************************
// This function targets a data file for opening.

static ERROR exec_data_file(CSTRING TargetFile)
{
   LogMsg("Executing target '%s' using the Task class.", TargetFile);

   CLASSID class_id;
   STRING command;
   ERROR error;

   if (!(error = IdentifyFile(TargetFile, "Open", 0, &class_id, NULL, &command))) {
      OBJECTPTR run;
      if (!CreateObject(ID_TASK, 0, &run, FID_Location|TSTR, command, TAGEND)) {
         if (glArgs) {
            char argbuffer[100];
            STRING argname = argbuffer;
            LONG i, j;
            for (i=0; glArgs[i]; i++) {
               CSTRING arg = glArgs[i];
               for (j=0; (arg[j]) AND (arg[j] != '=') AND (j < (LONG)sizeof(argbuffer)-10); j++) argname[j] = arg[j];
               argname[j] = 0;
               LONG al = j;

               if (arg[j] IS '=') {
                  j++;
                  if (arg[j] IS '{') {
                     // Array definition, e.g. files={ file1.txt file2.txt }
                     // This will be converted to files(0)=file.txt files(1)=file2.txt files:size=2

                     // Check in case of no gap, e.g. files={arg1 ... }

                     j++;
                     if (arg[j] > 0x20) SetVar(run, argbuffer, arg + j);

                     i++;
                     LONG arg_index = 0;
                     while ((arg) AND (arg[0] != '}')) {
                        StrFormat(argbuffer+al, sizeof(argbuffer)-al, "(%d)", arg_index);
                        SetVar(run, argbuffer, arg);
                        arg_index++;
                        i++;
                     }
                  }
                  else SetVar(run, argname, arg+j);
               }
               else SetVar(run, argname, "1");
            }
         }

         acActivate(run);
         acFree(run);
      }
   }

   return(ERR_LimitedSuccess);
}

//****************************************************************************
// Executes the target.

ERROR exec_source(CSTRING TargetFile, LONG ShowTime, CSTRING Procedure)
{
   LONG i, j;
   ERROR error;

   LogMsg("Identifying file '%s'", TargetFile);

   CLASSID class_id, subclass;
   if ((error = IdentifyFile(TargetFile, "Open", 0, &class_id, &subclass, NULL))) {
      print("Failed to identify the type of file for path '%s', error: %s.  Assuming ID_SCRIPT.", TargetFile, GetErrorMsg(error));
      subclass = ID_SCRIPT;
      class_id = ID_SCRIPT;
   }

   if (class_id IS ID_PARC) glSandbox = TRUE;

   if (glSandbox) {
      CSTRING *args = NULL;
      GetPointer(glTask, FID_Parameters, &args);

      #ifdef _WIN32
         IntegrityLevel il = get_integrity_level();
         if (il < INTEGRITY_LEVEL_LOW) {
            // If running with an integrity better than 'low', re-run the process with a low integrity.

            if (glRelaunched) return(ERR_Security);

            LogMsg("Inappropriate integrity level %d (must be %d or higher), re-launching...\n", il, INTEGRITY_LEVEL_LOW);

            BYTE cmdline[512];

            cmdline[0] = '"';
            ULONG i = get_exe(cmdline+1, sizeof(cmdline));
            if ((!i) OR (i >= sizeof(cmdline)-30)) return(ERR_Failed);
            i++;

            i += StrCopy("\" --relaunch", cmdline+i, sizeof(cmdline)-i);
            if (GetResource(RES_LOG_LEVEL) >= 5) i += StrCopy(" --log-debug", cmdline+i, sizeof(cmdline)-i);
            else if (GetResource(RES_LOG_LEVEL) >= 3) i += StrCopy(" --log-info", cmdline+i, sizeof(cmdline)-i);

            for (; *args; args++) {
               if (!StrMatch("--sandbox", *args)) continue;
               if (i < sizeof(cmdline)-2) {
                  cmdline[i++] = ' ';
                  cmdline[i++] ='"';
               }
               CSTRING arg = *args;
               while ((*arg) AND (i < sizeof(cmdline)-2)) {
                  if (*arg IS '"') cmdline[i++] = '\\'; // Escape '"'
                  cmdline[i++] = *arg++;
               }
               if (i < sizeof(cmdline)-1) cmdline[i++] = '"';
            }
            cmdline[i] = 0;
            if (i >= sizeof(cmdline)-3) return(ERR_BufferOverflow);

            // Temporarily switch off debug messages until the child process returns.

            LONG log_level = GetResource(RES_LOG_LEVEL);
            SetResource(RES_LOG_LEVEL, 1);

            create_low_process(cmdline, TRUE);

            SetResource(RES_LOG_LEVEL, log_level);

            return(ERR_LimitedSuccess);
         }

      #else
/*
         error = init_sandbox(args, glRelaunched ? FALSE : TRUE);
         if (error IS ERR_LimitedSuccess) {
            // Limited success means that the process was re-launched with a lower priority.
            return(error);
         }
         else if (error) {
            print("Sandbox initialisation failed.");
            return(error);
         }
*/
      #endif
   }

   if (class_id IS ID_PARC) {
      OBJECTPTR parc;

      if (!(error = CreateObject(ID_PARC, 0, &parc,
            FID_Path|TSTR,  TargetFile,
            FID_Allow|TSTR, glAllow,
            TAGEND))) {

         // The user can use the --allow parameter to automatically give the PARC program additional access
         // rights as required.  E.g. "--allow storage,maxSize:1M,maxFiles:20,memory:100M"

         if ((error = acActivate(parc))) {
            STRING msg;
            if (!GetString(parc, FID_Message, &msg)) {
               print("Failed to execute the archive, error: %s", GetErrorMsg(error));
            }
            else print("Failed to execute the archive, error: %s", GetErrorMsg(error));

            error = ERR_Activate;
         }
         else print("PARC execution completed successfully.");
      }
      else {
         print("Failed to initialise the PARC archive, error: %s", GetErrorMsg(error));
         error = ERR_CreateObject;
      }

      return(error);
   }
   else if (class_id != ID_SCRIPT) { // Target is a data file, may be able to run it if it has a file association.
      return(exec_data_file(TargetFile));
   }

   if (!NewObject(subclass ? subclass : class_id, 0, &glScript)) {
      if (!glTargetID) glTargetID = CurrentTaskID();
      SetFields(glScript, FID_Path|TSTR,      TargetFile,
                          FID_Target|TLONG,   glTargetID,
                          FID_Procedure|TSTR, Procedure,
                          TAGEND);
      if (glArgs) {
         BYTE argbuffer[100];
         STRING argname = argbuffer;
         for (i=0; glArgs[i]; i++) {
            for (j=0; (glArgs[i][j]) AND (glArgs[i][j] != '=') AND (j < (LONG)sizeof(argbuffer)-10); j++) argname[j] = glArgs[i][j];
            argname[j] = 0;
            LONG al = j;

            if (glArgs[i][j] IS '=') {
               j++;
               if (glArgs[i][j] IS '{') {
                  // Array definition, e.g. files={ file1.txt file2.txt }
                  // This will be converted to files(0)=file.txt files(1)=file2.txt

                  j++;
                  if (glArgs[i][j] > 0x20) SetVar(glScript, argname, glArgs[i] + j);

                  i++;
                  LONG arg_index = 0;
                  while ((glArgs[i]) AND (glArgs[i][0] != '}')) {
                     StrFormat(argname+al, sizeof(argbuffer)-al, "(%d)", arg_index);
                     SetVar(glScript, argname, glArgs[i]);
                     arg_index++;
                     i++;
                  }
                  if (!glArgs[i]) break;
                  // Note that the last arg in the array will be the "}" that closes it

                  char array_size[16];
                  StrCopy(":size", argname+al, sizeof(argbuffer)-al);
                  IntToStr(arg_index, array_size, sizeof(array_size));
                  SetVar(glScript, argname, array_size);
               }
               else SetVar(glScript, argname, glArgs[i]+j);
            }
            else SetVar(glScript, argname, "1");
         }
      }

      LARGE start_time = 0;
      if (ShowTime) start_time = PreciseTime();

      if (!(error = acInit(glScript))) {
         if (!(error = acActivate(glScript))) {
            if (ShowTime) { // Print the execution time of the script
               DOUBLE startseconds = (DOUBLE)start_time / 1000000.0;
               DOUBLE endseconds   = (DOUBLE)PreciseTime() / 1000000.0;
               print("Script executed in %f seconds.\n\n", endseconds - startseconds);
            }
         }
         else {
            print("Script failed during processing.  Use the --log-error option to examine the failure.");
            return(ERR_Failed);
         }
      }
      else {
         print("Failed to load / initialise the script.");
         return(ERR_Failed);
      }

      LogMsg("Script initialised.");
      return(ERR_Okay);
   }
   else {
      print("Internal Failure: Failed to create a new Script object for file processing.");
      return(ERR_Failed);
   }
}
