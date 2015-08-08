まずは，Hello World!
===================

プログラマーあれば，何はともあれ，まずは「Hello World」ですよね。それでは，「hello world!」と正しく入力できたら「OK」，そうでなければ「NG」と表示するプログラムを作ってみましょう！

これがソースコードです。

```cpp
// hello.cc
#include <iostream>
#include "peglib.h"

using namespace std;

// 文法定義
const auto grammar = R"(
    PROGRAM <- _ HELLO _ WORLD '!' _
    HELLO   <- [hH] 'ello'
    WORLD   <- [wW] 'orld'
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

        if (line == "quit") {
            break;
        }

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

このコードを`hello.cc`に保存して，それからコンパイルしてみましょう。PEG Parser Libraryを使用するので，[ここから](https://raw.githubusercontent.com/yhirose/cpp-peglib/master/peglib.h)`peglib.h`ダウンロードして，`hello.cc`があるディレクトリに保存してください。

コンパイルにはC++11の機能を有効にする設定が必要です。わたしは普段`clang++`のパージョン3.5を使っているので，こんな感じになります。

```
clang++ -std='c++11' -o hello hello.cc
```

g++ 5.1でもほとんど同じで，

```
g++ -std='c+11' -o hello hello.cc'
```

Visual C++ 14 (Visual Studio 2015) では，

```
cl -DUNICODE /EHsc hello.cc User32.lib
```

とするとうまくいきました。

では生成された実行ファイルを実行してみましょう。すると画面に`>`が表示され，ユーザーに入力を促します。文字列を入力してリターンキーを押してみましょう。正しく「hello world!」と入力すると「OK」と表示されます。間違うと「NG」です。ちなみに「   Hello   World!   」もOKです。

```
> はろーわーるど！
NG
> hello world!
OK
>    Hello   World!   
OK
```

どうでしょう，見事にPEG版Hello Worldが動きました！（プログラムを終了したい時は`quit`を入力するか，`Ctrl+C`を押してください。）

--

では，このプログラムを順を追って見てみましょう。

まずはPEGライブラリを読み込みます。`peglib.h`はC++ header-only libraryですので、libファイルをリンクする必要はありません。

```cpp
#include "peglib.h"
```

続いてPEGで文法を定義します。今回は「Hello World!」という文字列を受け付けるとても簡単な文法です。言葉の先頭は大文字でも小文字でも受け入れてくれます。「＿」は0以上長さのスペース文字やタブ文字，つまり「空白」を定義しています。入力文字列の前後や，「hello」と「world」の間には空白入れることができます。(ちなみに「world」と「!」の間には空白をいれることができません。)

```cpp
const auto grammar = R"(
    PROGRAM <- _ HELLO _ WORLD '!' _
    HELLO   <- [hH] 'ello'
    WORLD   <- [wW] 'orld'
    _       <- [ \t]*
)";
```

脇にそれますが，この文法には一つ大きなバグがあります。実は「helloworld!」のように言葉をくっつけて入力しても結果が「OK」になってしまいます。というのは「HELLO _ WORLD」の中の「＿」は「0以上」を意味する「*」を使っているので，長さ0の空文字列にもマッチしてしまうからです。このバグの解決法は後ほど改めてしたいと思います。

この文法を理解するPEGパーサーを生成しましょう。`peglib::peg`がパーサーです。先ほどの定義した文法をコンストラクタに渡してパーサーを生成します。

```cpp
    peglib::peg parser(grammar);

    if (!parser) {
        cerr << "grammar error..." << endl;
        return -1;
    }
```

文法にエラーがあると、上記のように`parser`オブジェクトの真偽値はfalseになります。

最後に`parser.parse`メソッドを呼び出して、ユーザーの入力した文字列をパースします。そしてコードに構文エラーがあるかどうかを、メソッドの戻り値が教えてくれます。

```cpp
        if (parser.parse(line.c_str())) {
            cout << "OK" << endl;
        } else {
            cout << "NG" << endl;
        }
```

ではこれを起点として，より実用的な言語を作っていきましょう。言語の作成には次のようなステップが必要です。

  1. 文法を定義する（PEGの表記を使う。もちろん自分で書きます）
  2. パーサーを生成する（PEGライブラリを使う）
  3. ソースコードをパースして、構文木（AST―Abstract Syntax Tree）を生成する
  4. ASTを実行する（ここも自分でコードを書かないといけません）

これからの章では，それぞれのステップを順を追って説明していきます。その際，実際にコードを動かしたり，自分で拡張していくなら，より一層理解が深まるに違いありません。Happy Hacking!

--

次章では、PEGでどのように言語の文法をデザインしていくかを見てみましょう。
