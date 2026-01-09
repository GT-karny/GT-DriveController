#include "OSMPController.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <sstream>

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
    // Properly release Python object by assigning None
    // (release() would leak by not decrementing refcount)
    if (m_pythonInitialized) {
        // Acquire GIL before touching Python objects
        py::gil_scoped_acquire acquire;
        m_pyController = py::none();
    }
}

fmi2Status OSMPController::doInit() {
    // Prevent double-initialization
    if (m_pythonInitialized) {
        std::cout << "[GT-DriveController] Already initialized, skipping" << std::endl;
        return fmi2OK;
    }

    std::cout << "[GT-DriveController] Enter doInit..." << std::endl;

    try {
        // Construct absolute path to resources
        fs::path resDir(m_resourcePath);
        fs::path pythonHome = resDir / "python";
        
        std::cout << "[GT-DriveController] Resource path: " << m_resourcePath << std::endl;
        std::cout << "[GT-DriveController] Python Home expected at: " << pythonHome.string() << std::endl;
        
        // Ensure Interpreter is running
        std::cout << "[GT-DriveController] Initializing Python Interpreter..." << std::endl;
        GlobalInitializePython(pythonHome.wstring());
        std::cout << "[GT-DriveController] Python Interpreter Initialized." << std::endl;

        std::cout << "[GT-DriveController] Importing sys module..." << std::endl;
        py::module sys = py::module::import("sys");
        std::cout << "[GT-DriveController] sys module imported." << std::endl;

        // 1. Handle PythonDependencyPath
        if (!m_pythonDependencyPath.empty()) {
            std::cout << "[GT-DriveController] Adding dependency path: " << m_pythonDependencyPath << std::endl;
            sys.attr("path").attr("append")(m_pythonDependencyPath);
        }

        // 2. Handle PythonScriptPath
        fs::path scriptPath(m_pythonScriptPath);
        if (scriptPath.is_relative()) {
            scriptPath = resDir / scriptPath;
        }

        // Normalize path separators to forward slashes for Python compatibility
        std::string scriptPathStr = scriptPath.string();
        std::replace(scriptPathStr.begin(), scriptPathStr.end(), '\\', '/');

        std::cout << "[GT-DriveController] Using script: " << scriptPathStr << std::endl;

        if (!fs::exists(scriptPath)) {
            std::cerr << "[GT-DriveController] Error: Script not found: " << scriptPathStr << std::endl;
            // Also print what we checked
            std::cerr << "[GT-DriveController] (Checked path: " << scriptPath.string() << ")" << std::endl;
            return fmi2Error;
        }

        // --- Path Setup Strategy for OSI Support ---
        // 1. Add 'resources' dir to sys.path -> allows 'import osi3'
        // 2. Add 'resources/osi3' dir to sys.path -> allows flat imports found in generated files (e.g. 'import osi_common_pb2')
        // 3. Add script's parent dir to sys.path -> allows importing the logic script itself

        std::string resDirStr = resDir.string();
        std::replace(resDirStr.begin(), resDirStr.end(), '\\', '/');

        fs::path osi3Dir = resDir / "osi3";
        std::string osi3DirStr = osi3Dir.string();
        std::replace(osi3DirStr.begin(), osi3DirStr.end(), '\\', '/');

        std::string scriptParent = scriptPath.parent_path().string();
        std::replace(scriptParent.begin(), scriptParent.end(), '\\', '/'); 
        
        std::cout << "[GT-DriveController] Updating sys.path:" << std::endl;
        std::cout << "  - Resources: " << resDirStr << std::endl;
        std::cout << "  - OSI3:      " << osi3DirStr << std::endl;
        std::cout << "  - Script:    " << scriptParent << std::endl;

        // Use PyRun_SimpleString with raw string literal to avoid any escape sequence issues
        std::string cmd = "import sys\n";
        cmd += "sys.path.insert(0, r'" + resDirStr + "')\n";
        cmd += "sys.path.insert(0, r'" + osi3DirStr + "')\n";
        cmd += "sys.path.insert(0, r'" + scriptParent + "')\n";
        
        if (PyRun_SimpleString(cmd.c_str()) != 0) {
            std::cerr << "[GT-DriveController] Error: Failed to update sys.path" << std::endl;
        }

        // Import module (filename without .py)
        std::string moduleName = scriptPath.stem().string();
        std::cout << "[GT-DriveController] Importing module: " << moduleName << std::endl;
        
        // Print sys.path for verification
        PyRun_SimpleString("print('[GT-DriveController] DEBUG sys.path:', sys.path)");

        // Import using module name, NOT path
        py::module logic = py::module::import(moduleName.c_str());
        std::cout << "[GT-DriveController] Module imported successfully." << std::endl;
        
        // Instantiate Controller
        std::cout << "[GT-DriveController] Instantiating Python Controller class..." << std::endl;
        m_pyController = logic.attr("Controller")();
        std::cout << "[GT-DriveController] Python Controller instantiated." << std::endl;
        
        m_pythonInitialized = true;
        std::cout << "[GT-DriveController] Python controller initialized successfully" << std::endl;

        return fmi2OK;
    }
    catch (py::error_already_set& e) {
        std::cerr << "[GT-DriveController] Python error in doInit: " << e.what() << std::endl;
        // Print sys.path for debugging
        try {
             py::module sys = py::module::import("sys");
             py::print("Current sys.path:", sys.attr("path"));
        } catch(...) {}
        return fmi2Error;
    }
    catch (std::exception& e) {
        std::cerr << "[GT-DriveController] Error in doInit: " << e.what() << std::endl;
        return fmi2Error;
    }
    catch (...) {
        std::cerr << "[GT-DriveController] Unknown error in doInit" << std::endl;
        return fmi2Error;
    }
}

