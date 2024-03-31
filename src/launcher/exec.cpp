
//********************************************************************************************************************
// Executes the target.

ERR exec_source(CSTRING TargetFile, LONG ShowTime, const std::string Procedure)
{
   pf::Log log(__FUNCTION__);
   ERR error;

   log.msg("Identifying file '%s'", TargetFile);

   CLASSID class_id, subclass;
   if ((error = IdentifyFile(TargetFile, &class_id, &subclass)) != ERR::Okay) {
      printf("Failed to identify the type of file for path '%s', error: %s.  Assuming ID_SCRIPT.\n", TargetFile, GetErrorMsg(error));
      subclass = ID_SCRIPT;
      class_id = ID_SCRIPT;
   }

   if (class_id IS ID_PARC) glSandbox = TRUE;

   if (glSandbox) {
      pf::vector<std::string> *params = NULL;
      glTask->getPtr(FID_Parameters, &params);

      #ifdef _WIN32
         IntegrityLevel il = get_integrity_level();
         if (il < INTEGRITY_LEVEL_LOW) {
            // If running with an integrity better than 'low', re-run the process with a low integrity.

            if (glRelaunched) return ERR::Security;

            log.msg("Inappropriate integrity level %d (must be %d or higher), re-launching...\n", il, INTEGRITY_LEVEL_LOW);

            BYTE cmdline[512];

            cmdline[0] = '"';
            ULONG i = get_exe(cmdline+1, sizeof(cmdline));
            if ((!i) or (i >= sizeof(cmdline)-30)) return ERR::Failed;
            i++;

            i += StrCopy("\" --relaunch", cmdline+i, sizeof(cmdline)-i);
            if (GetResource(RES::LOG_LEVEL) >= 5) i += StrCopy(" --log-debug", cmdline+i, sizeof(cmdline)-i);
            else if (GetResource(RES::LOG_LEVEL) >= 3) i += StrCopy(" --log-info", cmdline+i, sizeof(cmdline)-i);

            pf::vector<std::string> &args = *params;
            for (unsigned a=0; a < args.size(); a++) {
               if (StrMatch("--sandbox", args[a]) IS ERR::Okay) continue;

               if (i < sizeof(cmdline)-2) {
                  cmdline[i++] = ' ';
                  cmdline[i++] ='"';
               }

               auto arg = args[a].c_str();
               while ((*arg) and (i < sizeof(cmdline)-2)) {
                  if (*arg IS '"') cmdline[i++] = '\\'; // Escape '"'
                  cmdline[i++] = *arg++;
               }
               if (i < sizeof(cmdline)-1) cmdline[i++] = '"';
            }
            cmdline[i] = 0;
            if (i >= sizeof(cmdline)-3) return ERR::BufferOverflow;

            // Temporarily switch off debug messages until the child process returns.

            LONG log_level = GetResource(RES::LOG_LEVEL);
            SetResource(RES::LOG_LEVEL, 1);

            create_low_process(cmdline, TRUE);

            SetResource(RES::LOG_LEVEL, log_level);

            return ERR::LimitedSuccess;
         }

      #else
/*
         error = init_sandbox(args, glRelaunched ? FALSE : TRUE);
         if (error IS ERR::LimitedSuccess) {
            // Limited success means that the process was re-launched with a lower priority.
            return error;
         }
         else if (error) {
            printf("Sandbox initialisation failed.\n");
            return error;
         }
*/
      #endif
   }
#if 0
   if (class_id IS ID_PARC) {
      objParc::create parc = { fl::Path(TargetFile), fl::Allow(glAllow) };

      if (parc.ok()) {
         // The user can use the --allow parameter to automatically give the PARC program additional access
         // rights as required.  E.g. "--allow storage,maxSize:1M,maxFiles:20,memory:100M"

         if ((error = parc->activate())) {
            STRING msg;
            if (!parc->get(FID_Message, &msg)) {
               printf("Failed to execute the archive, error: %s\n", GetErrorMsg(error));
            }
            else printf("Failed to execute the archive, error: %s\n", GetErrorMsg(error));

            error = ERR::Activate;
         }
         else printf("PARC execution completed successfully.\n");
      }
      else {
         printf("Failed to initialise the PARC archive, error: %s\n", GetErrorMsg(error));
         error = ERR::CreateObject;
      }

      return error;
   }
#endif
   if (NewObject(subclass ? subclass : class_id, &glScript) IS ERR::Okay) {
      glScript->setTarget(glTarget ? glTarget->UID : CurrentTaskID());
      glScript->setPath(TargetFile);

      if (!Procedure.empty()) glScript->setProcedure(Procedure);

      if (glArgsIndex) {
         pf::vector<std::string> &args = *glArgs;

         for (unsigned i=glArgsIndex; i < args.size(); i++) {
            auto eq = args[i].find('=');
            if (eq IS std::string::npos) SetVar(glScript, args[i].c_str(), "1");
            else {
               auto argname = std::string(args[i], 0, eq);
               eq++;
               if (args[i][eq] IS '{') {
                  // Array definition, e.g. files={ file1.txt file2.txt }
                  // This will be converted to files(0)=file.txt files(1)=file2.txt

                  if (args[i][eq+1] > 0x20) SetVar(glScript, argname.c_str(), args[i].c_str() + eq);
                  else {
                     unsigned arg_index = 0;
                     for (++i; (i < args.size()) and (args[i][0] != '}'); i++) {
                        auto argindex = argname + '(' + std::to_string(arg_index) + ')';
                        SetVar(glScript, argindex.c_str(), args[i].c_str());
                        arg_index++;
                     }

                     if (i >= args.size()) break;

                     // Note that the last arg in the array will be the "}" that closes it

                     SetVar(glScript, (argname + ":size").c_str(), std::to_string(arg_index).c_str());
                  }
               }
               else SetVar(glScript, argname.c_str(), args[i].c_str() + eq);
            }
         }
      }

      LARGE start_time = 0;
      if (ShowTime) start_time = PreciseTime();

      if (auto error = InitObject(glScript); error IS ERR::Okay) {
         if (auto error = acActivate(glScript); error IS ERR::Okay) {
            if (ShowTime) { // Print the execution time of the script
               auto start_seconds = (DOUBLE)start_time / 1000000.0;
               auto end_seconds   = (DOUBLE)PreciseTime() / 1000000.0;
               printf("Script executed in %f seconds.\n\n", end_seconds - start_seconds);
            }

            if (glScript->Error != ERR::Okay) {
               log.msg("Script returned an error code of %d: %s", LONG(glScript->Error), GetErrorMsg(glScript->Error));
               return glScript->Error;
            }

            STRING msg;
            if ((glScript->get(FID_ErrorString, &msg) IS ERR::Okay) and (msg)) {
               log.msg("Script returned error message: %s", msg);
               return ERR::Failed;
            }
            else return ERR::Okay;
         }
         else {
            printf("Script failed during processing.  Use the --log-error option to examine the failure.\n");
            return ERR::Failed;
         }
      }
      else {
         printf("Failed to load / initialise the script.\n");
         return ERR::Failed;
      }
   }
   else {
      printf("Internal Failure: Failed to create a new Script object for file processing.\n");
      return ERR::Failed;
   }
}
