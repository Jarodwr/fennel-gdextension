@tool
extends SyntaxHighlighter

var _hl: FennelSyntaxHighlighter
var _cache: Dictionary = {}
var _updating := false

func _init() -> void:
	_hl = FennelSyntaxHighlighter.new()
	_hl.fill_editor_colors()

func _update_cache() -> void:
	if _updating:
		print("[FennelHL] _update_cache re-entrant call blocked")
		return
	_updating = true

	var te := get_text_edit()
	print("[FennelHL] _update_cache start, te=", te)
	if not te:
		_updating = false
		return

	var text := te.get_text()
	var default_color: Color = te.get_theme_color("font_color", "CodeEdit")
	print("[FennelHL] calling compute_highlights, text len=", text.length())
	_cache = _hl.compute_highlights(text, default_color)
	print("[FennelHL] cache lines=", _cache.size())

	_updating = false

func _get_line_syntax_highlighting(line: int) -> Dictionary:
	if line == 0:
		print("[FennelHL] _get_line_syntax_highlighting(0) returning hardcoded test")
		return {0: {"color": Color.RED}}
	return {}
