#ifndef OSMP_CONTROLLER_H
#define OSMP_CONTROLLER_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>

// FMI 2.0 Headers
#include "fmi2FunctionTypes.h"
#include "fmi2Functions.h"

// Pybind11 Headers
#include <pybind11/embed.h>
namespace py = pybind11;

// Define Value References (must match modelDescription.xml)
#define VR_OSI_BASELO 0
#define VR_OSI_BASEHI 1
#define VR_OSI_SIZE   2
#define VR_THROTTLE   3
#define VR_BRAKE      4
#define VR_STEERING   5
#define VR_VALID      6

class OSMPController {
public:
    OSMPController(fmi2String instanceName, fmi2String fmuResourceLocation);
    ~OSMPController();

    // FMI 2.0 Implementation Methods
    fmi2Status doInit();
    fmi2Status doStep(fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize);
    
    // Setters / Getters
    fmi2Status setInteger(const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]);
    fmi2Status getReal(const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]);
    
    // Other FMI stubs
    fmi2Status terminate();
    fmi2Status reset();

    static void GlobalInitializePython();
    static void GlobalFinalizePython();

private:
    std::string m_instanceName;
    std::string m_resourcePath;
    
    // Internal State for FMI Variables
    fmi2Integer m_osi_baseLo = 0;
    fmi2Integer m_osi_baseHi = 0;
    fmi2Integer m_osi_size = 0;
    
    fmi2Real m_throttle = 0.0;
    fmi2Real m_brake = 0.0;
    fmi2Real m_steering = 0.0;
    fmi2Boolean m_valid = fmi2True;

    // Python Objects
    py::object m_pyController;
    bool m_pythonInitialized = false;

    // Helper functions
    void* decodePointer(fmi2Integer hi, fmi2Integer lo);
    std::string decodeResourcePath(const std::string& uri);
};

#endif // OSMP_CONTROLLER_H
