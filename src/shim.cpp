// Shim.cpp - Proxies calls to the Core DLL
#include "fmi2FunctionTypes.h"
#include <windows.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <type_traits> // For std::remove_reference_t
#include <cstdio>      // For fopen, fprintf

#if defined _WIN32 || defined __CYGWIN__
  #define FMI2_Export __declspec(dllexport)
#else
  #define FMI2_Export __attribute__ ((visibility ("default")))
#endif

namespace fs = std::filesystem;

// Logging Helper
// Logging Helper
void Log(const std::string& msg) {
    static std::string logPath = "";
    if (logPath.empty()) {
        HMODULE hShim = NULL;
        // Use the address of Log itself to find the HMODULE
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Log, &hShim)) {
            char pathBuf[MAX_PATH];
            if (GetModuleFileNameA(hShim, pathBuf, MAX_PATH) != 0) {
                fs::path shimPath(pathBuf);
                logPath = (shimPath.parent_path() / "GT-DriveController_Shim.log").string();
            }
        }
        
        // Fallback to temp if we somehow couldn't determine path
        if (logPath.empty()) {
            char tempPath[MAX_PATH];
            GetTempPathA(MAX_PATH, tempPath);
            logPath = std::string(tempPath) + "GT-DriveController_Shim.log";
        }
    }
    
    FILE* f = fopen(logPath.c_str(), "a");
    if (f) {
        fprintf(f, "[Shim] %s\n", msg.c_str());
        fclose(f);
    }
}

void LogError(const std::string& msg, DWORD errCode) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s (Error: %lu)", msg.c_str(), errCode);
    Log(buf);
}

// DllMain - Entry point for the DLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Log immediately upon tracking
        // We cannot reliably predict path here if GetModuleFileName fails, but we try.
        // Also note: Calling complex functions in DllMain is risky, but fopen is usually okay if CRT is initialized.
        // We use a safe/simple logging path first: Temp dir.
        
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string logPath = std::string(tempPath) + "GT-DriveController_Shim_Boot.log";
        
        FILE* f = fopen(logPath.c_str(), "a");
        if (f) {
            fprintf(f, "[Shim:DllMain] PROCESS_ATTACH. DLL Base: %p\n", hinstDLL);
            fclose(f);
        }
        
        // Also try to log to our main log function if possible
        try {
             Log("DllMain: PROCESS_ATTACH");
        } catch (...) {
            // Ignore errors here
        }
    }
    return TRUE;
}

// The actual implementation DLL name
const wchar_t* CORE_DLL_NAME = L"GT-DriveController_Core.dll";

// Function pointers for FMI2 functions
static struct FMI2Functions {
    fmi2InstantiateTYPE* fmi2Instantiate;
    fmi2FreeInstanceTYPE* fmi2FreeInstance;
    fmi2SetupExperimentTYPE* fmi2SetupExperiment;
    fmi2EnterInitializationModeTYPE* fmi2EnterInitializationMode;
    fmi2ExitInitializationModeTYPE* fmi2ExitInitializationMode;
    fmi2TerminateTYPE* fmi2Terminate;
    fmi2ResetTYPE* fmi2Reset;
    fmi2GetRealTYPE* fmi2GetReal;
    fmi2GetIntegerTYPE* fmi2GetInteger;
    fmi2GetBooleanTYPE* fmi2GetBoolean;
    fmi2GetStringTYPE* fmi2GetString;
    fmi2SetRealTYPE* fmi2SetReal;
    fmi2SetIntegerTYPE* fmi2SetInteger;
    fmi2SetBooleanTYPE* fmi2SetBoolean;
    fmi2SetStringTYPE* fmi2SetString;
    fmi2GetFMUstateTYPE* fmi2GetFMUstate;
    fmi2SetFMUstateTYPE* fmi2SetFMUstate;
    fmi2FreeFMUstateTYPE* fmi2FreeFMUstate;
    fmi2SerializedFMUstateSizeTYPE* fmi2SerializedFMUstateSize;
    fmi2SerializeFMUstateTYPE* fmi2SerializeFMUstate;
    fmi2DeSerializeFMUstateTYPE* fmi2DeSerializeFMUstate;
    fmi2GetDirectionalDerivativeTYPE* fmi2GetDirectionalDerivative;
    fmi2EnterEventModeTYPE* fmi2EnterEventMode;
    fmi2NewDiscreteStatesTYPE* fmi2NewDiscreteStates;
    fmi2EnterContinuousTimeModeTYPE* fmi2EnterContinuousTimeMode;
    fmi2CompletedIntegratorStepTYPE* fmi2CompletedIntegratorStep;
    fmi2SetTimeTYPE* fmi2SetTime;
    fmi2SetContinuousStatesTYPE* fmi2SetContinuousStates;
    fmi2GetDerivativesTYPE* fmi2GetDerivatives;
    fmi2GetEventIndicatorsTYPE* fmi2GetEventIndicators;
    fmi2GetContinuousStatesTYPE* fmi2GetContinuousStates;
    fmi2GetNominalsOfContinuousStatesTYPE* fmi2GetNominalsOfContinuousStates;
    fmi2SetRealInputDerivativesTYPE* fmi2SetRealInputDerivatives;
    fmi2GetRealOutputDerivativesTYPE* fmi2GetRealOutputDerivatives;
    fmi2DoStepTYPE* fmi2DoStep;
    fmi2CancelStepTYPE* fmi2CancelStep;
    fmi2GetStatusTYPE* fmi2GetStatus;
    fmi2GetRealStatusTYPE* fmi2GetRealStatus;
    fmi2GetIntegerStatusTYPE* fmi2GetIntegerStatus;
    fmi2GetBooleanStatusTYPE* fmi2GetBooleanStatus;
    fmi2GetStringStatusTYPE* fmi2GetStringStatus;
    
