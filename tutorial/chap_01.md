# まずは，Hello world

はじめに取り組むプログラムは，もちろん**hello world**ですよね。

では「hello world!」と正しく入力できたら「OK」，そうでなければ「NG」と表示するプログラムを作ってみましょう！以下がソースコードです。

```cpp
// hello.cc
#include <iostream>
#include "peglib.h"

using namespace std;

// 文法定義
const auto grammar = R"(
    PROGRAM <- _ 'hello' _ 'world' '!' _
    _       <- [ \t]*
)";

int main(void)
{
    // 文法を読み込んでパーサーを生成
    peglib::peg parser(grammar);

    // 文法に誤りがあったかチェック
    if (!parser) {
        cerr << "grammar error..." << endl;
        return -1;
    }

    while (true) {
        // 文字列を一行読み込み
        cout << "> ";
        string line;
        getline(cin, line);

        // ユーザーからの入力をパース
        if (parser.parse(line.c_str())) {
            cout << "OK" << endl;
        } else {
            cout << "NG" << endl;
        }
    }

    return 0;
}
```

このコードを`hello.cc`に保存して，それからコンパイルしてみましょう。このコードはPEGパーサライブラリを使用するので，[ここから](https://raw.githubusercontent.com/yhirose/cpp-peglib/master/peglib.h)`peglib.h`ダウンロードして`hello.cc`があるディレクトリに保存してください。

コンパイル時にはC++11の機能を有効にする必要があります。`clang++`のパージョン3.5ではこんな感じになります。

    clang++ -std='c++11' -o hello hello.cc

g++ 5.1でもほとんど同じで，

    g++ -std='c+11' -o hello hello.cc'

Visual C++ 14 (Visual Studio 2015) では，

    cl -DUNICODE /EHsc hello.cc User32.lib

とするとコンパイルできました。

では生成された実行ファイルを実行してみましょう。画面に`>`が表示され，ユーザーに入力を促します。何か文字列を入力してリターンキーを押してみましょう。正確に「hello world!」と入力すると「OK」と表示されます。何かおかしな入力をすると「NG」になります。

```
> はろーわーるど！
NG
> hello world!
OK
>    hello   world!
OK
```

見事にPEG版Hello worldをクリアです！（プログラムを終了したい時は`Ctrl+C`を押してください。）

--

では，このプログラムのPEGに関する部分を順を追ってみましょう。

まずはPEGライブラリを読み込みます。`peglib.h`はC++ header-only libraryですので，他にファイルは必要ありません。

```cpp
#include "peglib.h"
```

続いてPEGで文法を定義します。この文法は「hello world!」という文字列を受け付けるだけのとても簡単なものです。入力文字列の前後や「hello」と「world」の間には，任意の長さのスペースやタブを入れることができます。(ちなみに「world」と「!」の間には入れることができません。)

```cpp
const auto grammar = R"(
    PROGRAM <- _ 'hello' _ 'world' '!' _
    _       <- [ \t]*
)";
```

脇にそれますが，この文法には一つ大きなバグがあります。実は「helloworld!」のように言葉をくっつけて入力しても結果が「OK」になってしまいます。このバグの解決法は後の章で説明します。

この文法を理解するPEGパーサーを生成しましょう。`peglib::peg`がパーサーです。先ほどの定義した文法をコンストラクタに渡してパーサーを生成します。

```cpp
    // 文法を読み込んでパーサーを生成
    peglib::peg parser(grammar);

    // 文法に誤りがあったかチェック
    if (!parser) {
        cerr << "grammar error..." << endl;
        return -1;
    }
```

文法にエラーがあると、上記のように`parser`オブジェクトの真偽値が'false'になります。

最後に`parser.parse`メソッドを呼び、ユーザーの入力した文字列をパースします。成功すると`true`が返ります。

```cpp
        // ユーザーからの入力をパース
        if (parser.parse(line.c_str())) {
            cout << "OK" << endl;
        } else {
            cout << "NG" << endl;
        }
```

このように，パーサジェネレータを使うと簡単に構文解析パーサを生成ことができます。もちろん実用的な言語の文法はもっと複雑ですし，パーサは構文チェックをするだけでコードを実行することできません。実際に動作するインタープリタ型言語の作成には，次のようなステップが必要です。

  1. 言語の文法を定義する
  2. パーサーを生成する
  3. ソースコードをパースして、AST（抽象構文木）を生成する
  4. ASTを実行するインタープリタを作成する

PEGライブラリはステップ2と3のみ扱い，ステップ1と2は自分で扱わなければなりません。しかし，この文法定義とインタープリタ作成の部分が一番面白いところで，デザインセンスと実装技術の見せ所です。。

これからの章では，それぞれのステップを順を追って説明していきます。その際，実際にコードを動かしたり拡張していくなら，より一層理解が深まるに違いありません。

Happy Hacking!
