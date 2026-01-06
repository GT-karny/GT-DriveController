#include "OSMPController.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Global Python Interpreter Guard
static std::unique_ptr<py::scoped_interpreter> g_interpreter;

// initializePython - When using python312._pth file, do NOT call Py_SetPythonHome
// The _pth file will automatically configure sys.path if it's in the same directory as python312.dll
void OSMPController::GlobalInitializePython(const std::wstring& pythonHome) {
    if (!g_interpreter) {
        // DO NOT set Python Home when using _pth file
        // Py_SetPythonHome(const_cast<wchar_t*>(pythonHome.c_str()));
        
        // Initialize Interpreter
        g_interpreter = std::make_unique<py::scoped_interpreter>();
    }
}

void OSMPController::GlobalFinalizePython() {
    g_interpreter.reset();
}

OSMPController::OSMPController(fmi2String instanceName, fmi2String fmuResourceLocation)
    : m_instanceName(instanceName)
{
    // Convert URI to Path (file:///E:/... -> E:/...)
    m_resourcePath = decodeResourcePath(fmuResourceLocation ? fmuResourceLocation : "");
}

OSMPController::~OSMPController() {
    m_pyController.release(); // Release reference
}

fmi2Status OSMPController::doInit() {
    try {
        // Construct absolute path to resources/python
        fs::path resDir(m_resourcePath);
        fs::path pythonHome = resDir / "python";
        
        std::cout << "[GT-DriveController] Resource path: " << m_resourcePath << std::endl;
        std::cout << "[GT-DriveController] Python home: " << pythonHome.string() << std::endl;
        
        // Ensure Interpreter is running (Pass Home Path)
        GlobalInitializePython(pythonHome.wstring());

        // 2. Add 'resources/python' to sys.path to find logic.py
        // We assume 'logic.py' is placed in 'resources' directory by build/packaging
        // or 'resources/python' if mapped there.
        // Let's add 'resources' root.
        
        py::module sys = py::module::import("sys");
        // Add resource path to sys.path to find 'logic.py'
        sys.attr("path").attr("append")(resDir.string());
        
        std::cout << "[GT-DriveController] Importing logic module..." << std::endl;
        // 3. Import logic module
        py::module logic = py::module::import("logic");
        
        // 4. Instantiate Controller
        m_pyController = logic.attr("Controller")();
        
        std::cout << "[GT-DriveController] Python controller initialized successfully" << std::endl;

        return fmi2OK;
    }
    catch (std::exception& e) {
        std::cerr << "[GT-DriveController] Error in doInit: " << e.what() << std::endl;
        return fmi2Error;
    }
}

fmi2Status OSMPController::doStep(fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize) {
    if (m_osi_size > 0 && m_osi_baseLo != 0) {
        try {
            // 1. Decode Pointer
            void* rawPtr = decodePointer(m_osi_baseHi, m_osi_baseLo);
            
            // 2. Read Bytes
            // Create a python bytes object from raw memory (copy)
            // Note: If size is huge, this copy is expensive. But for SensorView it's necessary for Python.
            py::bytes data(reinterpret_cast<const char*>(rawPtr), m_osi_size);
            
            // 3. Call Python Update
            py::object result = m_pyController.attr("update_control")(data);
            
            // 4. Parse Result [throttle, brake, steering]
            if (py::isinstance<py::list>(result)) {
                py::list resList = result.cast<py::list>();
                if (resList.size() >= 3) {
                    m_throttle = resList[0].cast<float>();
                    m_brake = resList[1].cast<float>();
                    m_steering = resList[2].cast<float>();
                }
            }
        }
        catch (std::exception& e) {
            std::cerr << "[GT-DriveController] Error in doStep: " << e.what() << std::endl;
            return fmi2Warning;
        }
    }
    return fmi2OK;
}

fmi2Status OSMPController::setInteger(const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case VR_OSI_BASELO: m_osi_baseLo = value[i]; break;
            case VR_OSI_BASEHI: m_osi_baseHi = value[i]; break;
            case VR_OSI_SIZE:   m_osi_size = value[i]; break;
            default: break;
        }
    }
    return fmi2OK;
}

fmi2Status OSMPController::getReal(const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case VR_THROTTLE: value[i] = m_throttle; break;
            case VR_BRAKE:    value[i] = m_brake; break;
            case VR_STEERING: value[i] = m_steering; break;
            default:          value[i] = 0.0; break;
        }
    }
    return fmi2OK;
}

fmi2Status OSMPController::terminate() {
    return fmi2OK;
}

fmi2Status OSMPController::reset() {
    return fmi2OK;
}

// --- Helpers ---

void* OSMPController::decodePointer(fmi2Integer hi, fmi2Integer lo) {
    if constexpr (sizeof(void*) == 8) {
        unsigned long long address = ((unsigned long long)hi << 32) | (unsigned int)lo;
        return reinterpret_cast<void*>(address);
    } else {
        // cast to uintptr_t first to avoid warning
        return reinterpret_cast<void*>((uintptr_t)lo);
    }
}

std::string OSMPController::decodeResourcePath(const std::string& uri) {
    std::string path = uri;
    if (path.find("file:///") == 0) {
        path = path.substr(8);
    } else if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    return path;
}
