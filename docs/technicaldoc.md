# Technical Design Document: OSI-Python Bridge & Controller Architecture

本ドキュメントでは、esminiから受け取ったOSIデータをPythonロジックへ橋渡しし、状態を保持しながら車両制御を行うための技術的詳細について解説します。

## 1. OSIデータフロー（C++ to Python）

esmini（またはOSI出力を持つシミュレータ）から渡される `SensorView` は、メモリ上のポインタとして提供されます。これをPython側で安全かつ効率的に扱うため、以下のプロセスでデータを転送します。

### データ変換プロセス

1. **C++受信**: esminiから提供されたバイナリポインタとサイズを基に、C++側で `osi3::SensorView` オブジェクトをパースします（`ParseFromArray`）。
2. **シリアライズ**: C++オブジェクトを一度標準的なバイナリ形式（`std::string`）にシリアライズします（`SerializeToString`）。
3. **Pythonへ転送**: `pybind11` の `py::bytes` 型を用いて、メモリコピーを伴うバイナリデータとしてPythonのメソッドに引数で渡します。
4. **Python復元**: Python側で `osi3` ライブラリ（Google Protobuf）を用い、バイナリからPythonオブジェクトを生成します（`ParseFromString`）。

> [!TIP]
> **なぜ二度パースするのか？**
> C++側で特定のプロパティだけを抽出してPythonに渡すと、Python側のロジックを変更するたびにC++側のラッパーコードも修正・再ビルドが必要になります。バイナリ（OSIそのまま）を渡すことで、**Python側でOSIの全データに自由にアクセスできる柔軟性**を確保しています。

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

1. **`Instantiate` (初期化)**:
* `py::scoped_interpreter` を起動。
* Embeddable Pythonのパス（`resources/python_env`）を `sys.path` に追加。
* `logic.py` 内の `Controller` クラスをインスタンス化し、C++のメンバ変数 `py::object py_controller` に格納。


2. **`DoStep` (計算実行)**:
* `py_controller.attr("update_control")(py::bytes(data))` を実行。
* 戻り値の `list` を `std::vector<double>` にキャストし、FMUの出力ポートへセット。


3. **`FreeInstance` (破棄)**:
* Pythonオブジェクトの参照カウントを解放。
* インタプリタの停止。



---

## 4. 配布・ポータビリティの実現

本プロジェクトでは、異なるPC環境での動作を保証するため **Embeddable Python** を採用しています。

### 相対パスによる解決

FMUがどこに展開されても、C++ DLL（`binaries/win64/`）から見た相対パスで `resources/python_env` を特定します。

* **`Py_SetPythonHome`**: Python本体の検索パスをFMU内の `resources/python_env` に固定。
* **`pythonXX._pth`**: `import site` を有効化することで、`resources/python_env/Lib/site-packages` 内の外部ライブラリ（`numpy`, `osi-python`）を優先的に読み込みます。

---

### まとめ

この構成により、**「C++並みのデータアクセス（OSIポインタ処理）」**と**「Pythonの生産性（制御アルゴリズム開発）」**を両立し、かつ**「依存関係のない単一のFMUファイル」**としての配布を可能にしています。

---
