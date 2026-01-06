# GT-DriveController FMU - ビルドガイド

## 概要

GT-DriveControllerは、Python 3.12を埋め込んだFMI 2.0 Co-Simulation FMUです。OSI (Open Simulation Interface) SensorViewを入力として受け取り、Pythonで実装されたコントローラーロジックを実行し、車両制御出力（スロットル、ブレーキ、ステアリング）を返します。

## 前提条件

### 必須ツール
- **Visual Studio 2017以降** (Python workloadとPython native development component)
- **CMake 3.15以降**
- **Git** (サブモジュール管理用)
- **PowerShell** (セットアップスクリプト実行用)

### 依存関係
- Python 3.12ソースコード（`thirdparty/cpython`に配置済み）
- pybind11（`thirdparty/pybind11`に配置済み）

## ビルド手順

### 1. Python 3.12のビルド

まず、プロジェクトで使用するPython 3.12をソースからビルドします。

```powershell
cd thirdparty\cpython\PCbuild
.\build.bat
```

**ビルド時間**: 約1〜2分  
**出力先**: `thirdparty/cpython/PCbuild/amd64/`

ビルド成果物:
- `python312.dll` (7.14 MB) - Python本体
- `python3.dll` (55 KB) - Python 3 stub
- `*.pyd` - 拡張モジュール（33個）
- `vcruntime140*.dll` - Visual C++ランタイム

### 2. Python埋め込みランタイムのセットアップ

プロジェクトルートで以下のスクリプトを実行します。

```powershell
cd e:\Repository\GT-karny\GT-DriveController
.\setup_python_runtime.ps1
```

このスクリプトは以下を実行します:
1. Python標準ライブラリを`python312.zip` (11.6 MB)にアーカイブ
2. `python312._pth`ファイルを作成（sys.path設定）
3. 拡張モジュール(`.pyd`)をコピー
4. ランタイムDLLをコピー
5. `resources/python/`ディレクトリに配置

### 3. CMake設定とビルド

```powershell
# CMake設定
cmake -B build -S .

# ビルド（Release構成）
cmake --build build --config Release
```

**出力先**: `build/Release/GT-DriveController.dll`

### 4. ランタイムファイルの配置

ビルド後、以下のファイルを`build/Release/`にコピーする必要があります:

```powershell
# Python DLLとZIPファイル
Copy-Item resources\python\python312.dll build\Release\
Copy-Item resources\python\python3.dll build\Release\
Copy-Item resources\python\python312.zip build\Release\
Copy-Item resources\python\python312._pth build\Release\
Copy-Item resources\python\vcruntime*.dll build\Release\

# 拡張モジュール（オプション、必要に応じて）
Copy-Item resources\python\*.pyd build\Release\resources\python\
```

### 5. テスト実行

```powershell
cd build\Release
.\test_fmu.exe
```

**期待される出力**:
```
[Test] Loading GT-DriveController.dll...
[GT-DriveController] Resource path: E:/Repository/GT-karny/GT-DriveController/build/Release/../resources
[GT-DriveController] Python home: E:/Repository/GT-karny/GT-DriveController/build/Release/../resources/python
[GT-DriveController] Importing logic module...
[Python] Controller initialized
[GT-DriveController] Python controller initialized successfully
[Test] Instantiation successful.
[Test] Testing Size=0 case...
[Test] doStep(Size=0) passed.
[Test] Testing Size>0 case...
[Test] doStep(Size>0) executed. Status: 0
[Test] Outputs: T=0 B=0 S=0
[Test] Done.
```

### 6. FMUパッケージの作成

テスト成功後、配布可能な`.fmu`ファイルを作成します。

```powershell
cd e:\Repository\GT-karny\GT-DriveController
.\create_fmu.ps1
```

**出力先**: `build/GT-DriveController.fmu` (約15.4 MB)

このスクリプトは以下を実行します:
1. FMI 2.0準拠のディレクトリ構造を作成
2. `modelDescription.xml`をコピー
3. `binaries/win64/`にDLLとPythonランタイムをコピー
4. `resources/`にPythonコントローラーをコピー
5. すべてを`.fmu`（ZIPアーカイブ）にパッケージ化

## プロジェクト構造

```
GT-DriveController/
├── CMakeLists.txt              # CMake設定
├── setup_python_runtime.ps1    # Python環境セットアップスクリプト
├── include/
│   ├── OSMPController.h        # コントローラーヘッダー
│   └── fmi2/                   # FMI 2.0ヘッダー
├── src/
│   ├── main.cpp                # FMI 2.0インターフェース実装
│   └── OSMPController.cpp      # コントローラー実装
├── tests/
│   └── test_fmu.cpp            # テストハーネス
├── resources/
│   ├── logic.py                # Pythonコントローラーロジック
│   └── python/                 # Python埋め込みランタイム
│       ├── python312.dll
│       ├── python312.zip       # 標準ライブラリ
│       ├── python312._pth      # sys.path設定
│       └── *.pyd               # 拡張モジュール
├── thirdparty/
│   ├── cpython/                # Python 3.12ソース
│   └── pybind11/               # pybind11ライブラリ
└── docs/
    └── build.md                # このファイル
```

## FMU配布パッケージの作成

FMUを配布する場合、以下のファイルが必要です:

```
GT-DriveController.fmu (ZIPアーカイブ)
├── modelDescription.xml        # FMI 2.0モデル記述
└── binaries/
    └── win64/
        ├── GT-DriveController.dll
        ├── python312.dll
        ├── python3.dll
        ├── python312.zip
        ├── python312._pth
        ├── vcruntime140.dll
        ├── vcruntime140_1.dll
        └── (必要に応じて*.pyd)
└── resources/
    ├── logic.py                # ユーザーコントローラー
    └── python/
        └── Lib/site-packages/  # 追加パッケージ
```

## トラブルシューティング

### Python初期化エラー

**症状**: `Fatal Python error: init_fs_encoding: failed to get the Python codec of the filesystem encoding`

**原因**: `python312._pth`ファイルが`python312.dll`と同じディレクトリにない

**解決策**: 
```powershell
Copy-Item resources\python\python312._pth build\Release\
```

### FMI関数が見つからない

**症状**: `[Test] Failed to load FMI functions.`

**原因**: `FMI2_FUNCTION_PREFIX`が定義されている

**解決策**: `CMakeLists.txt`で`FMI2_FUNCTION_PREFIX`の定義をコメントアウト（既に対応済み）

### モジュールインポートエラー

**症状**: `ModuleNotFoundError: No module named 'xxx'`

**原因**: 
1. `python312.zip`に標準ライブラリが含まれていない
2. `python312._pth`の設定が不正

**解決策**:
1. `setup_python_runtime.ps1`を再実行
2. `python312._pth`の内容を確認:
   ```
   python312.zip
   .\resources
   ```

## 参考資料

- [FMI 2.0仕様](https://fmi-standard.org/)
- [Python Embedding](https://docs.python.org/3.12/extending/embedding.html)
- [pybind11ドキュメント](https://pybind11.readthedocs.io/)
- [OSI (Open Simulation Interface)](https://github.com/OpenSimulationInterface/open-simulation-interface)
