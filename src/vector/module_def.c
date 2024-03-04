// Auto-generated by idl-c.fluid

#ifndef FDEF
#define FDEF static const struct FunctionField
#endif

FDEF argsApplyPath[] = { { "Error", FD_LONG|FD_ERROR }, { "Path", FD_PTR }, { "VectorPath", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsArcTo[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "RX", FD_DOUBLE }, { "RY", FD_DOUBLE }, { "Angle", FD_DOUBLE }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Flags", FD_LONG }, { 0, 0 } };
FDEF argsClosePath[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { 0, 0 } };
FDEF argsCurve3[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "CtrlX", FD_DOUBLE }, { "CtrlY", FD_DOUBLE }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsCurve4[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "CtrlX1", FD_DOUBLE }, { "CtrlY1", FD_DOUBLE }, { "CtrlX2", FD_DOUBLE }, { "CtrlY2", FD_DOUBLE }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsDrawPath[] = { { "Error", FD_LONG|FD_ERROR }, { "Bitmap", FD_OBJECTPTR }, { "Path", FD_PTR }, { "StrokeWidth", FD_DOUBLE }, { "StrokeStyle", FD_OBJECTPTR }, { "FillStyle", FD_OBJECTPTR }, { 0, 0 } };
FDEF argsFreePath[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { 0, 0 } };
FDEF argsGenerateEllipse[] = { { "Error", FD_LONG|FD_ERROR }, { "CX", FD_DOUBLE }, { "CY", FD_DOUBLE }, { "RX", FD_DOUBLE }, { "RY", FD_DOUBLE }, { "Vertices", FD_LONG }, { "Path", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsGeneratePath[] = { { "Error", FD_LONG|FD_ERROR }, { "Sequence", FD_STR }, { "Path", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsGenerateRectangle[] = { { "Error", FD_LONG|FD_ERROR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { "Width", FD_DOUBLE }, { "Height", FD_DOUBLE }, { "Path", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsGetFontHandle[] = { { "Error", FD_LONG|FD_ERROR }, { "Family", FD_STR }, { "Style", FD_STR }, { "Weight", FD_LONG }, { "Size", FD_LONG }, { "Handle", FD_PTR|FD_RESULT }, { 0, 0 } };
FDEF argsGetFontMetrics[] = { { "Error", FD_LONG|FD_ERROR }, { "Handle", FD_PTR }, { "FontMetrics:Info", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsGetVertex[] = { { "Result", FD_LONG }, { "Path", FD_PTR }, { "X", FD_DOUBLE|FD_RESULT }, { "Y", FD_DOUBLE|FD_RESULT }, { 0, 0 } };
FDEF argsLineTo[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsMoveTo[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsMultiply[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "ScaleX", FD_DOUBLE }, { "ShearY", FD_DOUBLE }, { "ShearX", FD_DOUBLE }, { "ScaleY", FD_DOUBLE }, { "TranslateX", FD_DOUBLE }, { "TranslateY", FD_DOUBLE }, { 0, 0 } };
FDEF argsMultiplyMatrix[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Target", FD_PTR|FD_STRUCT }, { "VectorMatrix:Source", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsParseTransform[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "Transform", FD_STR }, { 0, 0 } };
FDEF argsReadPainter[] = { { "Error", FD_LONG|FD_ERROR }, { "Scene", FD_OBJECTPTR }, { "IRI", FD_STR }, { "VectorPainter:Painter", FD_PTR|FD_STRUCT }, { "Result", FD_STR|FD_RESULT }, { 0, 0 } };
FDEF argsResetMatrix[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { 0, 0 } };
FDEF argsRewindPath[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { 0, 0 } };
FDEF argsRotate[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "Angle", FD_DOUBLE }, { "CenterX", FD_DOUBLE }, { "CenterY", FD_DOUBLE }, { 0, 0 } };
FDEF argsScale[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsSkew[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsSmooth3[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsSmooth4[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "CtrlX", FD_DOUBLE }, { "CtrlY", FD_DOUBLE }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsStringWidth[] = { { "Result", FD_DOUBLE }, { "FontHandle", FD_PTR }, { "String", FD_STR }, { "Chars", FD_LONG }, { 0, 0 } };
FDEF argsTranslate[] = { { "Error", FD_LONG|FD_ERROR }, { "VectorMatrix:Matrix", FD_PTR|FD_STRUCT }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };
FDEF argsTranslatePath[] = { { "Void", FD_VOID }, { "Path", FD_PTR }, { "X", FD_DOUBLE }, { "Y", FD_DOUBLE }, { 0, 0 } };

const struct Function glFunctions[] = {
   { (APTR)vecDrawPath, "DrawPath", argsDrawPath },
   { (APTR)vecFreePath, "FreePath", argsFreePath },
   { (APTR)vecGenerateEllipse, "GenerateEllipse", argsGenerateEllipse },
   { (APTR)vecGeneratePath, "GeneratePath", argsGeneratePath },
   { (APTR)vecGenerateRectangle, "GenerateRectangle", argsGenerateRectangle },
   { (APTR)vecReadPainter, "ReadPainter", argsReadPainter },
   { (APTR)vecTranslatePath, "TranslatePath", argsTranslatePath },
   { (APTR)vecMoveTo, "MoveTo", argsMoveTo },
   { (APTR)vecLineTo, "LineTo", argsLineTo },
   { (APTR)vecArcTo, "ArcTo", argsArcTo },
   { (APTR)vecCurve3, "Curve3", argsCurve3 },
   { (APTR)vecSmooth3, "Smooth3", argsSmooth3 },
   { (APTR)vecCurve4, "Curve4", argsCurve4 },
   { (APTR)vecSmooth4, "Smooth4", argsSmooth4 },
   { (APTR)vecClosePath, "ClosePath", argsClosePath },
   { (APTR)vecRewindPath, "RewindPath", argsRewindPath },
   { (APTR)vecGetVertex, "GetVertex", argsGetVertex },
   { (APTR)vecApplyPath, "ApplyPath", argsApplyPath },
   { (APTR)vecRotate, "Rotate", argsRotate },
   { (APTR)vecTranslate, "Translate", argsTranslate },
   { (APTR)vecSkew, "Skew", argsSkew },
   { (APTR)vecMultiply, "Multiply", argsMultiply },
   { (APTR)vecMultiplyMatrix, "MultiplyMatrix", argsMultiplyMatrix },
   { (APTR)vecScale, "Scale", argsScale },
   { (APTR)vecParseTransform, "ParseTransform", argsParseTransform },
   { (APTR)vecResetMatrix, "ResetMatrix", argsResetMatrix },
   { (APTR)vecGetFontHandle, "GetFontHandle", argsGetFontHandle },
   { (APTR)vecGetFontMetrics, "GetFontMetrics", argsGetFontMetrics },
   { (APTR)vecStringWidth, "StringWidth", argsStringWidth },
   { NULL, NULL, NULL }
};

