import ctypes
import os
import shutil
import sys
import tempfile
import struct

def is_x64(dll_path):
    with open(dll_path, 'rb') as f:
        d = f.read(64)
        if d[:2] != b'MZ': return "Unknown (Not MZ)"
        pe_offset = struct.unpack('<I', d[60:64])[0]
        f.seek(pe_offset)
        pe_sig = f.read(4)
        if pe_sig != b'PE\0\0': return "Unknown (Not PE)"
        machine = struct.unpack('<H', f.read(2))[0]
        if machine == 0x8664: return "x64"
        if machine == 0x014c: return "x86"
        return f"Unknown ({hex(machine)})"

def test_load(dll_name, directory, description):
    print(f"\n--- Testing: {description} ---")
    print(f"Directory: {directory}")
    dll_path = os.path.join(directory, dll_name)
    if not os.path.exists(dll_path):
        print("DLL not found!")
        return False
    
    # Check Architecture
    arch = is_x64(dll_path)
    print(f"Architecture: {arch}")

    try:
        # Load without adding extra directories
        lib = ctypes.CDLL(dll_path)
        print("SUCCESS: LoadLibrary passed.")
        
        # Check exports
        exports = ["fmi2Instantiate", "fmi2GetVersion", "fmi2GetTypesPlatform"]
        # CDLL doesn't expose list of exports easily, but we can try invalid access or simple `hasattr` (not reliable on loaded lib object directly for finding exports)
        # But we can try to access them
        for exp in exports:
            try:
                getattr(lib, exp)
                print(f"Found export: {exp}")
            except AttributeError:
                print(f"MISSING export: {exp}")

        # Close handle if possible (ctypes doesn't fully support FreeLibrary easily on Windows without handle)
        return True
    except OSError as e:
        print(f"FAILED: {e}")
        return False

def run_diagnostics():
    project_root = os.getcwd()
    build_dir = os.path.join(project_root, "build", "Release")
    shim_dll = "GT-DriveController.dll"
    
    with tempfile.TemporaryDirectory() as temp_dir:
        print(f"Created clean temp dir: {temp_dir}")
        
        # 1. Copy ONLY Shim DLL
        src = os.path.join(build_dir, shim_dll)
        if not os.path.exists(src):
            print("Build artifact not found.")
            return

        dst = os.path.join(temp_dir, shim_dll)
        shutil.copy2(src, dst)
        
        # Test 1: Shim Only
        success = test_load(shim_dll, temp_dir, "Shim DLL Isolated")
        
        if not success:
            print("\n-> Shim DLL failed to load in isolation. It likely depends on other DLLs.")
            
            # 2. Try adding VCRuntime
            runtimes = ["vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll"]
            for rt in runtimes:
                rt_src = os.path.join(build_dir, rt)
                if os.path.exists(rt_src):
                    print(f"Copying {rt}...")
                    shutil.copy2(rt_src, temp_dir)
            
            test_load(shim_dll, temp_dir, "Shim + VCRuntime")

if __name__ == "__main__":
    run_diagnostics()
