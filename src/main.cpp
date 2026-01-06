#include "OSMPController.h"

// FMI 2.0 Interface Implementation using OSMPController

extern "C" {

// ---------------------------------------------------------------------------
// FMI functions: class methods not depending on instantiation
// ---------------------------------------------------------------------------

FMI2_Export const char* fmi2GetTypesPlatform() {
    return fmi2TypesPlatform;
}

FMI2_Export const char* fmi2GetVersion() {
    return fmi2Version;
}

FMI2_Export fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn, size_t nCategories, const fmi2String categories[]) {
    return fmi2OK; // Not implemented
}

// ---------------------------------------------------------------------------
// FMI functions: instantiation, freeing, etc.
// ---------------------------------------------------------------------------

FMI2_Export fmi2Component fmi2Instantiate(fmi2String instanceName,
                                          fmi2Type fmuType,
                                          fmi2String fmuGUID,
                                          fmi2String fmuResourceLocation,
                                          const fmi2CallbackFunctions* functions,
                                          fmi2Boolean visible,
                                          fmi2Boolean loggingOn) {
    // Only Co-Simulation supported
    if (fmuType != fmi2CoSimulation) {
        return NULL;
    }

    // Create Controller Instance
    // Note: Per FMI 2.0 spec, fmi2Instantiate should be lightweight (object creation only).
    // Heavy initialization (Python, file I/O) is deferred to EnterInitializationMode.
    OSMPController* controller = new OSMPController(instanceName, fmuResourceLocation);

    return (fmi2Component)controller;
}

FMI2_Export void fmi2FreeInstance(fmi2Component c) {
    if (c) {
        OSMPController* controller = (OSMPController*)c;
        delete controller;
    }
}

FMI2_Export fmi2Status fmi2SetupExperiment(fmi2Component c,
                                           fmi2Boolean toleranceDefined,
                                           fmi2Real tolerance,
                                           fmi2Real startTime,
                                           fmi2Boolean stopTimeDefined,
                                           fmi2Real stopTime) {
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    // Per FMI 2.0 spec, this is the proper place for heavy initialization
    // (Python interpreter, module loading, resource allocation)
    if (c) {
        return ((OSMPController*)c)->doInit();
    }
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2Terminate(fmi2Component c) {
    if (c) return ((OSMPController*)c)->terminate();
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2Reset(fmi2Component c) {
    if (c) return ((OSMPController*)c)->reset();
    return fmi2Error;
}

// ---------------------------------------------------------------------------
// FMI functions: stepping
// ---------------------------------------------------------------------------

FMI2_Export fmi2Status fmi2DoStep(fmi2Component c,
                                  fmi2Real currentCommunicationPoint,
                                  fmi2Real communicationStepSize,
                                  fmi2Boolean noSetFMUStatePriorToCurrentPoint) {
    if (c) {
        return ((OSMPController*)c)->doStep(currentCommunicationPoint, communicationStepSize);
    }
    return fmi2Error;
}

// ---------------------------------------------------------------------------
// FMI functions: variable access
// ---------------------------------------------------------------------------

FMI2_Export fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    if (c) return ((OSMPController*)c)->getReal(vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    // Only inputs provided, we usually don't need to get them back, but for debugging:
    return fmi2OK; 
}

FMI2_Export fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    // No real inputs defined currently (only outputs), but if needed:
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    if (c) return ((OSMPController*)c)->setInteger(vr, nvr, value);
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    return fmi2OK;
}

// ... Other unsupported functions stubbed ...
FMI2_Export fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) { return fmi2Error; }
FMI2_Export fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate) { return fmi2Error; }
FMI2_Export fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) { return fmi2Error; }
FMI2_Export fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size) { return fmi2Error; }
FMI2_Export fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size) { return fmi2Error; }
FMI2_Export fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate) { return fmi2Error; }
FMI2_Export fmi2Status fmi2GetDirectionalDerivative(fmi2Component c, const fmi2ValueReference vUnknown_ref[], size_t nUnknown, const fmi2ValueReference vKnown_ref[], size_t nKnown, const fmi2Real dvKnown[], fmi2Real dvUnknown[]) { return fmi2Error; }

} // extern "C"
