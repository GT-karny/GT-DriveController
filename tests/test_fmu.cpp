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
typedef fmi2Status (*fmi2GetReal_t)(fmi2Component, const fmi2ValueReference[], size_t, fmi2Real[]);

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
    auto f_getReal = (fmi2GetReal_t)GetProcAddress(hLib, "fmi2GetReal");

    if (!f_instantiate || !f_enterInitMode || !f_exitInitMode || !f_doStep || !f_setInteger || !f_getReal) {
        std::cerr << "[Test] Failed to load FMI functions." << std::endl;
        return 1;
    }

    fmi2CallbackFunctions callbacks = { cb_logger, NULL, NULL, NULL, NULL };
    
    // 1. Instantiate
    // Note: We need to pass a valid resource URI for Python to load.
    // Assuming we run from build dir and resources is parallel or we pass absolute path.
    // For this test, we might fail python init if path is wrong, but let's try to pass the current dir as resource.
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    std::string resPath = "file:///" + std::string(buffer) + "/../resources"; // Assumption
    
    std::cout << "[Test] Instantiating with Resource Path: " << resPath << std::endl;
    fmi2Component c = f_instantiate("TestInstance", fmi2CoSimulation, "{guid}", resPath.c_str(), &callbacks, fmi2False, fmi2False);
    
    if (!c) {
        std::cerr << "[Test] Instantiation failed." << std::endl;
        return 1;
    }
    std::cout << "[Test] Instantiation successful." << std::endl;

    // 1b. Setup Experiment (optional but good practice)
    std::cout << "[Test] Setting up experiment..." << std::endl;
    f_setupExperiment(c, fmi2False, 0.0, 0.0, fmi2False, 0.0);

    // 1c. Enter Initialization Mode (THIS IS WHERE PYTHON LOADS NOW)
    std::cout << "[Test] Entering initialization mode..." << std::endl;
    fmi2Status initStatus = f_enterInitMode(c);
    if (initStatus != fmi2OK) {
        std::cerr << "[Test] EnterInitializationMode failed. (Likely Python init issue or path issue)" << std::endl;
        f_free(c);
        return 1;
    }
    std::cout << "[Test] Initialization mode entered successfully." << std::endl;

    // 1d. Exit Initialization Mode
    std::cout << "[Test] Exiting initialization mode..." << std::endl;
    f_exitInitMode(c);
    std::cout << "[Test] Ready for simulation." << std::endl;

    // 2. Test Size = 0 (Should not crash, should not run python update)
    {
        std::cout << "[Test] Testing Size=0 case..." << std::endl;
        fmi2ValueReference vr_size[] = { 2 }; // OSI_SensorView_In_Size
        fmi2Integer val_size[] = { 0 };
        f_setInteger(c, vr_size, 1, val_size);

        fmi2Status status = f_doStep(c, 0.0, 0.1, fmi2True);
        if (status == fmi2OK) {
            std::cout << "[Test] doStep(Size=0) passed." << std::endl;
        } else {
            std::cerr << "[Test] doStep(Size=0) failed with status " << status << std::endl;
        }
    }

    // 3. Test Size > 0 (With dummy data)
    {
        std::cout << "[Test] Testing Size>0 case..." << std::endl;
        // Use static buffer to ensure pointer remains valid
        static char dummyData[] = "DUMMY_OSI_DATA"; // Not valid proto, so Python parse exception expected but handled.
        
        uintptr_t ptrVal = (uintptr_t)dummyData;
        fmi2Integer lo = (fmi2Integer)(ptrVal & 0xFFFFFFFF);
        fmi2Integer hi = (fmi2Integer)((ptrVal >> 32) & 0xFFFFFFFF);
        fmi2Integer size = (fmi2Integer)strlen(dummyData);

        fmi2ValueReference vrs[] = { 0, 1, 2 }; // BaseLo, BaseHi, Size
        fmi2Integer vals[] = { lo, hi, size };
        f_setInteger(c, vrs, 3, vals);

        // Run Step
        fmi2Status status = f_doStep(c, 0.1, 0.1, fmi2True);
         if (status == fmi2OK || status == fmi2Warning) { // Warning expected due to parse error
            std::cout << "[Test] doStep(Size>0) executed. Status: " << status << std::endl;
        } else {
            std::cerr << "[Test] doStep(Size>0) failed with status " << status << std::endl;
        }

        // Check Outputs (should be default 0.0, 0.0, 0.0 due to parse error, or valid if logic handles it)
        fmi2ValueReference vr_out[] = { 3, 4, 5 }; // Throttle, Brake, Steering
        fmi2Real out_vals[3];
        f_getReal(c, vr_out, 3, out_vals);
        std::cout << "[Test] Outputs: T=" << out_vals[0] << " B=" << out_vals[1] << " S=" << out_vals[2] << std::endl;
    }

    // Free
    f_free(c);
    std::cout << "[Test] Done." << std::endl;

    return 0;
}
