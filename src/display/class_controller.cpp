/*********************************************************************************************************************

-CLASS-
Controller: Provides support for reading state-based game controllers.

Use the Controller class to read the state of game controllers that are recognised by the operating system.

Unlike analog devices that stream input commands (e.g. mice), gamepad controllers maintain a state that can be read 
at any time.  Typically the controller state is read at least once per frame, which can be achieved in the main
inner loop, or in a separate timer.

Controller input management is governed by the @Display class.  The `GRAB_CONTROLLERS` flag must be defined in the 
active Display's Flags field in order to ensure that controller input can be received.  Failure to do so may mean 
that the Controller object appears to work but does not receive input.

-END-

*********************************************************************************************************************/

#include "defs.h"

#ifdef _WIN32
using namespace display;
#endif

/*********************************************************************************************************************
-ACTION-
Query: Get the current controller state.
-END-
*********************************************************************************************************************/

static ERR CONTROLLER_Query(objController *Self)
{
#ifdef _WIN32
//   winReadController(Self->Port, (DOUBLE *)&Self->LeftTrigger);
   pf::Log log;
   log.msg("Control: %.2f %.2f", Self->LeftStickX, Self->LeftStickY); 
   return ERR::Okay;
#else
   return ERR::NoSupport;
#endif
}

/*********************************************************************************************************************

-FIELD-
LeftTrigger: Left trigger value between 0.0 and 1.0.

-FIELD-
RightTrigger: Right trigger value between 0.0 and 1.0.

-FIELD-
LeftStickX: Left analog stick value for X axis, between -1.0 and 1.0.

-FIELD-
LeftStickY: Left analog stick value for Y axis, between -1.0 and 1.0.

-FIELD-
RightStickX: Right analog stick value for X axis, between -1.0 and 1.0.

-FIELD-
RightStickY: Right analog stick value for Y axis, between -1.0 and 1.0.

-FIELD-
Buttons: JET button values expressed as bit-fields.

-FIELD-
Port: The port number assigned to the controller.

*********************************************************************************************************************/

//********************************************************************************************************************

#include "class_controller_def.c"

static const FieldArray clFields[] = {
   { "LeftTrigger",  FDF_DOUBLE|FDF_R },
   { "RightTrigger", FDF_DOUBLE|FDF_R },
   { "LeftStickX",   FDF_DOUBLE|FDF_R },
   { "LeftStickY",   FDF_DOUBLE|FDF_R },
   { "RightStickX",  FDF_DOUBLE|FDF_R },
   { "RightStickY",  FDF_DOUBLE|FDF_R },
   { "Buttons",      FDF_LONG|FDF_R },
   { "Port",         FDF_LONG|FDF_RI },
   END_FIELD
};

//********************************************************************************************************************

ERR create_controller_class(void)
{
   clController = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::CONTROLLER),
      fl::ClassVersion(VER_CONTROLLER),
      fl::Name("Controller"),
      fl::Category(CCF::IO),
      fl::Actions(clControllerActions),
      fl::Fields(clFields),
      fl::Size(sizeof(objController)),
      fl::Path(MOD_PATH));

   return clController ? ERR::Okay : ERR::AddClass;
}