    fmi2GetTypesPlatformTYPE* fmi2GetTypesPlatform;
    fmi2GetVersionTYPE* fmi2GetVersion;
    fmi2SetDebugLoggingTYPE* fmi2SetDebugLogging;
} g_funcs; // Static zero-initialization (no = {0})

HMODULE g_hCore = NULL;

static bool ResolveAll() {
    auto load = [&](auto& fp, const char* sym) -> bool {
        FARPROC p = GetProcAddress(g_hCore, sym);
        if (!p) {
            // Log missing symbol
            Log(std::string("Missing symbol: ") + sym);
            fp = nullptr;
            return false;
        }
        // Cast FARPROC to the correct function pointer type
        fp = reinterpret_cast<std::remove_reference_t<decltype(fp)>>(p);
        return true;
    };

    bool ok = true;

    ok &= load(g_funcs.fmi2Instantiate, "fmi2Instantiate");
    ok &= load(g_funcs.fmi2FreeInstance, "fmi2FreeInstance");
    ok &= load(g_funcs.fmi2SetupExperiment, "fmi2SetupExperiment");
    ok &= load(g_funcs.fmi2EnterInitializationMode, "fmi2EnterInitializationMode");
    ok &= load(g_funcs.fmi2ExitInitializationMode, "fmi2ExitInitializationMode");
    ok &= load(g_funcs.fmi2Terminate, "fmi2Terminate");
    ok &= load(g_funcs.fmi2Reset, "fmi2Reset");
    ok &= load(g_funcs.fmi2GetReal, "fmi2GetReal");
    ok &= load(g_funcs.fmi2GetInteger, "fmi2GetInteger");
    ok &= load(g_funcs.fmi2GetBoolean, "fmi2GetBoolean");
    ok &= load(g_funcs.fmi2GetString, "fmi2GetString");
    ok &= load(g_funcs.fmi2SetReal, "fmi2SetReal");
    ok &= load(g_funcs.fmi2SetInteger, "fmi2SetInteger");
    ok &= load(g_funcs.fmi2SetBoolean, "fmi2SetBoolean");
    ok &= load(g_funcs.fmi2SetString, "fmi2SetString");
    ok &= load(g_funcs.fmi2GetFMUstate, "fmi2GetFMUstate");
    ok &= load(g_funcs.fmi2SetFMUstate, "fmi2SetFMUstate");
    ok &= load(g_funcs.fmi2FreeFMUstate, "fmi2FreeFMUstate");
    ok &= load(g_funcs.fmi2SerializedFMUstateSize, "fmi2SerializedFMUstateSize");
    ok &= load(g_funcs.fmi2SerializeFMUstate, "fmi2SerializeFMUstate");
    ok &= load(g_funcs.fmi2DeSerializeFMUstate, "fmi2DeSerializeFMUstate");
    ok &= load(g_funcs.fmi2GetDirectionalDerivative, "fmi2GetDirectionalDerivative");
    ok &= load(g_funcs.fmi2EnterEventMode, "fmi2EnterEventMode");
    ok &= load(g_funcs.fmi2NewDiscreteStates, "fmi2NewDiscreteStates");
    ok &= load(g_funcs.fmi2EnterContinuousTimeMode, "fmi2EnterContinuousTimeMode");
    ok &= load(g_funcs.fmi2CompletedIntegratorStep, "fmi2CompletedIntegratorStep");
    ok &= load(g_funcs.fmi2SetTime, "fmi2SetTime");
    ok &= load(g_funcs.fmi2SetContinuousStates, "fmi2SetContinuousStates");
    ok &= load(g_funcs.fmi2GetDerivatives, "fmi2GetDerivatives");
    ok &= load(g_funcs.fmi2GetEventIndicators, "fmi2GetEventIndicators");
    ok &= load(g_funcs.fmi2GetContinuousStates, "fmi2GetContinuousStates");
    ok &= load(g_funcs.fmi2GetNominalsOfContinuousStates, "fmi2GetNominalsOfContinuousStates");
    ok &= load(g_funcs.fmi2SetRealInputDerivatives, "fmi2SetRealInputDerivatives");
    ok &= load(g_funcs.fmi2GetRealOutputDerivatives, "fmi2GetRealOutputDerivatives");
    ok &= load(g_funcs.fmi2DoStep, "fmi2DoStep");
    ok &= load(g_funcs.fmi2CancelStep, "fmi2CancelStep");
    ok &= load(g_funcs.fmi2GetStatus, "fmi2GetStatus");
    ok &= load(g_funcs.fmi2GetRealStatus, "fmi2GetRealStatus");
    ok &= load(g_funcs.fmi2GetIntegerStatus, "fmi2GetIntegerStatus");
    ok &= load(g_funcs.fmi2GetBooleanStatus, "fmi2GetBooleanStatus");
    ok &= load(g_funcs.fmi2GetStringStatus, "fmi2GetStringStatus");
    
    ok &= load(g_funcs.fmi2GetTypesPlatform, "fmi2GetTypesPlatform");
    ok &= load(g_funcs.fmi2GetVersion, "fmi2GetVersion");
    ok &= load(g_funcs.fmi2SetDebugLogging, "fmi2SetDebugLogging");

    return ok;
}

