#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>
#include "fmi2Functions.h"

// Define function pointers for FMI (if loading DLL dynamically)
// But to simplify, we can link against the library if we export symbols correctly.
// For verification on Windows, we can LoadLibrary.

#include <windows.h>

typedef fmi2Component (*fmi2Instantiate_t)(fmi2String, fmi2Type, fmi2String, fmi2String, const fmi2CallbackFunctions*, fmi2Boolean, fmi2Boolean);
typedef void (*fmi2FreeInstance_t)(fmi2Component);
typedef fmi2Status (*fmi2SetupExperiment_t)(fmi2Component, fmi2Boolean, fmi2Real, fmi2Real, fmi2Boolean, fmi2Real);
typedef fmi2Status (*fmi2EnterInitializationMode_t)(fmi2Component);
typedef fmi2Status (*fmi2ExitInitializationMode_t)(fmi2Component);
typedef fmi2Status (*fmi2DoStep_t)(fmi2Component, fmi2Real, fmi2Real, fmi2Boolean);
typedef fmi2Status (*fmi2SetInteger_t)(fmi2Component, const fmi2ValueReference[], size_t, const fmi2Integer[]);
typedef fmi2Status (*fmi2GetInteger_t)(fmi2Component, const fmi2ValueReference[], size_t, fmi2Integer[]);
typedef fmi2Status (*fmi2GetReal_t)(fmi2Component, const fmi2ValueReference[], size_t, fmi2Real[]);
typedef fmi2Status (*fmi2SetString_t)(fmi2Component, const fmi2ValueReference[], size_t, const fmi2String[]);

// Dummy Logger
void cb_logger(fmi2ComponentEnvironment c, fmi2String instanceName, fmi2Status status, fmi2String category, fmi2String message, ...) {
    printf("[FMU Log] %s: %s\n", category, message);
}

