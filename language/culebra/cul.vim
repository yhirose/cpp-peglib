
syn match   culOperator     "\%(+\|-\|/\|*\|=\|\^\|&\||\|!\|>\|<\|%\)=\?"
syn match   culDecNumber    "\<[0-9][0-9_]*"
syn match   culFuncCall     "\w\(\w\)*("he=e-1,me=e-1
syn match   culError        ";"
syn match   culError        "\s*$"
syn match   culLineComment  "\(\/\/\|#\).*" contains=@Spell,javaScriptCommentTodo

syn keyword culFunction     fn
syn keyword culSelf         self
syn keyword culConditional	if else
syn keyword culRepeat		while
syn keyword culReturn		return
syn keyword culDebugger		debugger
syn keyword culBoolean		true false
syn keyword culCommentTodo  TODO FIXME XXX TBD contained
syn keyword culStorage      mut

syn region  culStringS      start=+'+ skip=+\\\\\|\\'+ end=+'\|$+
syn region  culStringD      start=+"+ skip=+\\\\\|\\"+ end=+"\|$+
syn region  culComment      start="/\*" end="\*/" contains=@Spell,javaScriptCommentTodo

hi def link culBoolean	       Boolean
hi def link culComment	       Comment
hi def link culCommentTodo	   Todo
hi def link culConditional	   Conditional
hi def link culDecNumber	   Number
hi def link culFuncCall        Function
hi def link culFunction        Type
hi def link culLineComment	   Comment
hi def link culOperator        Operator
hi def link culRepeat	       Repeat
hi def link culReturn	       Statement
hi def link culDebugger	       Debug
hi def link culSelf            Constant
hi def link culStorage         StorageClass
hi def link culStringD	       String
hi def link culStringS	       String
hi def link culError	       Error

let b:current_syntax = "cul"