static bool EnsureCoreLoaded() {
    if (g_hCore) return true;
    
    Log("EnsureCoreLoaded called");

    // 1. Get Path to this Shim DLL
    HMODULE hShim = NULL;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&EnsureCoreLoaded, &hShim)) {
        LogError("GetModuleHandleExW failed", GetLastError());
        return false;
    }
    
    wchar_t pathBuf[MAX_PATH];
    if (GetModuleFileNameW(hShim, pathBuf, MAX_PATH) == 0) {
        LogError("GetModuleFileNameW failed", GetLastError());
        return false;
    }
    
    fs::path shimPath(pathBuf);
    fs::path binDir = shimPath.parent_path();
    fs::path corePath = binDir / CORE_DLL_NAME;
    
    Log("Shim Path: " + shimPath.string());
    Log("Bin Dir: " + binDir.string());
    Log("Core Path: " + corePath.string());

    // 2. Set DLL Directory to include the bin directory (so Python dependencies in the same dir are found)
    wchar_t oldDir[MAX_PATH] = {0};
    if (GetDllDirectoryW(MAX_PATH, oldDir) == 0) {
        // If fails, usually means not set.
        oldDir[0] = 0;
    }
    {
        std::wstring wOld(oldDir);
        Log("Original DllDirectory: " + std::string(wOld.begin(), wOld.end()));
    }

    if (!SetDllDirectoryW(binDir.c_str())) {
        LogError("SetDllDirectoryW failed", GetLastError());
    } else {
        Log("SetDllDirectoryW succeeded. New path: " + binDir.string());
    }

    // 3. Load Core DLL
    // The core DLL (and its dependencies like Python) should now be loadable
    Log("Attempting LoadLibraryW for Core DLL...");
    g_hCore = LoadLibraryW(corePath.c_str());

    // 4. Restore DLL Directory
    SetDllDirectoryW(oldDir[0] ? oldDir : NULL);
    Log("Restored original DllDirectory");

    if (!g_hCore) {
        LogError("LoadLibraryW for Core failed", GetLastError());
        // Fallback: try simple load
        Log("Attempting fallback LoadLibraryW (name only)...");
        g_hCore = LoadLibraryW(CORE_DLL_NAME);
    }

    if (!g_hCore) {
        LogError("Final failure to load core DLL", GetLastError());
        std::cerr << "[GT-Shim] Failed to load core DLL: " << corePath.string() << " Error: " << GetLastError() << std::endl;
        return false;
    }
    
    Log("Core DLL loaded successfully. Resolving symbols...");

    // 5. Resolve Symbols
    if (!ResolveAll()) {
        Log("Warning: Some symbols were missing in Core DLL");
        std::cerr << "[GT-Shim] Warning: Some symbols were missing in Core DLL" << std::endl;
        // Depending on strictness, we might return false here. 
        // But FMU might still work if unused functions are missing.
    } else {
        Log("All symbols resolved.");
    }

    return true;
}

