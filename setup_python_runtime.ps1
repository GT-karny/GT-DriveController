# Setup Python 3.12 Embedded Runtime
# このスクリプトはビルドしたPython 3.12から必要なファイルを
# resources/pythonディレクトリにコピーします

$ErrorActionPreference = "Stop"

$PYTHON_BUILD = "e:\Repository\GT-karny\GT-DriveController\thirdparty\cpython\PCbuild\amd64"
$PYTHON_SRC = "e:\Repository\GT-karny\GT-DriveController\thirdparty\cpython"
$DEST_DIR = "e:\Repository\GT-karny\GT-DriveController\resources\python"

Write-Host "Python 3.12 埋め込み環境のセットアップを開始します..." -ForegroundColor Green

# 1. ディレクトリの作成
Write-Host "`n[1/6] ディレクトリを作成中..."
New-Item -ItemType Directory -Force -Path $DEST_DIR | Out-Null

# 2. DLLファイルのコピー
Write-Host "[2/6] DLLファイルをコピー中..."
Copy-Item -Path "$PYTHON_BUILD\python312.dll" -Destination $DEST_DIR -Force
Copy-Item -Path "$PYTHON_BUILD\python3.dll" -Destination $DEST_DIR -Force
Copy-Item -Path "$PYTHON_BUILD\vcruntime140.dll" -Destination $DEST_DIR -Force -ErrorAction SilentlyContinue
Copy-Item -Path "$PYTHON_BUILD\vcruntime140_1.dll" -Destination $DEST_DIR -Force -ErrorAction SilentlyContinue

# 3. 標準ライブラリをZIPにアーカイブ
Write-Host "[3/6] 標準ライブラリをpython312.zipにアーカイブ中..."
$zipPath = Join-Path $DEST_DIR "python312.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

# Libディレクトリの内容をZIPに圧縮（site-packagesは除外）
$libSource = Join-Path $PYTHON_SRC "Lib"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zip = [System.IO.Compression.ZipFile]::Open($zipPath, 'Create')
try {
    Get-ChildItem -Path $libSource -Recurse -File | Where-Object {
        $_.FullName -notmatch "\\site-packages\\" -and
        $_.FullName -notmatch "\\__pycache__\\" -and
        $_.FullName -notmatch "\\.pyc$"
    } | ForEach-Object {
        $relativePath = $_.FullName.Substring($libSource.Length + 1)
        $entry = $zip.CreateEntry($relativePath.Replace('\', '/'))
        $entryStream = $entry.Open()
        $fileStream = [System.IO.File]::OpenRead($_.FullName)
        $fileStream.CopyTo($entryStream)
        $fileStream.Close()
        $entryStream.Close()
    }
} finally {
    $zip.Dispose()
}

Write-Host "  python312.zip を作成しました ($('{0:N2}' -f ((Get-Item $zipPath).Length / 1MB)) MB)"

# 4. 拡張モジュール(.pyd)のコピー
Write-Host "[4/6] 拡張モジュール(.pyd)をコピー中..."
$pydCount = 0
Get-ChildItem -Path $PYTHON_BUILD -Filter "*.pyd" | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $DEST_DIR -Force
    $pydCount++
}
Write-Host "  $pydCount 個の.pydファイルをコピーしました"

# 5. python312._pth ファイルの作成（重要！）
Write-Host "[5/6] python312._pth を作成中..."
$pthContent = @"
python312.zip
.

# 以下の行のコメントを外すとsite-packagesが有効になります
# import site
"@
$pthContent | Out-File -FilePath (Join-Path $DEST_DIR "python312._pth") -Encoding ascii -Force

# 6. site-packagesディレクトリの作成（将来の拡張用）
Write-Host "[6/6] site-packagesディレクトリを作成中..."
$sitePackages = Join-Path $DEST_DIR "Lib\site-packages"
New-Item -ItemType Directory -Force -Path $sitePackages | Out-Null

# 完了メッセージ
Write-Host "`n✅ セットアップ完了！" -ForegroundColor Green
Write-Host "`n作成されたファイル:"
Get-ChildItem -Path $DEST_DIR -File | ForEach-Object {
    $size = if ($_.Length -gt 1MB) { "{0:N2} MB" -f ($_.Length / 1MB) } else { "{0:N0} KB" -f ($_.Length / 1KB) }
    Write-Host "  - $($_.Name) ($size)"
}

Write-Host "`n次のステップ:"
Write-Host "  1. GT-DriveControllerをリビルド"
Write-Host "  2. test_fmu.exeを実行してPython初期化を確認"
