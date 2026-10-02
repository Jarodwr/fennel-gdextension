# Fennel Script Language Support

Add `.fnl` (Fennel) as a first-class script language in lua-gdextension. Fennel compiles to
Lua, so at runtime Fennel scripts are identical in performance to hand-written Lua. The key
additions are a compile step in `_reload()` and a source-map lookup so error messages show
Fennel line numbers instead of compiled-Lua line numbers.

## Architecture decisions

- `FennelScript` inherits `LuaScript` — reuses `LuaScriptInstance`, `LuaScriptMetadata`,
  all method/property/signal wiring. Only `_reload()`, `_get_language()`, and
  `translate_error_message()` are overridden.
- `FennelScriptLanguage` is a new `ScriptLanguageExtension` that shares the single
  `LuaState` owned by `LuaScriptLanguage`. No second Lua VM.
- `fennel.lua` (v1.5.0) is vendored at `addons/lua-gdextension/fennel.lua` and loaded at
  `FennelScriptLanguage::_init()` via Godot `FileAccess`. Submodule lives at
  `vendor/fennel` (sourcehut, tag 1.6.1) but the built artifact is copied from
  `~/Development/fennel-ls-rs/fennel-ls/vendor/fennel.lua`.
- Source map: `fennel.compileStream` returns `(lua_src, file_sourcemap)` where
  `file_sourcemap[lua_line] = {filename, fennel_line}`. Stored as `HashMap<int,int>` in
  `FennelScript` and used to rewrite line numbers in error strings.

## Tasks

### Infrastructure
- [x] Vendor `fennel.lua` → `addons/lua-gdextension/fennel.lua` (v1.5.0, from fennel-ls-rs)
- [x] `vendor/fennel` submodule added (sourcehut, tag 1.6.1) — source reference only

### Modify existing files

- [x] `src/script-language/LuaScript.hpp/.cpp`
      Added `virtual String translate_error_message(const String& msg) const` (default
      returns `msg` unchanged). `FennelScript` overrides it to do source-map lookup.

- [x] `src/script-language/LuaScriptInstance.cpp` — two changes:
  1. `get_language_func`: now uses `instance->script->_get_language()->_owner`
  2. `property_get_revert_func` ERR_PRINT wrapped with `translate_error_message`

- [x] `src/script-language/LuaScriptResourceFormatSaver.cpp`
      `_recognize()` now excludes resources whose path ends in `.fnl`

### New files

- [x] `src/script-language/FennelScript.hpp/.cpp`
- [x] `src/script-language/FennelScriptLanguage.hpp/.cpp`
- [x] `src/script-language/FennelScriptResourceFormatLoader.hpp/.cpp`
- [x] `src/script-language/FennelScriptResourceFormatSaver.hpp/.cpp`

### Register in main.cpp

- [x] All four classes registered
- [x] `FennelScriptLanguage::get_or_create_singleton()`
- [x] Loader/saver registered and torn down in correct order

### Stretch

- [ ] Bytecode cache — hash Fennel source, write compiled Lua to
      `.godot/fennel_cache/<hash>.lua`, skip recompile if hash matches. Eliminates
      per-startup compile cost for exported projects.
- [ ] Syntax highlighting — `FennelSyntaxHighlighter` using a simple tokenizer or
      tree-sitter-fennel (separate submodule).
