// ReverseShellDLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"


#define DllExport  extern "C" __declspec( dllexport )


DllExport void SometimesBinariesVerifyExportsMatch() {};
DllExport void BeforeLoadingDLLs() {};
DllExport void YouMightHaveto() {};
DllExport void ChangeTheseExports() {};
DllExport void ToMatchTheTargetBinary() {};
