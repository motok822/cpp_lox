# cpp_lox

C++17 で実装した [Lox](https://craftinginterpreters.com/) 言語のバイトコードコンパイラ & スタックベース VM。

Robert Nystrom 著 *Crafting Interpreters* の clox をベースに、配列・例外処理・クラス継承などを独自に拡張しています。

## ビルド

```
make
```

## 使い方

```bash
# REPL
./clox

# スクリプト実行
./clox script.lox
```

## テスト

```bash
make test
```

## 言語仕様

### データ型

```lox
var n = 42;          // 数値 (double)
var s = "hello";     // 文字列
var b = true;        // 真偽値 (true / false)
var x = nil;         // nil
```

### 変数とスコープ

```lox
var global = "g";
{
    var local = "l";
    print local;
}
```

### 制御構文

```lox
if (x > 0) { print "positive"; } else { print "non-positive"; }

while (i < 10) { i = i + 1; }

for (var i = 0; i < 5; i = i + 1) {
    if (i == 3) continue;
    print i;
}
```

### 関数とクロージャ

```lox
fun makeCounter() {
    var count = 0;
    fun increment() {
        count = count + 1;
        return count;
    }
    return increment;
}

var counter = makeCounter();
print counter(); // 1
print counter(); // 2
```

### 配列

```lox
var arr[3];
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
print arr[1]; // 20
```

関数の戻り値に対する添字アクセスも可能です:

```lox
fun getArray() { return arr; }
print getArray()[0]; // 10
```

### クラスと継承

```lox
class Animal {
    init(name) { this.name = name; }
    speak() { print this.name + " says ..."; }
}

class Dog < Animal {
    speak() { print this.name + " says Woof!"; }
}

class Puppy < Dog {
    speak() {
        super.speak();
        print "Yip!";
    }
}
```

### 例外処理

```lox
fun divide(a, b) {
    if (b == 0) throw "division by zero";
    return a / b;
}

try {
    print divide(10, 0);
} catch (e) {
    print e; // "division by zero"
}
```

`throw` は任意の値を投げられます。関数呼び出しを跨いだ例外伝播にも対応しています。

## プロジェクト構成

```
src/
  main.cpp            # エントリポイント (REPL / ファイル実行)
include/
  scanner.h           # 字句解析器
  compiler.h / .cpp   # バイトコードコンパイラ
  vm.h                # 仮想マシン
  chunk.h             # バイトコード / オペコード定義
  object.h            # ランタイムオブジェクト (文字列, 関数, クロージャ, クラス, 配列 等)
  value.h / .cpp      # Value 型
  table.h             # ハッシュテーブル
  debug.h             # ディスアセンブラ
  common.h            # 共通定義
tests/
  main.cpp            # テストランナー
  classes/            # クラス・継承のテスト
  closures/           # クロージャのテスト
  exceptions/         # 例外処理のテスト
  functions/          # 関数のテスト
  global_variables/   # グローバル変数のテスト
  local_variables/    # ローカル変数のテスト
```