fmi2Status OSMPController::doStep(fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize) {
    // Fallback: Initialize if not done yet
    if (!m_pythonInitialized) {
        std::cerr << "[GT-DriveController] Warning: doStep called before initialization, initializing now" << std::endl;
        if (doInit() != fmi2OK) {
            return fmi2Error;
        }
    }

    if (m_osi_size > 0 && m_osi_baseLo != 0) {
        try {
            // 1. Decode Pointer
            void* rawPtr = decodePointer(m_osi_baseHi, m_osi_baseLo);
            
            // 2. Validate Pointer (Risk #3: OSI buffer safety)
            if (!rawPtr) {
                std::cerr << "[GT-DriveController] Warning: Invalid OSI pointer (null), using default values" << std::endl;
                return fmi2Warning;
            }
            
            // Validate size is reasonable (< 100MB)
            const size_t MAX_OSI_SIZE = 100 * 1024 * 1024;
            if (m_osi_size > MAX_OSI_SIZE) {
                std::cerr << "[GT-DriveController] Warning: OSI size too large (" << m_osi_size 
                          << " bytes), using default values" << std::endl;
                return fmi2Warning;
            }
            
            // 3. Acquire GIL for Python calls (Risk #2: thread safety)
            // Note: For single-threaded host, this is defensive programming
            py::gil_scoped_acquire acquire;
            
            // 4. Read Bytes
            // Create a python bytes object from raw memory (copy)
            // Note: This can throw if the pointer is invalid
            py::bytes data;
            try {
                data = py::bytes(reinterpret_cast<const char*>(rawPtr), m_osi_size);
            }
            catch (...) {
                // Catch all exceptions including access violations
                std::cerr << "[GT-DriveController] Warning: Failed to read OSI data (invalid pointer or size), using default values" << std::endl;
                return fmi2Warning;
            }
            
            // 5. Call Python Update
            py::object result = m_pyController.attr("update_control")(data);
            
            // 6. Parse Result [throttle, brake, steering, drive_mode, osi_bytes]
            if (py::isinstance<py::list>(result)) {
                py::list resList = result.cast<py::list>();
                size_t size = resList.size();
                
                if (size >= 3) {
                    m_throttle = resList[0].cast<float>();
                    m_brake = resList[1].cast<float>();
                    m_steering = resList[2].cast<float>();
                }
                
                if (size >= 4) {
                    m_driveMode = resList[3].cast<int>();
                }

                if (size >= 5 && py::isinstance<py::bytes>(resList[4])) {
                    // Double buffering: write to the other buffer
                    int next_idx = 1 - m_osi_out_idx;
                    m_osi_out_buffer[next_idx] = resList[4].cast<std::string>();
                    m_osi_out_idx = next_idx;
                    
                    encodePointer(m_osi_out_buffer[m_osi_out_idx].data(), m_osi_out_baseHi, m_osi_out_baseLo);
                    m_osi_out_size = (fmi2Integer)m_osi_out_buffer[m_osi_out_idx].size();
                } else {
                    m_osi_out_baseHi = 0;
                    m_osi_out_baseLo = 0;
                    m_osi_out_size = 0;
                }
            }
        }
        catch (py::error_already_set& e) {
            // Enhanced Python error reporting (Risk #7)
            std::cerr << "[GT-DriveController] Python error in doStep: " << e.what() << std::endl;
            return fmi2Warning;
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

fmi2Status OSMPController::getInteger(const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case VR_OSI_OUT_BASELO: value[i] = m_osi_out_baseLo; break;
            case VR_OSI_OUT_BASEHI: value[i] = m_osi_out_baseHi; break;
            case VR_OSI_OUT_SIZE:   value[i] = m_osi_out_size; break;
            case VR_DRIVEMODE:      value[i] = m_driveMode; break;
            default:                value[i] = 0; break;
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

fmi2Status OSMPController::setString(const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case VR_PYTHON_SCRIPT_PATH: 
                std::cout << "[GT-DriveController] setString: PythonScriptPath overridden to: " << value[i] << std::endl;
                m_pythonScriptPath = value[i]; 
                break;
            case VR_PYTHON_DEP_PATH:    
                std::cout << "[GT-DriveController] setString: PythonDependencyPath overridden to: " << value[i] << std::endl;
                m_pythonDependencyPath = value[i]; 
                break;
            default: break;
        }
    }
    return fmi2OK;
}

fmi2Status OSMPController::getString(const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case VR_PYTHON_SCRIPT_PATH: value[i] = m_pythonScriptPath.c_str(); break;
            case VR_PYTHON_DEP_PATH:    value[i] = m_pythonDependencyPath.c_str(); break;
            default:                    value[i] = ""; break;
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

void OSMPController::encodePointer(const void* ptr, fmi2Integer& hi, fmi2Integer& lo) {
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    if constexpr (sizeof(void*) == 8) {
        hi = (fmi2Integer)((address >> 32) & 0xFFFFFFFF);
        lo = (fmi2Integer)(address & 0xFFFFFFFF);
    } else {
        hi = 0;
        lo = (fmi2Integer)address;
    }
}

std::string OSMPController::decodeResourcePath(const std::string& uri) {
    std::string path = uri;
    
    // Remove file:// prefix
    if (path.find("file:///") == 0) {
        path = path.substr(8);
    } else if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    
    // Decode percent-encoding (Risk #4: URI decoding)
    // Example: "My%20FMU" -> "My FMU"
    path = urlDecode(path);
    
    return path;
}

std::string OSMPController::urlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            // Convert %XX to character
            int value;
            std::istringstream is(encoded.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                // Invalid encoding, keep as-is
                decoded += encoded[i];
            }
        } else {
            decoded += encoded[i];
        }
    }
    
    return decoded;
}
