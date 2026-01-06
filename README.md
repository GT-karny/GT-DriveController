# GT DriveController

esminiからのOSIデータ（SensorView）をバイナリポインタ経由で受け取り、内部に埋め込まれたPythonロジックで車両制御（Accel/Brake/Steer）を行い、Chrono等の車両物理FMUへ指令値を送るためのFMI 2.0準拠コントローラーFMUです。

## 1. システム構成

本FMUは、以下の共同シミュレーション（Co-simulation）環境での使用を想定しています。

| 役割 | ソフトウェア | インターフェース |
| --- | --- | --- |
| **シナリオ再生 / センサー** | esmini | ASAM OSI (via Pointer) |
| **コントローラー** | **本FMU (Python Embedded)** | FMI 2.0 (CS) |
| **車両ダイナミクス** | Project Chrono (FMU) | FMI 2.0 (CS) |

---

## 2. 特徴

* **双方向OSI通信:** 入力だけでなく、Pythonからの計算結果をOSIバイナリとしてFMU外部へ出力可能。
* **柔軟な開発:** 制御アルゴリズムは `logic.py` を書き換えるだけで即座に変更可能。`PythonScriptPath` パラメータでスクリプトファイルの場所も自由に変更できます。再ビルドは不要です。
* **メモリ安全性:** ダブルバッファリングにより、出力されるOSIポインタのメモリ安定性を保証。
* **OSIネイティブ:** Python側にはバイナリとしてデータを渡し、標準的な `osi3` ライブラリ（Google Protobuf）を用いて直感的にデータを扱えます。

---

## 3. ディレクトリ構造

```text
.
├── CMakeLists.txt              # ビルド定義
├── fmu/
│   └── modelDescription.xml    # FMUの変数・入出力定義
├── src/
│   ├── main.cpp                # FMI 2.0 エントリポイント
│   └── OSMPController.cpp      # C++ Wrapper (Python呼び出し・OSIシリアライズ)
├── include/
│   └── OSMPController.h        # クラス定義
├── python/
│   └── logic.py                # 制御ロジック本体（ここを直接編集）
└── scripts/
    └── setup_env.py            # Python環境構築・ライブラリ同梱スクリプト

```

---

## 4. 開発環境のセットアップ

### 必須ツール

* **OS:** Windows 10/11 (64-bit)
* **Compiler:** Visual Studio 2022 (MSVC)
* **Build System:** CMake 3.15以上
* **Package Manager:** vcpkg

### ライブラリの準備

```powershell
vcpkg install pybind11 protobuf

```

### Python環境の自動構築 (Embeddable Python)

以下のスクリプトを実行し、`resources/python_env` に実行用環境を構築します。

```powershell
python scripts/setup_env.py

```

> [!NOTE]
> このスクリプトは、Embeddable Pythonのダウンロード、`._pth` ファイルの修正（`import site` の有効化）、および `numpy`, `osi-python` 等の必須ライブラリを `site-packages` へインストールする処理を自動で行います。

---

## 5. ビルドとパッケージング

1. **ビルド手順:**
```powershell
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

```


2. **FMUの作成:**
`build/fmu_root/` 内の全ファイルをZIP形式で圧縮し、拡張子を `.fmu` に変更してください。

---

## 6. 使用方法

### 制御ロジックのカスタマイズ

`resources/logic.py` 内の `Controller` クラスを編集します。`update_control` メソッドは毎ステップ呼ばれ、OSIバイナリを受け取ります。

```python
def update_control(self, raw_bytes):
    # OSIのデシリアライズ
    sv = osi_sv.SensorView()
    sv.ParseFromString(raw_bytes)
    
    # 周辺車両や自車位置に基づいた計算
    # ... 
    
    # [accel, brake, steer, drive_mode, osi_output_bytes] を返す
    return [0.2, 0.0, -0.05, 1, raw_bytes]

```

### デバッグ方法

esminiの実行ログ、またはPython内の `print()` 出力を通じて、リアルタイムに内部変数の確認が可能です。

---

## License

This project is a hybrid-licensed project based on the following components:

* **OSMP Framework**: The C++ wrapper and framework code are licensed under the [Mozilla Public License, v. 2.0 (MPL-2.0)](https://www.mozilla.org/en-US/MPL/2.0/).
* **FMI Standard Headers**: The FMI headers (`fmi2/3.h`) are provided by the Modelica Association under the [2-Clause BSD License](https://opensource.org/licenses/BSD-2-Clause).
* **Python Embedded Runtime**: The Python runtime and standard libraries bundled in `resources/python_env` are subject to the [Python Software Foundation (PSF) License Agreement](https://docs.python.org/3/license.html).
* **User Control Logic**: The specific control algorithms implemented in `resources/logic.py` (and any other user-added Python modules) are provided under the [MIT License](https://opensource.org/licenses/MIT) (or your preferred license).

### Redistribution of FMU
When redistributing the generated `.fmu` file, please ensure that the copyright notices for the FMI headers and the Python runtime are preserved within the `resources` directory as required by their respective licenses.

---