int main() {
    std::cout << "[Test] Loading GT-DriveController.dll..." << std::endl;
    HMODULE hLib = LoadLibraryA("GT-DriveController.dll");
    if (!hLib) {
        std::cerr << "[Test] Failed to load DLL. Error: " << GetLastError() << std::endl;
        return 1;
    }

    auto f_instantiate = (fmi2Instantiate_t)GetProcAddress(hLib, "fmi2Instantiate");
    auto f_free = (fmi2FreeInstance_t)GetProcAddress(hLib, "fmi2FreeInstance");
    auto f_setupExperiment = (fmi2SetupExperiment_t)GetProcAddress(hLib, "fmi2SetupExperiment");
    auto f_enterInitMode = (fmi2EnterInitializationMode_t)GetProcAddress(hLib, "fmi2EnterInitializationMode");
    auto f_exitInitMode = (fmi2ExitInitializationMode_t)GetProcAddress(hLib, "fmi2ExitInitializationMode");
    auto f_doStep = (fmi2DoStep_t)GetProcAddress(hLib, "fmi2DoStep");
    auto f_setInteger = (fmi2SetInteger_t)GetProcAddress(hLib, "fmi2SetInteger");
    auto f_getInteger = (fmi2GetInteger_t)GetProcAddress(hLib, "fmi2GetInteger");
    auto f_getReal = (fmi2GetReal_t)GetProcAddress(hLib, "fmi2GetReal");
    auto f_setString = (fmi2SetString_t)GetProcAddress(hLib, "fmi2SetString");

    if (!f_instantiate || !f_enterInitMode || !f_exitInitMode || !f_doStep || !f_setInteger || !f_getInteger || !f_getReal || !f_setString) {
        std::cerr << "[Test] Failed to load FMI functions." << std::endl;
        return 1;
    }

    fmi2CallbackFunctions callbacks = { cb_logger, NULL, NULL, NULL, NULL };
    
    // 1. Instantiate
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    std::string resPath = "file:///" + std::string(buffer) + "/resources";
    
    // Replace backslashes with forward slashes for URI compliance
    for (auto& c : resPath) if (c == '\\') c = '/';
    
    std::cout << "[Test] Instantiating with Resource Path: " << resPath << std::endl;
    fmi2Component c = f_instantiate("TestInstance", fmi2CoSimulation, "{guid}", resPath.c_str(), &callbacks, fmi2False, fmi2False);
    
    if (!c) {
        std::cerr << "[Test] Instantiation failed." << std::endl;
        return 1;
    }
    std::cout << "[Test] Instantiation successful." << std::endl;

    // 1a. Set Parameters (Risk mitigation: Parameterized paths)
    {
        std::cout << "[Test] Setting Python paths..." << std::endl;
        fmi2ValueReference vr_path = 11; // PythonScriptPath
        fmi2String val_path = "logic.py";
        f_setString(c, &vr_path, 1, &val_path);
    }

    // 1b. Setup Experiment
    f_setupExperiment(c, fmi2False, 0.0, 0.0, fmi2False, 0.0);

    // 1c. Enter Initialization Mode
    std::cout << "[Test] Entering initialization mode..." << std::endl;
    fmi2Status initStatus = f_enterInitMode(c);
    if (initStatus != fmi2OK) {
        std::cerr << "[Test] EnterInitializationMode failed." << std::endl;
        f_free(c);
        return 1;
    }

    // 1d. Exit Initialization Mode
    f_exitInitMode(c);
    std::cout << "[Test] Ready for simulation." << std::endl;

    // 2. Test Size = 0
    {
        std::cout << "[Test] Testing Size=0 case..." << std::endl;
        fmi2ValueReference vr_size = 2; // OSI_SensorView_In_Size
        fmi2Integer val_size = 0;
        f_setInteger(c, &vr_size, 1, &val_size);

        f_doStep(c, 0.0, 0.1, fmi2True);
        std::cout << "[Test] doStep(Size=0) passed." << std::endl;
    }

    // 3. Test Size > 0
    {
        std::cout << "[Test] Testing Size>0 case..." << std::endl;
        static char dummyData[] = "DUMMY_OSI_DATA";
        
        uintptr_t ptrVal = (uintptr_t)dummyData;
        fmi2Integer lo = (fmi2Integer)(ptrVal & 0xFFFFFFFF);
        fmi2Integer hi = (fmi2Integer)((ptrVal >> 32) & 0xFFFFFFFF);
        fmi2Integer size = (fmi2Integer)strlen(dummyData);

        fmi2ValueReference vrs[] = { 0, 1, 2 }; // BaseLo, BaseHi, Size
        fmi2Integer vals[] = { lo, hi, size };
        f_setInteger(c, vrs, 3, vals);

        f_doStep(c, 0.1, 0.1, fmi2True);
        std::cout << "[Test] doStep(Size>0) executed." << std::endl;

        // Check Outputs
        fmi2ValueReference vr_real[] = { 3, 4, 5 }; // T, B, S
        fmi2Real val_real[3];
        f_getReal(c, vr_real, 3, val_real);
        std::cout << "[Test] Outputs: T=" << val_real[0] << " B=" << val_real[1] << " S=" << val_real[2] << std::endl;

        fmi2ValueReference vr_int[] = { 7, 8, 9, 10 }; // OSI_Out_Lo, OSI_Out_Hi, OSI_Out_Size, DriveMode
        fmi2Integer val_int[4];
        f_getInteger(c, vr_int, 4, val_int);
        std::cout << "[Test] Outputs: OSI_Out_Ptr=" << std::hex << val_int[1] << val_int[0] 
                  << std::dec << " OSI_Out_Size=" << val_int[2] << " DriveMode=" << val_int[3] << std::endl;
        
        // Verify DriveMode is 1 (Forward)
        assert(val_int[3] == 1);
        // Verify OSI_Out_Size matches input (since passthrough)
        assert(val_int[2] == size);
    }

    f_free(c);
    std::cout << "[Test] Done." << std::endl;
    return 0;
}

