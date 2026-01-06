# Create FMU Package
# このスクリプトはGT-DriveController.fmuファイルを作成します

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = "e:\Repository\GT-karny\GT-DriveController"
$BUILD_DIR = "$PROJECT_ROOT\build\Release"
$FMU_TEMP = "$PROJECT_ROOT\build\fmu_package"
$OUTPUT_FMU = "$PROJECT_ROOT\build\GT-DriveController.fmu"

Write-Host "GT-DriveController.fmu パッケージを作成中..." -ForegroundColor Green

# 1. 一時ディレクトリをクリーンアップ
Write-Host "`n[1/5] 一時ディレクトリを準備中..."
if (Test-Path $FMU_TEMP) {
    Remove-Item -Path $FMU_TEMP -Recurse -Force
}
New-Item -ItemType Directory -Path $FMU_TEMP | Out-Null

# 2. FMU構造を作成
Write-Host "[2/5] FMU構造を作成中..."

# modelDescription.xmlをコピー
Copy-Item -Path "$PROJECT_ROOT\fmu\modelDescription.xml" -Destination $FMU_TEMP

# binaries/win64ディレクトリを作成
$BIN_DIR = "$FMU_TEMP\binaries\win64"
New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null

# 3. バイナリファイルをコピー
Write-Host "[3/5] バイナリファイルをコピー中..."

# FMU DLL
Copy-Item -Path "$BUILD_DIR\GT-DriveController.dll" -Destination $BIN_DIR

# Python DLLs
Copy-Item -Path "$BUILD_DIR\python312.dll" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python3.dll" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python312.zip" -Destination $BIN_DIR
Copy-Item -Path "$BUILD_DIR\python312._pth" -Destination $BIN_DIR

# VC++ Runtime
Copy-Item -Path "$BUILD_DIR\vcruntime140.dll" -Destination $BIN_DIR -ErrorAction SilentlyContinue
Copy-Item -Path "$BUILD_DIR\vcruntime140_1.dll" -Destination $BIN_DIR -ErrorAction SilentlyContinue

Write-Host "  - GT-DriveController.dll"
Write-Host "  - python312.dll (7.14 MB)"
Write-Host "  - python312.zip (11.6 MB)"
Write-Host "  - python312._pth"
Write-Host "  - vcruntime140*.dll"

# 4. リソースファイルをコピー
Write-Host "[4/5] リソースファイルをコピー中..."

$RES_DIR = "$FMU_TEMP\resources"
New-Item -ItemType Directory -Path $RES_DIR -Force | Out-Null

# logic.pyをコピー
Copy-Item -Path "$PROJECT_ROOT\resources\logic.py" -Destination $RES_DIR -ErrorAction SilentlyContinue

# 拡張モジュール用ディレクトリ（オプション）
$PYD_DIR = "$RES_DIR\python"
New-Item -ItemType Directory -Path $PYD_DIR -Force | Out-Null

# .pydファイルをコピー（必要に応じて）
if (Test-Path "$PROJECT_ROOT\resources\python\*.pyd") {
    Copy-Item -Path "$PROJECT_ROOT\resources\python\*.pyd" -Destination $PYD_DIR
    $pydCount = (Get-ChildItem -Path $PYD_DIR -Filter "*.pyd").Count
    Write-Host "  - $pydCount 個の.pydファイル"
}

# site-packagesディレクトリ
$SITE_DIR = "$PYD_DIR\Lib\site-packages"
New-Item -ItemType Directory -Path $SITE_DIR -Force | Out-Null

# 5. ZIPアーカイブを作成
Write-Host "[5/5] .fmuファイル（ZIPアーカイブ）を作成中..."

if (Test-Path $OUTPUT_FMU) {
    Remove-Item $OUTPUT_FMU -Force
}

# PowerShellのCompress-Archiveを使用
Compress-Archive -Path "$FMU_TEMP\*" -DestinationPath $OUTPUT_FMU -CompressionLevel Optimal

# 一時ディレクトリを削除
Remove-Item -Path $FMU_TEMP -Recurse -Force

# 完了メッセージ
Write-Host "`n✅ FMUパッケージが作成されました！" -ForegroundColor Green
Write-Host "`n出力ファイル:"
$fmuFile = Get-Item $OUTPUT_FMU
$sizeMB = [math]::Round($fmuFile.Length / 1MB, 2)
Write-Host "  📦 $($fmuFile.FullName)"
Write-Host "  📊 サイズ: $sizeMB MB"

Write-Host "`nFMU内容:"
Write-Host "  ├── modelDescription.xml"
Write-Host "  ├── binaries/"
Write-Host "  │   └── win64/"
Write-Host "  │       ├── GT-DriveController.dll"
Write-Host "  │       ├── python312.dll"
Write-Host "  │       ├── python312.zip"
Write-Host "  │       ├── python312._pth"
Write-Host "  │       └── vcruntime140*.dll"
Write-Host "  └── resources/"
Write-Host "      ├── logic.py"
Write-Host "      └── python/"
Write-Host "          └── *.pyd"

Write-Host "`n次のステップ:"
Write-Host "  1. FMUをシミュレーション環境にインポート"
Write-Host "  2. logic.pyをカスタマイズして独自のコントローラーを実装"
