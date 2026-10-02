# Fork divergences

This is a personal fork of [lua-gdextension](https://github.com/gilzoide/lua-gdextension). The changes below document where and why it diverges from upstream.

---

## 1. `gd` namespace — all Godot globals moved under one table

**Upstream:** Every Godot class (`Node3D`, `Vector3`, …), utility function (`lerp`, `sin`, …), enum value (`KEY_ENTER`, …), singleton (`Engine`, `OS`, …), and script API helper (`signal`, `export`, `export_range`) is injected directly into `_G`. Bare names like `Node3D` and `lerp` work anywhere in a script.

**Fork:** Nothing is injected into `_G`. All of the above live under a single `gd` table. Scripts use `gd.Node3D`, `gd.lerp`, `gd.signal`, etc.

**Why:** Avoids polluting the global namespace, makes code-completion tractable (one prefix to complete against), and makes it obvious at the call site that something is a Godot binding.

**Key files changed:**
- `src/utils/_G_metatable.cpp` — lazy ClassDB lookup metatable moved from `_G` to `gd`; `_G` metatable is no longer set
- `src/luaopen/variant.cpp` — all `state.set(...)` → `gd.set(...)`
- `src/luaopen/utility_functions.cpp` — `register_utility_functions(gd, state)`, manual `gd.set` calls
- `src/luaopen/enums.cpp` — `register_global_enums(gd)`
- `src/script-language/LuaScriptProperty.cpp` — all `state.set(...)` → `gd.set(...)`
- `src/script-language/LuaScriptSignal.cpp` — `gd.set("signal", ...)`
- `tools/code_generation/generate_cpp_code.py` — `register_utility_functions(target, state)` two-param macro; `register_global_enums(target)` single-param macro

---

## 2. Fennel compiler runs in an isolated Lua state

**Upstream:** No Fennel support.

**Fork:** Fennel scripts (`.fnl`) are compiled to Lua before execution. The Fennel compiler (`vendor/fennel/fennel.lua`) runs in a **separate `lua_State`** created with `luaL_newstate()`, completely isolated from the Godot runtime state.

**Why:** Godot's extension modifies the `string` table metatable (to expose `String` methods). Fennel's compiler is itself written in Lua and uses `string` operations internally — running it inside the Godot state caused `attempt to index a sol.godot::Variant value` errors inside Fennel's `stringStream`.

**Key files changed:**
- `src/script-language/FennelScriptLanguage.cpp` — `fennel_lua_state = luaL_newstate()` in `_init()`; `fennel.path` set to the Godot project root so `import-macros` resolves `.fnl` files

---

## 3. `sol_lua_push` exact-match overload for `std::string`

**Upstream:** Passing a `const char*` or `std::string` to a sol2 push would trigger sol2's ADL dispatch, which found the `sol_lua_push(lua_State*, const godot::String&)` overload via an implicit `const char* → godot::String` conversion. This caused plain C++ strings to be pushed to Lua as Godot `Variant` userdatas.

**Fork:** Added an exact-match overload in `src/utils/custom_sol.hpp`:

```cpp
inline int sol_lua_push(lua_State* L, const std::string& s) {
    lua_pushlstring(L, s.c_str(), s.size());
    return 1;
}
```

This short-circuits the ADL lookup and pushes `std::string` as a plain Lua string.

---

## 4. Explicit sub-table protocol replacing flat-table type dispatch

**Upstream:** A Lua script returns a single flat table. `LuaScriptMetadata::setup` iterates it with `pairs()` and dispatches on each value's type: `LuaScriptSignal` userdata → signal, `LuaScriptProperty` userdata → property, `function` → method, known string key (`extends`, `tool`, …) → metadata, anything else → implicit property with default.

**Fork:** Scripts return a table with named sub-tables. `LuaScriptMetadata::setup` reads them directly by key — no type sniffing:

| Key | Contents | How read |
|---|---|---|
| `extends`, `tool`, `class_name`, `icon` | scalar metadata | direct key access |
| `__signals` | `{ name = true, … }` | iterate keys |
| `__exports` | `{ name = LuaScriptProperty, … }` | iterate key/value pairs |
| `__methods` | `{ name = { fn = function, rpc = nil\|Dictionary }, … }` | iterate, read `fn` and `rpc` fields |

RPC config is now co-located with the method definition (in the `rpc` field) rather than a separate top-level `rpc_config` table. The `rpc_config` Dictionary returned by `_get_rpc_config()` is built from `__methods` entries where `rpc` is non-nil.

**Key files changed:**
- `src/script-language/LuaScriptMetadata.cpp` — rewritten `setup()`; removed `_G_pairs` static reference and `StackTopResetter`

---

## 5. `defnode` Fennel macro

**Upstream:** No Fennel support.

**Fork:** The `addons/lua-gdextension/defnode.fnl` macro provides explicit, structured syntax for Fennel scripts. It expands at compile time into the sub-table structure described above, so script authors never write `__signals` or `__methods` directly.

```fennel
(import-macros {: defnode} :addons.fennel-gdextension.defnode)

(defnode MyNode extends gd.Node3D
  (tool)
  (signal pinged)
  (export speed (gd.export_range 1.0 100.0 0.5))
  (fn _ready [self] ...)
  (rpcfn sync_pos ["any_peer" "reliable"] [self pos] ...))
```

`(fn ...)` expands to `{ fn = function, rpc = nil }`. `(rpcfn ...)` expands to `{ fn = function, rpc = gd.rpc(...) }`.

---

## 6. `gd.rpc` — RPC helper moved into the `gd` namespace

**Upstream:** `rpc(...)` is a global helper function that builds an RPC config Dictionary.

**Fork:** Moved to `gd.rpc(...)` for namespace consistency. The `rpcfn` form in `defnode` calls `gd.rpc` automatically — script authors generally don't call it directly.

**Key files changed:**
- `src/script-language/globals.lua` — `function rpc(...)` → `gd.rpc = function(...)`
- `src/generated/lua_script_globals.h` — regenerated

---

## 7. Fennel LSP type provider

**Upstream:** No Fennel support.

**Fork:** `addons/lua-gdextension/godot_api_docs.fnl` generates a `fennel-ls` global-docs table at analysis time by reading `extension_api.json` (bundled with the addon). All completions are prefixed `gd.*`. Wire it from the project's `.lsp.fnl`:

```fennel
(local godot (require :addons.lua-gdextension.godot_api_docs))
{:platform :lua54 :global-docs godot}
```

**Key files changed:**
- `SConstruct` — copies `vendor/json.lua` and `lib/godot-cpp/gdextension/extension_api.json` into the addon directory at build time
- `addons/lua-gdextension/godot_api_docs.fnl` — new file
- `addons/lua-gdextension/defnode.fnl` and `godot_api_docs.fnl` registered as tracked addon files in SConstruct
