/*********************************************************************************************************************

-CLASS-
Controller: Provides support for reading state-based game controllers.

Use the Controller class to read the state of game controllers that are recognised by the operating system.

Unlike analog devices that stream input commands (e.g. mice), gamepad controllers maintain a state that can be read 
at any time.  The controller state is normally read at least once per frame, which can be achieved in the main
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
   if (auto error = winReadController(Self->Port, (DOUBLE *)&Self->LeftTrigger, Self->Buttons); error IS ERR::Okay) {
      return ERR::Okay;
   }
   else return error;
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

Set the port number to choose the controller that will be queried for state changes.  The default of zero is assigned
to the primary controller.

The port number can be changed at any time, so multiple controllers can be queried through one interface at the cost
of overwriting the previous state.  Check #TotalPorts if your program supports more than one controller.

-FIELD-
TotalPorts: Reports the total number of controllers connected to the system.

*********************************************************************************************************************/

static ERR CONTROLLER_GET_TotalPorts(extSurface *Self, LONG &Value)
{
#ifdef _WIN32
   if (glLastPort >= 0) Value = glLastPort;
   else Value = 0;
   return ERR::Okay;
#endif

   return ERR::NoSupport;
}

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
   { "TotalPorts",   FDF_VIRTUAL|FDF_LONG|FDF_R, CONTROLLER_GET_TotalPorts },
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
