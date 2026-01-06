# Create FMU Package
# このスクリプトはGT-DriveController.fmuファイルを作成します

$ErrorActionPreference = "Stop"

$PROJECT_ROOT = "e:\Repository\GT-karny\GT-DriveController"
$BUILD_DIR = "$PROJECT_ROOT\build\Release"
$FMU_TEMP = "$PROJECT_ROOT\build\build_fmu"
$OUTPUT_FMU = "$PROJECT_ROOT\build\build_fmu\GT-DriveController.fmu"

Write-Host "GT-DriveController.fmu パッケージを作成中..." -ForegroundColor Green

# 1. 一時ディレクトリをクリーンアップ
Write-Host "`n[1/6] 一時ディレクトリを準備中..."
if (Test-Path $FMU_TEMP) {
    Remove-Item -Path $FMU_TEMP -Recurse -Force
}
New-Item -ItemType Directory -Path $FMU_TEMP | Out-Null

# 2. FMU構造を作成
Write-Host "[2/6] FMU構造を作成中..."

# modelDescription.xmlとREADME.mdをfmuフォルダからコピー
Copy-Item -Path "$PROJECT_ROOT\fmu\modelDescription.xml" -Destination $FMU_TEMP
if (Test-Path "$PROJECT_ROOT\fmu\README.md") {
    Copy-Item -Path "$PROJECT_ROOT\fmu\README.md" -Destination $FMU_TEMP
    Write-Host "  - README.md を追加"
}

# binaries/win64ディレクトリを作成
$BIN_DIR = "$FMU_TEMP\binaries\win64"
New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null

# 3. バイナリファイルをコピー
Write-Host "[3/6] バイナリファイルをコピー中..."

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
Write-Host "[4/6] リソースファイルをコピー中..."

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
Write-Host "[5/6] .fmuファイル（ZIPアーカイブ）を作成中..."

if (Test-Path $OUTPUT_FMU) {
    Remove-Item $OUTPUT_FMU -Force
}

# PowerShellのCompress-Archiveを使用
Compress-Archive -Path "$FMU_TEMP\*" -DestinationPath $OUTPUT_FMU -CompressionLevel Optimal

# 6. build_fmuディレクトリをクリーンアップ（配布用）
Write-Host "[6/6] 配布用にディレクトリをクリーンアップ..."

# binariesとresourcesフォルダを削除（FMU内に含まれているため不要）
if (Test-Path "$FMU_TEMP\binaries") {
    Remove-Item -Path "$FMU_TEMP\binaries" -Recurse -Force
    Write-Host "  - binaries/ を削除（FMU内に含まれています）"
}
if (Test-Path "$FMU_TEMP\resources") {
    Remove-Item -Path "$FMU_TEMP\resources" -Recurse -Force
    Write-Host "  - resources/ を削除（FMU内に含まれています）"
}

Write-Host "  配布用ディレクトリ: $FMU_TEMP"
Write-Host "  (GT-DriveController.fmu と README.md のみ)"

# 完了メッセージ
Write-Host "`n✅ FMUパッケージが作成されました！" -ForegroundColor Green
Write-Host "`n出力ファイル:"
$fmuFile = Get-Item $OUTPUT_FMU
$sizeMB = [math]::Round($fmuFile.Length / 1MB, 2)
Write-Host "  📦 $($fmuFile.FullName)"
Write-Host "  📊 サイズ: $sizeMB MB"

Write-Host "`nFMU内容:"
Write-Host "  ├── modelDescription.xml"
Write-Host "  ├── README.md"
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
