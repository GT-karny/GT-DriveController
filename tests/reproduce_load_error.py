import os
import sys
import ctypes
import zipfile
import tempfile
import shutil
from pathlib import Path

# Config
FMU_PATH = r"E:\Repository\GT-karny\GT-SimulatorIntegration\FMU\gt_drivecontroller\GT-DriveController.fmu"

def test_load():
    print(f"Testing load of FMU: {FMU_PATH}")
    
    if not os.path.exists(FMU_PATH):
        print("Error: FMU not found")
        return

    # Create temp dir
    extract_dir = Path(tempfile.mkdtemp())
    print(f"Extracting to: {extract_dir}")
    
    try:
        with zipfile.ZipFile(FMU_PATH, 'r') as zip_ref:
            zip_ref.extractall(extract_dir)
            
        dll_path = extract_dir / "binaries" / "win64" / "GT-DriveController.dll"
        if not dll_path.exists():
            print("Error: DLL not found in FMU")
            print(f"Contents of {extract_dir}:")
            for f in extract_dir.rglob("*"):
                print(f)
            return

        print(f"Attempting to load DLL: {dll_path}")
        
        # Test 1: Direct Load (Expected to fail if dependent DLLs not found)
        print("\n--- Test 1: Direct Load ---")
        try:
            lib = ctypes.CDLL(str(dll_path))
            print("SUCCESS: DLL loaded directly (Shim works)")
            
            # Verify explicit loading of Core
            print("\n--- Test 3: Instantiation (Verify Core Load) ---")
            try:
                # fmi2Instantiate(name, type, guid, resource, callback, visible, logging)
                lib.fmi2Instantiate.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
                lib.fmi2Instantiate.restype = ctypes.c_void_p
                
                print("Attempting instantiation...")
                comp = lib.fmi2Instantiate(
                    b"TestInstance",
                    1, # CoSimulation
                    b"{GUID}",
                    b"file:///E:/resources", # Dummy resource path
                    None,
                    0,
                    0
                )
                
                if comp:
                    print(f"SUCCESS: Instantiation returned Component {comp}")
                    
                    # Cleanup
                    try:
                        lib.fmi2FreeInstance.argtypes = [ctypes.c_void_p]
                        lib.fmi2FreeInstance.restype = None
                        lib.fmi2FreeInstance(comp)
                        print("Instance freed.")
                    except:
                        pass
                else:
                    print("FAILURE: Instantiation returned NULL (Core load failed?)")
            except Exception as e:
                print(f"FAILURE calling instantiate: {e}")

        except OSError as e:
            print(f"FAILURE: {e} (Shim load failed)")

        # Test 2: With DLL Directory Added (Redundant if Test 1 passed, but kept for comparison)
        print("\n--- Test 2: With Add Dll Directory (Legacy) ---")
        print("\n--- Test 2: With Add Dll Directory ---")
        try:
            # Add the bin directory to search path
            os.add_dll_directory(str(dll_path.parent))
            print(f"Added {dll_path.parent} to DLL search path")
            
            lib = ctypes.CDLL(str(dll_path))
            print("SUCCESS: DLL loaded with directory added")
        except Exception as e:
            print(f"FAILURE: {e}")
            # Try to verify what's missing
            print("Listing directory contents:")
            for f in dll_path.parent.iterdir():
                print(f.name)

    finally:
        # Cleanup
        try:
            # shutil.rmtree(extract_dir) 
            # Keep it for manual inspection if needed, or remove
            pass
        except:
            pass
        print("\nDone.")

if __name__ == "__main__":
    test_load()
