# Technical Design Document: OSI-Python Bridge & Controller Architecture

本ドキュメントでは、esminiから受け取ったOSIデータをPythonロジックへ橋渡しし、状態を保持しながら車両制御を行うための技術的詳細について解説します。

## 1. OSIデータフロー（C++ to Python）

esmini（またはOSI出力を持つシミュレータ）から渡される `SensorView` は、メモリ上のポインタとして提供されます。これをPython側で安全かつ効率的に扱うため、以下のプロセスでデータを転送します。

### データ変換プロセス

1. **C++受信**: esminiからバイナリポインタ（BaseLo/BaseHi）とサイズ情報を受け取ります。
2. **バリデーション**: ポインタの有効性を検証し、異常なサイズ（>100MB）でないかチェックします。
3. **Pythonへ転送**: `pybind11` の `py::bytes` を用いて、ポインタから直接バイナリデータをコピーし、Pythonのメソッドに引数で渡します。
4. **Python復元**: Python側で `osi3` ライブラリ（Google Protobuf）を用い、受け取ったバイナリを `ParseFromString` でデコードします。

> C++側で特定のプロパティだけを抽出してPythonに渡すと、Python側のロジックを変更するたびにC++側のラッパーコードも修正・再ビルドが必要になります。バイナリ（OSIそのまま）を渡すことで、**Python側でOSIの全データに自由にアクセスできる柔軟性**を確保しています。

### 制御出力とOSIフィードバック（Python to C++）

Pythonコントローラーは、計算結果として制御コマンド（Throttle/Brake/Steer/DriveMode）だけでなく、**加工済みのOSIバイナリデータ**を返すことも可能です。

1. **Python戻り値**: `[throttle, brake, steering, drive_mode, osi_output_bytes]` のリストを返します。
2. **ダブルバッファリング**: C++側では、Pythonから受け取ったバイナリデータを保持するために2つの固定長バッファ（`std::string`）を用意しています。毎ステップ交互に書き込むことで、**「現在のステップで取得したポインタの内容が、次の `doStep` 完了まで不変であること」**を保証します。
3. **ポインタ変換**: C++側でバッファのアドレスを `BaseLo/BaseHi` にエンコードし、FMU出力変数としてシミュレータへ提供します。

---

## 2. Python Controller クラス設計

制御ロジックは、シミュレーションのステップ間で内部状態（前回値、積分値、他車の追従フラグなど）を保持するため、クラス形式で実装します。

### クラス構成（Composition Pattern）

`Controller` クラスを最上位とし、役割ごとにサブクラスを保持する「コンポジション」を採用します。

* **`Controller` (Main)**: C++から直接呼ばれるエントリポイント。データの受信と各サブシステムへの分配を担当。
* **`LongitudinalControl`**: 加減速、PID制御、先行車追従ロジックなどを担当。
* **`LateralControl`**: ステアリング、Pure Pursuit、車線維持ロジックなどを担当。

### 状態保持の仕組み

Pythonのインスタンス変数は、C++側のラッパーがインスタンスを保持し続ける限りメモリ上に残り続けます。これにより、PID制御の `integral` 値などを毎ステップ持ち越すことが可能です。

---

## 3. C++側でのPython管理（Life Cycle）

C++のFMUラッパーは、`pybind11` を介してPythonインタプリタの生存期間とオブジェクトの参照を管理します。

### ライフサイクル管理

1. **`Instantiate` (インスタンス化)**:
* コントローラーオブジェクトのみを生成。FMI初期化シーケンスに従い、重量処理は行いません。

2. **`EnterInitializationMode` (初期化開始時)**:
* `py::scoped_interpreter` を起動（初回のみ）。
* `resources` ディレクトリを `sys.path` に追加し、`logic.py` を検索可能にします。
* `logic.py` 内の `Controller` クラスをインスタンス化し、`py::object` として保持。

3. **`DoStep` (計算実行)**:
* **GIL取得**: スレッド安全のため Python GIL を取得。
* **データコピー**: `py::bytes` でOSIデータをコピー（不正ポインタ保護付き）。
* **Python実行**: `update_control` を呼び出し。
* **出力セット**: 制御値のセットに加え、戻り値のバイナリをダブルバッファに格納しポインタを更新。

4. **`FreeInstance` (破棄)**:
* **GIL取得**: Pythonオブジェクト解放のため GIL を取得。
* `m_pyController = py::none()` により参照を確実に解放。
* インタプリタの停止（グローバルデストラクタによる）。



---

## 4. 配布・ポータビリティの実現

本プロジェクトでは、異なるPC環境での動作を保証するため **Embeddable Python** を採用しています。

### 相対パスによる解決

FMUがどこに展開されても、C++ DLL（`binaries/win64/`）から見た相対パスで `resources/python_env` を特定します。

* **URI デコード**: `fmuResourceLocation` が URI エンコードされている場合（`%20` など）も正しくデコードして実パスを特定します。
* **`pythonXX._pth`**: 標準ライブラリの検索パスをFMU内に固定。`_pth` ファイルを使用する場合、`Py_SetPythonHome` は使用せず、自動構成に任せます。

---

### まとめ

この構成により、**「C++並みのデータアクセス（OSIポインタ処理）」**と**「Pythonの生産性（制御アルゴリズム開発）」**を両立し、かつ**「依存関係のない単一のFMUファイル」**としての配布を可能にしています。

---
