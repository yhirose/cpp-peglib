#include <iostream>
#include "linenoise.hpp"

using namespace std;

int main(void)
{
	while (true) {
	    // テキスト入力
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
