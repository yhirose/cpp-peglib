// Setup editros
function setupInfoArea(id) {
  const e = ace.edit(id);
  e.setShowPrintMargin(false);
  e.setOptions({
    readOnly: true,
    highlightActiveLine: false,
    highlightGutterLine: false
  })
  e.renderer.$cursorLayer.element.style.opacity=0;
  return e;
}

function setupEditorArea(id, lsKey) {
  const e = ace.edit(id);
  e.setShowPrintMargin(false);
  e.setValue(localStorage.getItem(lsKey) || '');
  e.moveCursorTo(0, 0);
  return e;
}

const grammar = setupEditorArea("grammar-editor", "grammarText");
const code = setupEditorArea("code-editor", "codeText");

const codeAst = setupInfoArea("code-ast");
const codeAstOptimized = setupInfoArea("code-ast-optimized");
const profile = setupInfoArea("profile");

$('#opt-mode').val(localStorage.getItem('optimazationMode') || 'all');
$('#packrat').prop('checked', localStorage.getItem('packrat') === 'true');
$('#auto-refresh').prop('checked', localStorage.getItem('autoRefresh') === 'true');
$('#parse').prop('disabled', $('#auto-refresh').prop('checked'));

// Parse
function escapeHtml(unsafe) {
  return unsafe
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

function generateErrorListHTML(errors) {
  let html = '<ul>';

  html += $.map(errors, function (x) {
    return '<li data-ln="' + x.ln + '" data-col="' + x.col + '"><span>' + x.ln + ':' + x.col + '</span> <span>' + escapeHtml(x.msg) + '</span></li>';
  }).join('');

  html += '<ul>';

  return html;
}

function updateLocalStorage() {
  localStorage.setItem('grammarText', grammar.getValue());
  localStorage.setItem('codeText', code.getValue());
  localStorage.setItem('optimazationMode', $('#opt-mode').val());
  localStorage.setItem('packrat', $('#packrat').prop('checked'));
  localStorage.setItem('autoRefresh', $('#auto-refresh').prop('checked'));
}

function parse() {
  const $grammarValidation = $('#grammar-validation');
  const $grammarInfo = $('#grammar-info');
  const grammarText = grammar.getValue();

  const $codeValidation = $('#code-validation');
  const $codeInfo = $('#code-info');
  const codeText = code.getValue();

  const optimazationMode = $('#opt-mode').val();
  const packrat = $('#packrat').prop('checked');
  const autoRefresh = $('#auto-refresh').prop('checked');

  $grammarInfo.html('');
  $grammarValidation.hide();
  $codeInfo.html('');
  $codeValidation.hide();
  codeAst.setValue('');
  codeAstOptimized.setValue('');
  profile.setValue('');

  if (grammarText.length === 0) {
   return;
  }

  const mode = optimazationMode == 'all';
  console.log({packrat});
  const data = JSON.parse(Module.lint(grammarText, codeText, mode, packrat));

  if (data.grammar_valid) {
    $grammarValidation.removeClass('editor-validation-invalid').text('Valid').show();

    codeAst.insert(data.ast);
    codeAstOptimized.insert(data.astOptimized);
    profile.insert(data.profile);

    if (data.source_valid) {
      $codeValidation.removeClass('editor-validation-invalid').text('Valid').show();
    } else {
      $codeValidation.addClass('editor-validation-invalid').text('Invalid').show();
    }

    if (data.code.length > 0) {
      const html = generateErrorListHTML(data.code);
      $codeInfo.html(html);
    }
  } else {
    $grammarValidation.addClass('editor-validation-invalid').text('Invalid').show();
  }

  if (data.grammar.length > 0) {
    const html = generateErrorListHTML(data.grammar);
    $grammarInfo.html(html);
  }
}

// Event handing for text editiing
let timer;
function setupTimer() {
  clearTimeout(timer);
  timer = setTimeout(() => {
    updateLocalStorage();
    if ($('#auto-refresh').prop('checked')) {
      parse();
    }
  }, 750);
};
grammar.getSession().on('change', setupTimer);
code.getSession().on('change', setupTimer);

// Event handing in the info area
function makeOnClickInInfo(editor) {
  return function () {
    const el = $(this);
    editor.navigateTo(el.data('ln') - 1, el.data('col') - 1);
    editor.scrollToLine(el.data('ln') - 1, true, false, null);
    editor.focus();
  }
};
$('#grammar-info').on('click', 'li', makeOnClickInInfo(grammar));
$('#code-info').on('click', 'li', makeOnClickInInfo(code));

// Event handing in the AST optimazation
$('#opt-mode').on('change', setupTimer);
$('#packrat').on('change', setupTimer);
$('#auto-refresh').on('change', () => {
  updateLocalStorage();
  $('#parse').prop('disabled', $('#auto-refresh').prop('checked'));
  setupTimer();
});
$('#parse').on('click', parse);

// Show page
$('#main').css({
  'display': 'flex',
});

// WebAssembly
var Module = {
  onRuntimeInitialized: function() {
    // Initial parse
    parse();
  }
};
