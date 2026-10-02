/**
 * Copyright (C) 2026 Gil Barbosa Reis.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "FennelScript.hpp"

#include "FennelScriptLanguage.hpp"
#include "LuaScriptLanguage.hpp"
#include "LuaScriptInstance.hpp"
#include "LuaScriptMetadata.hpp"
#include "../LuaError.hpp"
#include "../LuaFunction.hpp"
#include "../LuaState.hpp"
#include "../LuaTable.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/reg_ex.hpp>
#include <godot_cpp/classes/reg_ex_match.hpp>

#include <vector>

namespace luagdextension {

void FennelScript::_set_source_code(const String &p_code) {
	source_code = p_code;
}

Error FennelScript::_reload(bool keep_state) {
	// Evict this file's entry from package.loaded so callers that require it
	// get the fresh version on their next require call.
	clear_required_modules();

	placeholder_fallback_enabled = true;
	source_map.clear();

	String compiled_lua = FennelScriptLanguage::get_singleton()->compile_fennel(source_code, get_path(), source_map);
	if (compiled_lua.is_empty()) {
		return ERR_PARSE_ERROR;
	}

	Variant result = LuaScriptLanguage::get_singleton()->get_lua_state()->load_string(compiled_lua, get_path());
	if (LuaError *error = Object::cast_to<LuaError>(result)) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			ERR_PRINT(translate_error_message(error->get_message()));
		}
		return ERR_PARSE_ERROR;
	}

	result = Object::cast_to<LuaFunction>(result)->invokev(Array());
	if (LuaError *error = Object::cast_to<LuaError>(result)) {
		// Suppress runtime errors from addon utility files (lsp.fnl, godot_api_docs.fnl,
		// defnode.fnl) — they are Fennel modules, not standalone scripts, and will fail
		// when Godot tries to execute them as such.
		if (!get_path().begins_with("res://addons/fennel-gdextension/")) {
			ERR_PRINT(translate_error_message(error->get_message()));
		}
	}
	else if (LuaTable *table = Object::cast_to<LuaTable>(result)) {
		placeholder_fallback_enabled = false;
		metadata.clear();
		metadata.setup(table->get_table());
		if (!keep_state) {
			LuaScriptInstance::reset_all_for_script(this, metadata);
		}
	}
	return OK;
}

void FennelScript::clear_required_modules() {
	// Convert res://path/to/module.fnl → "path.to.module" (the key Fennel
	// uses in package.loaded) and evict it so the next require gets fresh code.
	String path = get_path();
	if (path.begins_with("res://")) {
		path = path.substr(6);
	}
	if (path.ends_with(".fnl")) {
		path = path.substr(0, path.length() - 4);
	}
	String module_name = path.replace("/", ".");

	sol::state_view lua = LuaScriptLanguage::get_singleton()->get_lua_state()->get_lua_state();
	sol::optional<sol::table> package_opt = lua.get<sol::optional<sol::table>>("package");
	if (!package_opt) {
		return;
	}
	sol::optional<sol::table> loaded_opt = package_opt->get<sol::optional<sol::table>>("loaded");
	if (!loaded_opt) {
		return;
	}
	loaded_opt->set(module_name.utf8().get_data(), sol::nil);
}

ScriptLanguage *FennelScript::_get_language() const {
	return FennelScriptLanguage::get_singleton();
}

String FennelScript::translate_error_message(const String &msg) const {
	if (source_map.is_empty()) {
		return msg;
	}
	// Find the first :number: pattern — that's the Lua line number in the error string.
	Ref<RegEx> re = RegEx::create_from_string(":(\\d+):");
	Ref<RegExMatch> match = re->search(msg);
	if (match.is_null()) {
		return msg;
	}
	int lua_line = match->get_string(1).to_int();
	const int *fennel_line = source_map.getptr(lua_line);
	if (!fennel_line) {
		return msg;
	}
	// Replace only the digits portion, keeping the surrounding colons.
	String lua_line_str = match->get_string(1);
	String fennel_line_str = String::num_int64(*fennel_line);
	int colon_pos = msg.find(":" + lua_line_str + ":");
	if (colon_pos < 0) {
		return msg;
	}
	return msg.substr(0, colon_pos + 1) + fennel_line_str + msg.substr(colon_pos + 1 + lua_line_str.length());
}

void FennelScript::_bind_methods() {
}

}