extern "C" {

FMI2_Export const char* fmi2GetTypesPlatform() {
    // Log("fmi2GetTypesPlatform called");
    if (!EnsureCoreLoaded()) return "default";
    if (g_funcs.fmi2GetTypesPlatform) return g_funcs.fmi2GetTypesPlatform();
    return "default";
}

FMI2_Export const char* fmi2GetVersion() {
    Log("fmi2GetVersion called");
    if (!EnsureCoreLoaded()) return "2.0";
    if (g_funcs.fmi2GetVersion) return g_funcs.fmi2GetVersion();
    return "2.0";
}

FMI2_Export fmi2Component fmi2Instantiate(fmi2String instanceName, fmi2Type fmuType, fmi2String fmuGUID, fmi2String fmuResourceLocation, const fmi2CallbackFunctions* functions, fmi2Boolean visible, fmi2Boolean loggingOn) {
    Log("fmi2Instantiate called");
    if (!EnsureCoreLoaded()) return NULL;
    if (g_funcs.fmi2Instantiate) return g_funcs.fmi2Instantiate(instanceName, fmuType, fmuGUID, fmuResourceLocation, functions, visible, loggingOn);
    return NULL;
}

FMI2_Export void fmi2FreeInstance(fmi2Component c) {
    if (g_funcs.fmi2FreeInstance && c) g_funcs.fmi2FreeInstance(c);
}

FMI2_Export fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn, size_t nCategories, const fmi2String categories[]) {
    if (g_funcs.fmi2SetDebugLogging && c) return g_funcs.fmi2SetDebugLogging(c, loggingOn, nCategories, categories);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetupExperiment(fmi2Component c, fmi2Boolean toleranceDefined, fmi2Real tolerance, fmi2Real startTime, fmi2Boolean stopTimeDefined, fmi2Real stopTime) {
    if (g_funcs.fmi2SetupExperiment && c) return g_funcs.fmi2SetupExperiment(c, toleranceDefined, tolerance, startTime, stopTimeDefined, stopTime);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    if (g_funcs.fmi2EnterInitializationMode && c) return g_funcs.fmi2EnterInitializationMode(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    if (g_funcs.fmi2ExitInitializationMode && c) return g_funcs.fmi2ExitInitializationMode(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2Terminate(fmi2Component c) {
    if (g_funcs.fmi2Terminate && c) return g_funcs.fmi2Terminate(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2Reset(fmi2Component c) {
    if (g_funcs.fmi2Reset && c) return g_funcs.fmi2Reset(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    if (g_funcs.fmi2GetReal && c) return g_funcs.fmi2GetReal(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    if (g_funcs.fmi2GetInteger && c) return g_funcs.fmi2GetInteger(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    if (g_funcs.fmi2GetBoolean && c) return g_funcs.fmi2GetBoolean(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    if (g_funcs.fmi2GetString && c) return g_funcs.fmi2GetString(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    if (g_funcs.fmi2SetReal && c) return g_funcs.fmi2SetReal(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    if (g_funcs.fmi2SetInteger && c) return g_funcs.fmi2SetInteger(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    if (g_funcs.fmi2SetBoolean && c) return g_funcs.fmi2SetBoolean(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    if (g_funcs.fmi2SetString && c) return g_funcs.fmi2SetString(c, vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    if (g_funcs.fmi2GetFMUstate && c) return g_funcs.fmi2GetFMUstate(c, FMUstate);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate) {
    if (g_funcs.fmi2SetFMUstate && c) return g_funcs.fmi2SetFMUstate(c, FMUstate);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    if (g_funcs.fmi2FreeFMUstate && c) return g_funcs.fmi2FreeFMUstate(c, FMUstate);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size) {
    if (g_funcs.fmi2SerializedFMUstateSize && c) return g_funcs.fmi2SerializedFMUstateSize(c, FMUstate, size);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size) {
    if (g_funcs.fmi2SerializeFMUstate && c) return g_funcs.fmi2SerializeFMUstate(c, FMUstate, serializedState, size);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate) {
    if (g_funcs.fmi2DeSerializeFMUstate && c) return g_funcs.fmi2DeSerializeFMUstate(c, serializedState, size, FMUstate);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetDirectionalDerivative(fmi2Component c, const fmi2ValueReference vUnknown_ref[], size_t nUnknown, const fmi2ValueReference vKnown_ref[], size_t nKnown, const fmi2Real dvKnown[], fmi2Real dvUnknown[]) {
    if (g_funcs.fmi2GetDirectionalDerivative && c) return g_funcs.fmi2GetDirectionalDerivative(c, vUnknown_ref, nUnknown, vKnown_ref, nKnown, dvKnown, dvUnknown);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2EnterEventMode(fmi2Component c) {
    if (g_funcs.fmi2EnterEventMode && c) return g_funcs.fmi2EnterEventMode(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2NewDiscreteStates(fmi2Component c, fmi2EventInfo* fmi2eventInfo) {
    if (g_funcs.fmi2NewDiscreteStates && c) return g_funcs.fmi2NewDiscreteStates(c, fmi2eventInfo);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2EnterContinuousTimeMode(fmi2Component c) {
    if (g_funcs.fmi2EnterContinuousTimeMode && c) return g_funcs.fmi2EnterContinuousTimeMode(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2CompletedIntegratorStep(fmi2Component c, fmi2Boolean noSetFMUStatePriorToCurrentPoint, fmi2Boolean* enterEventMode, fmi2Boolean* terminateSimulation) {
    if (g_funcs.fmi2CompletedIntegratorStep && c) return g_funcs.fmi2CompletedIntegratorStep(c, noSetFMUStatePriorToCurrentPoint, enterEventMode, terminateSimulation);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetTime(fmi2Component c, fmi2Real time) {
    if (g_funcs.fmi2SetTime && c) return g_funcs.fmi2SetTime(c, time);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetContinuousStates(fmi2Component c, const fmi2Real x[], size_t nx) {
    if (g_funcs.fmi2SetContinuousStates && c) return g_funcs.fmi2SetContinuousStates(c, x, nx);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetDerivatives(fmi2Component c, fmi2Real derivatives[], size_t nx) {
    if (g_funcs.fmi2GetDerivatives && c) return g_funcs.fmi2GetDerivatives(c, derivatives, nx);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetEventIndicators(fmi2Component c, fmi2Real eventIndicators[], size_t ni) {
    if (g_funcs.fmi2GetEventIndicators && c) return g_funcs.fmi2GetEventIndicators(c, eventIndicators, ni);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetContinuousStates(fmi2Component c, fmi2Real x[], size_t nx) {
    if (g_funcs.fmi2GetContinuousStates && c) return g_funcs.fmi2GetContinuousStates(c, x, nx);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetNominalsOfContinuousStates(fmi2Component c, fmi2Real x_nominal[], size_t nx) {
    if (g_funcs.fmi2GetNominalsOfContinuousStates && c) return g_funcs.fmi2GetNominalsOfContinuousStates(c, x_nominal, nx);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetRealInputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[], const fmi2Real value[]) {
    if (g_funcs.fmi2SetRealInputDerivatives && c) return g_funcs.fmi2SetRealInputDerivatives(c, vr, nvr, order, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[], fmi2Real value[]) {
    if (g_funcs.fmi2GetRealOutputDerivatives && c) return g_funcs.fmi2GetRealOutputDerivatives(c, vr, nvr, order, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize, fmi2Boolean noSetFMUStatePriorToCurrentPoint) {
    if (g_funcs.fmi2DoStep && c) return g_funcs.fmi2DoStep(c, currentCommunicationPoint, communicationStepSize, noSetFMUStatePriorToCurrentPoint);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2CancelStep(fmi2Component c) {
    if (g_funcs.fmi2CancelStep && c) return g_funcs.fmi2CancelStep(c);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value) {
    if (g_funcs.fmi2GetStatus && c) return g_funcs.fmi2GetStatus(c, s, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value) {
    if (g_funcs.fmi2GetRealStatus && c) return g_funcs.fmi2GetRealStatus(c, s, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value) {
    if (g_funcs.fmi2GetIntegerStatus && c) return g_funcs.fmi2GetIntegerStatus(c, s, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value) {
    if (g_funcs.fmi2GetBooleanStatus && c) return g_funcs.fmi2GetBooleanStatus(c, s, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value) {
    if (g_funcs.fmi2GetStringStatus && c) return g_funcs.fmi2GetStringStatus(c, s, value);
    return fmi2Error;
}

} // extern C
