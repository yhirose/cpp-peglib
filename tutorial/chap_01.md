はろーわーるど！
==============

まずはRubyでお馴染みの`irb`のようなREPL(Read-Eval-Print-Loop)環境を実現します。以下のソースコードを`repl.cc`に保存します。

```cpp
#include <iostream>
#include "linenoise.hpp"

using namespace std;

int main(void)
{
	while (true) {
	    // 一行入力
	    auto line = linenoise::Readline("> ");

		// 空行ならスキップ
	    if (line.empty()) {
	        break;
	    }

		// 入力テキストをそのまま出力
	    cout <<  "echo: '" << line << "'" << endl;

	    // 履歴に追加
	    linenoise::AddHistory(line.c_str());
	}
	return 0;
}
```

次に`linenoise.hpp`を[ここから](https://raw.githubusercontent.com/yhirose/cpp-linenoise/master/linenoise.hpp)ダウンロードして，`repl.cc`と同じディレクトリに保存します。私はこんな感じで`wget`を使いました。

```
wget https://raw.githubusercontent.com/yhirose/cpp-linenoise/master/linenoise.hpp
```

続いてコンパイルです。わたしは`clang++`のパージョン3.5を使っています。

```
clang++ -std='c++11' -o repl repl.cc
```

g++ 5.1では，

```
g++ -std='c+11 -o repl repl.cc'
```

Visual C++ 14 (Visual Studio 2015) では，

```
cl -DUNICODE /EHsc repl.cc User32.lib
```

とするとうまくいきました。

では生成された実行ファイル`repl`を実行してみましょう。

```
./repl
```

REPLのプロンプト`>`が表示されるので，何か入力してリターンキーを押してみましょう。

```
> はろーわーるど！
echo: 'はろーわーるど！'
> _
```

プログラミング入門の定番「はろーわーるど！」が表示されました！

実は入力したテキストは，履歴リストに追加されています。`Ctrl+P`か`↑`キーを押してみてください。

```
> はろーわーるど！_
```

このように`linenoise`ライブラリを使うと，以前に入力したテキストを呼び出すことができます。長い文字列を再度入力しなければならない時などはとても便利です。（`linenoise`に関する情報は[Githubのプロジェクトページ](https://github.com/yhirose/cpp-linenoise)をご覧ください。）

`Ctrl+C`を押すとREPLから抜けることができます。

まだPEGについて何も出てきませんね．．．とりあえずPEGライブラリを[ここから](https://raw.githubusercontent.com/yhirose/cpp-peglib/master/peglib.h)ダウンロードして，後の章のために準備しておきましょう。このファイル`peglib.h`も同じディレクトリに保存してください。

これで必要な準備が終わりました！
