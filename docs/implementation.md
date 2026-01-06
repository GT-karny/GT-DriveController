# GT-DriveController FMU - 実装ガイド

## アーキテクチャ概要

GT-DriveControllerは、C++で実装されたFMI 2.0 Co-Simulation FMUで、Python 3.12インタープリタを埋め込んでいます。OSI (Open Simulation Interface)プロトコルを使用して、シミュレーション環境から車両センサーデータを受け取り、Pythonで実装されたコントローラーロジックを実行します。

```
┌─────────────────────────────────────────────────┐
│         シミュレーション環境 (e.g., esmini)      │
└────────────────┬────────────────────────────────┘
                 │ OSI SensorView (バイナリ)
                 ▼
┌─────────────────────────────────────────────────┐
│          GT-DriveController.dll (FMU)           │
│  ┌───────────────────────────────────────────┐  │
│  │  FMI 2.0 Interface (main.cpp)             │  │
│  │  - fmi2Instantiate                        │  │
│  │  - fmi2DoStep                             │  │
│  │  - fmi2SetInteger / fmi2GetReal           │  │
│  └──────────────┬────────────────────────────┘  │
│                 │                                │
│  ┌──────────────▼────────────────────────────┐  │
│  │  OSMPController (C++)                     │  │
│  │  - OSIポインタのデコード                  │  │
│  │  - Python初期化・管理                     │  │
│  │  - Pythonコントローラー呼び出し           │  │
│  └──────────────┬────────────────────────────┘  │
│                 │ pybind11                       │
│  ┌──────────────▼────────────────────────────┐  │
│  │  Python 3.12 Interpreter                  │  │
│  │  ┌────────────────────────────────────┐   │  │
│  │  │  logic.py                          │   │  │
│  │  │  class Controller:                 │   │  │
│  │  │    def update_control(osi_data)    │   │  │
│  │  └────────────────────────────────────┘   │  │
│  └───────────────────────────────────────────┘  │
└────────────────┬────────────────────────────────┘
                 │ 制御出力 (throttle, brake, steering)
                 ▼
┌─────────────────────────────────────────────────┐
│         シミュレーション環境 (車両モデル)        │
└─────────────────────────────────────────────────┘
```

## コンポーネント詳細

### 1. FMI 2.0インターフェース (`src/main.cpp`)

FMI 2.0標準に準拠したエントリーポイントを提供します。

**主要な関数**:

```cpp
// 1. FMUのインスタンス化 (軽量: オブジェクト生成のみ)
fmi2Component fmi2Instantiate(...);

// 2. 初期化モードへの進入 (重量: Python初期化、モジュールロード)
fmi2Status fmi2EnterInitializationMode(fmi2Component c);

// 3. シミュレーションステップの実行 (GIL管理、ポインタ検証)
fmi2Status fmi2DoStep(fmi2Component c, ...);
```

**重要な設計決定**:
- `FMI2_FUNCTION_PREFIX`を定義しない（DLLエクスポートのため）
- `OSMPController`インスタンスを`fmi2Component`として返す
- 重い初期化処理（Pythonインタープリタの起動など）は`fmi2EnterInitializationMode`で行う

### 2. OSMPController (`src/OSMPController.cpp`)

FMUのコアロジックを実装します。

**主要な機能**:

#### 2.1 Python初期化

```cpp
void OSMPController::GlobalInitializePython(const std::wstring& pythonHome) {
    if (!g_interpreter) {
        // python312._pthファイルを使用する場合、
        // Py_SetPythonHome()を呼び出さない
        g_interpreter = std::make_unique<py::scoped_interpreter>();
    }
}
```

**重要**: `python312._pth`ファイルが存在する場合、`Py_SetPythonHome()`を呼び出すと競合が発生します。

#### 2.2 OSIポインタのデコード

OSMPプロトコルでは、OSIデータへのポインタを2つの32ビット整数（BaseLo, BaseHi）として渡します。

```cpp
void* OSMPController::decodePointer(fmi2Integer hi, fmi2Integer lo) {
    if constexpr (sizeof(void*) == 8) {
        unsigned long long address = ((unsigned long long)hi << 32) | (unsigned int)lo;
        return reinterpret_cast<void*>(address);
    } else {
        return reinterpret_cast<void*>((uintptr_t)lo);
    }
}
```

#### 2.3 Pythonコントローラー呼び出し

```cpp
fmi2Status OSMPController::doStep(fmi2Real currentCommunicationPoint, 
                                   fmi2Real communicationStepSize) {
    if (m_osi_size > 0 && m_osi_baseLo != 0) {
        try {
            // 1. ポインタをデコード
            void* rawPtr = decodePointer(m_osi_baseHi, m_osi_baseLo);
            
            // 2. ポインタとサイズのバリデーション (メモリ安全性)
            if (!rawPtr || m_osi_size > MAX_OSI_SIZE) {
                return fmi2Warning;
            }

            // 3. Python GILの取得 (スレッド安全性)
            py::gil_scoped_acquire acquire;

            // 4. Pythonバイトオブジェクトを作成
            // ポインタが無効な場合のセグフォを防ぐためtry-catchで保護
            py::bytes data;
            try {
                data = py::bytes(reinterpret_cast<const char*>(rawPtr), m_osi_size);
            } catch (...) {
                return fmi2Warning;
            }
            
            // 5. Pythonコントローラーを呼び出し
            py::object result = m_pyController.attr("update_control")(data);
            
            // 6. 結果をパース
            if (py::isinstance<py::list>(result)) {
                py::list resList = result.cast<py::list>();
                if (resList.size() >= 3) {
                    m_throttle = resList[0].cast<float>();
                    m_brake = resList[1].cast<float>();
                    m_steering = resList[2].cast<float>();
                }
            }
        } catch (py::error_already_set& e) {
            // Pythonトレースバックを含むエラー出力
            std::cerr << "[GT-DriveController] Python Error: " << e.what() << std::endl;
            return fmi2Warning;
        } catch (std::exception& e) {
            std::cerr << "[GT-DriveController] Error: " << e.what() << std::endl;
            return fmi2Warning;
        }
    }
    return fmi2OK;
}
```

