# まずは"Hello world!"

はじめに取り組むプログラムは，もちろん**Hello world!**ですよね。

では「hello world!」と正しく入力できたら「OK」，そうでなければ「NG」と表示するプログラムを作ってみましょう！以下がソースコードです。

```cpp
// hello.cc
#include <iostream>
#include "peglib.h"
#include "linenoise.hpp"
using namespace std;

int main(void)
{
    // 文法を読み込んでパーサーを生成
    peg::parser parser(R"(
        PROGRAM <- _ HELLO _ WORLD '!' _
        HELLO   <- [hH] 'ello'
        WORLD   <- [wW] 'orld'
        _       <- [ \t]*
    )");

    if (!parser) { // 文法に誤りがあったかチェック
        cerr << "grammar error..." << endl;
        return -1;
    }

    while (true) {
        auto line = linenoise::Readline("> "); // 文字列を一行読み込み

        if (line == "exit") { break; }         // 終了

        if (parser.parse(line.c_str())) {      // ユーザーからの入力をパース
            cout << "OK" << endl;
        } else {
            cout << "NG" << endl;
        }

        linenoise::AddHistory(line.c_str());   // 入力履歴に追加
    }

    return 0;
}
```

このコードを`hello.cc`として保存してください。このコードは以下の2つのライブラリを使用するので，ダウンロードして`hello.cc`と同じディレクトリに保存してください。

  * cpp-peglib C++ PEG parser library - [peblib.h](https://raw.githubusercontent.com/yhirose/cpp-peglib/master/peglib.h)
  * cpp-linenoise C++ Readline library - [linenoise.hpp](https://raw.githubusercontent.com/yhirose/cpp-linenoise/master/linenoise.hpp)

ではコンパイルしましょう。コンパイル時にはC++11の機能を有効にする必要があります。`clang++`のパージョン3.5ではこんな感じになります。

    clang++ -std='c++11' -o hello hello.cc

g++ 5.1でもほとんど同じで，

    g++ -std='c+11' -o hello hello.cc'

Visual C++ 14 (Visual Studio 2015) では，

    cl /EHsc hello.cc

とするだけでコンパイルできます。

では生成された実行ファイルを実行してみましょう。画面に`>`が表示され，ユーザーに入力を促します。何か文字列を入力してリターンキーを押してみましょう。正確に「hello world!」と入力すると「OK」と表示されます。何かおかしな入力をすると「NG」になります。

```
> はろーわーるど！
NG
> hello world!
OK
>    hello   world!
OK
```

見事にPEG版Hello worldをクリアです！（プログラムを終了したい時は`exit`を入力してください。）

--

では，このプログラムのPEGに関する部分を順を追ってみましょう。

まずはPEGライブラリを読み込みます。`peglib.h`はC++ header-only libraryですので，他にファイルは必要ありません。

```cpp
#include "peglib.h"
```

続いてPEGで文法を定義します。この文法は「hello world!」という文字列を受け付けるだけのとても簡単なものです。入力文字列の前後や「hello」と「world」の間には，任意の長さのスペースやタブを入れることができます。(ちなみに「world」と「!」の間には入れることができません。)

```
    PROGRAM <- _ 'hello' _ 'world' '!' _
    _       <- [ \t]*
```

この文法を理解するPEGパーサーを生成しましょう。`peglib::peg`がパーサーです。先ほどの定義した文法をコンストラクタに渡してパーサーを生成します。

```cpp
    // 文法を読み込んでパーサーを生成
    peg::parser parser(R"(
        PROGRAM <- _ HELLO _ WORLD '!' _
        HELLO   <- [hH] 'ello'
        WORLD   <- [wW] 'orld'
        _       <- [ \t]*
    )");

    if (!parser) { // 文法に誤りがあったかチェック
        cerr << "grammar error..." << endl;
        return -1;
    }
```

文法にエラーがあると、上記のように`parser`オブジェクトの真偽値が'false'になります。

最後に`parse`メソッドを呼び、ユーザーの入力した文字列をパースします。成功すると`true`が返ります。

```cpp
        if (parser.parse(line.c_str())) {      // ユーザーからの入力をパース
            cout << "OK" << endl;
        } else {
            cout << "NG" << endl;
        }
```

これでパーサジェネレータを使う準備ができました。

実用的な言語の文法はずっと複雑ですし，パーサだけでコードを実行することできません。実際に動作する状態に持って行くには，さらに行うべきことがあります。インタープリタ型言語の場合，次のようなステップが必要です。

  1. 言語の文法を定義する
  2. パーサーを生成する
  3. ソースコードをパースして、AST（抽象構文木）を生成する
  4. ASTを実行するインタープリタを作成する

PEGライブラリはステップ2と3のみ扱い，残りは自分で扱わなければなりません。しかし，この文法定義とインタープリタ作成の部分が個性を出せる一番面白いところで，デザインセンスと実装技術の見せ所です。

次章では，PEGがどんなものかを見てみましょう。
