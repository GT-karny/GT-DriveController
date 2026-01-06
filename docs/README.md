# GT-DriveController Documentation

GT-DriveControllerは、Python 3.12を埋め込んだFMI 2.0 Co-Simulation FMUです。OSI (Open Simulation Interface)を使用して、シミュレーション環境と通信します。

## ドキュメント一覧

### [ビルドガイド](build.md)
プロジェクトのビルド手順、環境セットアップ、トラブルシューティングを説明します。

**内容**:
- 前提条件とツール
- Python 3.12のビルド
- Python埋め込みランタイムのセットアップ
- CMake設定とビルド
- テスト実行
- FMU配布パッケージの作成

### [実装ガイド](implementation.md)
アーキテクチャ、コンポーネント設計、実装の詳細を説明します。

**内容**:
- アーキテクチャ概要
- FMI 2.0インターフェース実装
- OSMPController実装
- Pythonコントローラーインターフェース
- Python埋め込み環境の詳細
- CMake設定のポイント
- デバッグとパフォーマンス

## クイックスタート

```powershell
# 1. Python 3.12をビルド
cd thirdparty\cpython\PCbuild
.\build.bat

# 2. Python埋め込み環境をセットアップ
cd ..\..\..\
.\setup_python_runtime.ps1

# 3. プロジェクトをビルド
cmake -B build -S .
cmake --build build --config Release

# 4. ランタイムファイルをコピー
Copy-Item resources\python\python312.* build\Release\
Copy-Item resources\python\vcruntime*.dll build\Release\

# 5. テスト実行
cd build\Release
.\test_fmu.exe
```

## プロジェクト構造

```
GT-DriveController/
├── docs/                       # ドキュメント
│   ├── README.md              # このファイル
│   ├── build.md               # ビルドガイド
│   └── implementation.md      # 実装ガイド
├── include/                    # ヘッダーファイル
├── src/                        # ソースコード
├── tests/                      # テストコード
├── resources/                  # リソースファイル
│   ├── logic.py               # Pythonコントローラー
│   └── python/                # Python埋め込みランタイム
├── thirdparty/                 # サードパーティライブラリ
│   ├── cpython/               # Python 3.12ソース
│   └── pybind11/              # pybind11
├── CMakeLists.txt             # CMake設定
└── setup_python_runtime.ps1   # セットアップスクリプト
```

## 主要な技術スタック

- **FMI 2.0**: Functional Mock-up Interface for Co-Simulation
- **Python 3.12**: 埋め込みインタープリタ
- **pybind11**: C++/Pythonバインディング
- **OSI**: Open Simulation Interface
- **CMake**: ビルドシステム
- **Visual Studio**: C++コンパイラ

## ライセンス

このプロジェクトは以下のオープンソースコンポーネントを使用しています:
- Python 3.12: PSF License
- pybind11: BSD-style license
- FMI Standard: CC-BY-SA

## サポート

問題が発生した場合は、[ビルドガイドのトラブルシューティングセクション](build.md#トラブルシューティング)を参照してください。
