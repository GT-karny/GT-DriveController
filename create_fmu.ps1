# Create FMU Package
# This script packages the GT-DriveController.fmu

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = "e:\Repository\GT-karny\GT-DriveController"
$BUILD_DIR = "$PROJECT_ROOT\build\Release"
$FMU_TEMP = "$PROJECT_ROOT\build\build_fmu"
$OUTPUT_FMU = "$PROJECT_ROOT\build\build_fmu\GT-DriveController.fmu"

Write-Host "Creating GT-DriveController.fmu package..." -ForegroundColor Green

# 1. Cleanup Temp Dir
Write-Host "`n[1/6] Cleaning up temp directory..."
if (Test-Path $FMU_TEMP) {
    Remove-Item -Path $FMU_TEMP -Recurse -Force
}
New-Item -ItemType Directory -Path $FMU_TEMP | Out-Null

# 2. Create FMU Structure
Write-Host "[2/6] Creating FMU structure..."

# Copy modelDescription.xml and README.md
Copy-Item -Path "$PROJECT_ROOT\fmu\modelDescription.xml" -Destination $FMU_TEMP
if (Test-Path "$PROJECT_ROOT\fmu\README.md") {
    Copy-Item -Path "$PROJECT_ROOT\fmu\README.md" -Destination $FMU_TEMP
    Write-Host "  - Added README.md"
}

# Create binaries/win64
$BIN_DIR = "$FMU_TEMP\binaries\win64"
New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null

# 3. Copy Binaries
Write-Host "[3/6] Copying binary files..."

# FMU DLLs
Copy-Item -Path "$BUILD_DIR\GT_DriveController.dll" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\GT-DriveController_Core.dll" -Destination $BIN_DIR

# Python DLLs and Runtime
Copy-Item -Path "$BUILD_DIR\python312.dll" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python3.dll" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python312.zip" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python312._pth" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\vcruntime140.dll" -Destination $BIN_DIR -ErrorAction SilentlyContinue
Copy-Item -Path "$BUILD_DIR\vcruntime140_1.dll" -Destination $BIN_DIR -ErrorAction SilentlyContinue

Write-Host "  - GT-DriveController.dll"
Write-Host "  - GT-DriveController_Core.dll"
Write-Host "  - python312 files"

# 4. Copy Resources
Write-Host "[4/6] Copying resources..."

$RES_DIR = "$FMU_TEMP\resources"
New-Item -ItemType Directory -Path $RES_DIR -Force | Out-Null

# Copy logic.py
Copy-Item -Path "$PROJECT_ROOT\resources\logic.py" -Destination $RES_DIR -ErrorAction SilentlyContinue

# Copy OSI Python bindings (osi3 package)
$OSI_SRC = "$PROJECT_ROOT\python\osi"
$OSI_DST = "$RES_DIR\osi3"
if (Test-Path $OSI_SRC) {
    Write-Host "  - Packaging OSI3 bindings..."
    Copy-Item -Path $OSI_SRC -Destination $OSI_DST -Recurse -Force
    # Ensure __init__.py exists
    if (-not (Test-Path "$OSI_DST\__init__.py")) {
        New-Item -Path "$OSI_DST\__init__.py" -ItemType File -Force | Out-Null
    }
} else {
    Write-Warning "OSI Python bindings not found at $OSI_SRC"
}

# Copy Google Protobuf Runtime
$GOOGLE_SRC = "$PROJECT_ROOT\python\google"
$GOOGLE_DST = "$RES_DIR\google"
if (Test-Path $GOOGLE_SRC) {
    Write-Host "  - Packaging Google Protobuf runtime..."
    Copy-Item -Path $GOOGLE_SRC -Destination $GOOGLE_DST -Recurse -Force
} else {
    Write-Warning "Google Protobuf runtime not found at $GOOGLE_SRC"
}

$PROTO_INFO_SRC = "$PROJECT_ROOT\python\protobuf-*.dist-info"
if (Test-Path $PROTO_INFO_SRC) {
    Copy-Item -Path $PROTO_INFO_SRC -Destination $RES_DIR -Recurse -Force
    Write-Host "  - Packaging Protobuf dist-info"
}

# Extension Modules
$PYD_DIR = "$RES_DIR\python"
New-Item -ItemType Directory -Path $PYD_DIR -Force | Out-Null

if (Test-Path "$PROJECT_ROOT\resources\python\*.pyd") {
    Copy-Item -Path "$PROJECT_ROOT\resources\python\*.pyd" -Destination $PYD_DIR
    Write-Host "  - Added .pyd files"
}

# site-packages
$SITE_DIR = "$PYD_DIR\Lib\site-packages"
New-Item -ItemType Directory -Path $SITE_DIR -Force | Out-Null

# 5. Create ZIP Archive
Write-Host "[5/6] Creating .fmu file (ZIP archive)..."

$OUTPUT_ZIP = "$PROJECT_ROOT\build\build_fmu\GT-DriveController.zip"
if (Test-Path $OUTPUT_ZIP) { Remove-Item $OUTPUT_ZIP -Force }

Compress-Archive -Path "$FMU_TEMP\*" -DestinationPath $OUTPUT_ZIP -CompressionLevel Optimal
Move-Item -Path $OUTPUT_ZIP -Destination $OUTPUT_FMU -Force

# 6. Cleanup
Write-Host "[6/6] Final cleanup..."

if (Test-Path "$FMU_TEMP\binaries") {
    Remove-Item -Path "$FMU_TEMP\binaries" -Recurse -Force
}
if (Test-Path "$FMU_TEMP\resources") {
    Remove-Item -Path "$FMU_TEMP\resources" -Recurse -Force
}

Write-Host "`nSUCCESS: FMU package created!" -ForegroundColor Green
Write-Host "Output: $OUTPUT_FMU"