### 3. Pythonコントローラー (`resources/logic.py`)

ユーザーが実装するコントローラーロジックです。

**インターフェース**:

```python
class Controller:
    def __init__(self):
        """コントローラーの初期化"""
        pass
    
    def update_control(self, osi_data: bytes) -> list[float]:
        """
        OSI SensorViewデータを処理し、制御出力を返す
        
        Args:
            osi_data: シリアライズされたOSI SensorViewデータ
        
        Returns:
            [throttle, brake, steering] のリスト
            - throttle: 0.0 ~ 1.0 (アクセル開度)
            - brake: 0.0 ~ 1.0 (ブレーキ圧)
            - steering: -1.0 ~ 1.0 (ステアリング角度、左が負)
        """
        # OSIデータをパース（例: protobuf使用）
        # from osi3.osi_sensorview_pb2 import SensorView
        # sensor_view = SensorView()
        # sensor_view.ParseFromString(osi_data)
        
        # コントローラーロジックを実装
        throttle = 0.5
        brake = 0.0
        steering = 0.0
        
        return [throttle, brake, steering]
```

## FMI変数定義

### 入力変数

| 名前 | Value Reference | 型 | 説明 |
|------|----------------|-----|------|
| `OSI_SensorView_In.base.lo` | 0 | Integer | OSIポインタの下位32ビット |
| `OSI_SensorView_In.base.hi` | 1 | Integer | OSIポインタの上位32ビット |
| `OSI_SensorView_In.size` | 2 | Integer | OSIデータのサイズ（バイト） |

### 出力変数

| 名前 | Value Reference | 型 | 説明 |
|------|----------------|-----|------|
| `Throttle` | 3 | Real | スロットル開度 (0.0 ~ 1.0) |
| `Brake` | 4 | Real | ブレーキ圧 (0.0 ~ 1.0) |
| `Steering` | 5 | Real | ステアリング角度 (-1.0 ~ 1.0) |

## Python埋め込み環境

### ファイル構成

```
build/Release/
├── GT-DriveController.dll      # FMU本体
├── python312.dll                # Python本体 (7.14 MB)
├── python3.dll                  # Python3 stub (55 KB)
├── python312.zip                # 標準ライブラリ (11.6 MB)
├── python312._pth               # sys.path設定ファイル
├── vcruntime140.dll             # VC++ランタイム
└── vcruntime140_1.dll
```

### python312._pth の役割

このファイルはPythonのモジュール検索パス（sys.path）を設定します。

```
python312.zip
.\resources

# Uncomment to enable site-packages
# import site
```

**重要な特性**:
1. `python312.dll`と同じディレクトリに配置する必要がある
2. このファイルが存在すると、Pythonは自動的にこれを使用する
3. `Py_SetPythonHome()`と併用すると競合が発生する

### 標準ライブラリのZIPアーカイブ

Python標準ライブラリを`python312.zip`にアーカイブすることで:
- ファイル数を大幅に削減（数千ファイル → 1ファイル）
- 配布が容易になる
- ロード時間がわずかに向上

**制限事項**:
- `.pyd`ファイル（拡張モジュール）はZIPに含められない
- 動的にロードされるモジュールは個別に配置が必要

## CMake設定のポイント

```cmake
# Python 3.12のパス設定
set(PYTHON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cpython")
set(PYTHON_INCLUDE_DIRS 
    "${PYTHON_SOURCE_DIR}/Include" 
    "${PYTHON_SOURCE_DIR}/PC"
)

# python312.libとpython3.libの両方をリンク
set(PYTHON_LIBRARY 
    "${PYTHON_SOURCE_DIR}/PCbuild/amd64/python312.lib"
    "${PYTHON_SOURCE_DIR}/PCbuild/amd64/python3.lib"
)

# FMI2_FUNCTION_PREFIXは定義しない（DLLエクスポートのため）
# target_compile_definitions(GT-DriveController PRIVATE FMI2_FUNCTION_PREFIX=...)
```

## デバッグのヒント

### Pythonエラーのキャッチ

```cpp
try {
    py::object result = m_pyController.attr("update_control")(data);
} catch (py::error_already_set& e) {
    std::cerr << "Python error: " << e.what() << std::endl;
    // Pythonトレースバックを表示
    if (e.trace()) {
        std::cerr << "Traceback:\n" << e.trace() << std::endl;
    }
}
```

### ログ出力

診断出力を有効にするには、`OSMPController.cpp`の`std::cout`行のコメントを外します。

```cpp
std::cout << "[GT-DriveController] Resource path: " << m_resourcePath << std::endl;
std::cout << "[GT-DriveController] Python home: " << pythonHome.string() << std::endl;
```

## パフォーマンス考慮事項

1. **OSIデータのコピー**: `py::bytes`作成時にメモリコピーが発生します。大きなデータの場合、パフォーマンスに影響する可能性があります。

2. **Python GIL**: Pythonのグローバルインタープリタロック（GIL）により、マルチスレッド環境では注意が必要です。

3. **初期化コスト**: Python初期化は最初のインスタンス化時に1回だけ実行されます。

## 今後の拡張

- OSI TrafficCommandのサポート
- マルチスレッド対応
- Pythonパッケージの動的インストール機能
- ホットリロード機能（開発時）
