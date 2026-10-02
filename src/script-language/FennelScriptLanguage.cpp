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
#include "FennelScriptLanguage.hpp"

#include "../generated/fennel_macros.gen.hpp"
#include "FennelScript.hpp"
#include "LuaScriptLanguage.hpp"
#include "../LuaState.hpp"
#include "../utils/VariantArguments.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/reg_ex.hpp>
#include <godot_cpp/classes/reg_ex_match.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace luagdextension {

String FennelScriptLanguage::_get_name() const {
	return "Fennel";
}

void FennelScriptLanguage::_init() {
	String fennel_path = "res://addons/fennel-gdextension/fennel.lua";
	String fennel_src = FileAccess::get_file_as_string(fennel_path);
	ERR_FAIL_COND_MSG(fennel_src.is_empty(), "FennelScriptLanguage: could not read " + fennel_path);

	// Use a separate, clean Lua state for the Fennel compiler so that
	// Godot-specific modifications to the Lua environment (e.g. string table
	// metatable changes) don't interfere with Fennel's own string operations.
	fennel_lua_state = luaL_newstate();
	sol::state_view L(fennel_lua_state);
	L.open_libraries();

	sol::load_result loaded = L.load(fennel_src.utf8().get_data(), "fennel.lua", sol::load_mode::text);
	ERR_FAIL_COND_MSG(!loaded.valid(), "FennelScriptLanguage: failed to load fennel.lua");

	sol::protected_function_result exec = ((sol::protected_function)loaded)();
	ERR_FAIL_COND_MSG(!exec.valid(), "FennelScriptLanguage: failed to execute fennel.lua");

	sol::table fennel_module = exec[0];
	L["fennel"] = fennel_module;

	fennel_compile_stream = fennel_module["compileStream"];
	fennel_string_stream = fennel_module["stringStream"];

	// Set fennel.path so import-macros can find .fnl files relative to the project root.
	// Module names use dots as separators: `:addons.lua-gdextension.defnode`
	// resolves to `<project_root>/addons/fennel-gdextension/defnode.fnl`
	String project_root = ProjectSettings::get_singleton()->globalize_path("res://");
	if (!project_root.is_empty()) {
		std::string root = project_root.utf8().get_data();
		std::string search_path = root + "?.fnl;" + root + "?/init.fnl";
		sol::optional<std::string> existing_path = fennel_module["path"];
		if (existing_path) {
			search_path += ";" + *existing_path;
		}
		fennel_module["path"] = search_path;
		// Also set macro-path so import-macros loads .fnl macro files in
		// compile-time scope (env="_COMPILER"), enabling quasiquotes in macro bodies.
		sol::optional<std::string> existing_macro_path = fennel_module["macro-path"];
		std::string macro_search_path = search_path;
		if (existing_macro_path) {
			macro_search_path += ";" + *existing_macro_path;
		}
		fennel_module["macro-path"] = macro_search_path;
	}

	install_global_macros(L, fennel_module);
}

void FennelScriptLanguage::install_global_macros(sol::state_view L, sol::table fennel_module) {
	sol::protected_function fennel_eval = fennel_module["eval"];

	for (int i = 0; i < FENNEL_MACRO_COUNT; i++) {
		sol::table eval_opts(L.lua_state(), sol::create);
		eval_opts["env"] = "_COMPILER";
		eval_opts["filename"] = std::string(FENNEL_MACROS[i].filename) + " (embedded)";

		sol::protected_function_result res = fennel_eval(
			std::string(FENNEL_MACROS[i].source), eval_opts);
		if (!res.valid()) {
			sol::error err = res;
			ERR_PRINT(String("FennelScriptLanguage: failed to load embedded macro '")
				+ FENNEL_MACROS[i].filename + "': " + err.what());
			continue;
		}
		sol::optional<sol::table> macro_mod = res[0];
		if (!macro_mod) {
			ERR_PRINT(String("FennelScriptLanguage: embedded macro '")
				+ FENNEL_MACROS[i].filename + "' eval returned non-table (res[0] is not a table)");
			// Try res[1] as fallback
			sol::optional<sol::table> macro_mod1 = res[1];
			if (!macro_mod1) {
				ERR_PRINT("FennelScriptLanguage: res[1] also not a table, skipping");
				continue;
			}
			macro_mod = macro_mod1;
		}

		// Register each macro directly into compiler.scopes.global.macros.
		// make_scope() inherits this table via __index so every compilation sees
		// these macros without any import-macros.
		L["_fennel_macro_mod"] = *macro_mod;
		auto script_result = L.safe_script(R"lua(
			local compiler = require("fennel.compiler")
			for k, v in pairs(_fennel_macro_mod) do
				compiler.scopes.global.macros[k] = v
			end
			_fennel_macro_mod = nil
		)lua", sol::script_pass_on_error);
		if (!script_result.valid()) {
			sol::error err = script_result;
			ERR_PRINT(String("FennelScriptLanguage: failed to register global macros: ") + err.what());
		}
	}
}

String FennelScriptLanguage::_get_type() const {
	return "FennelScript";
}

String FennelScriptLanguage::_get_extension() const {
	return "fnl";
}

void FennelScriptLanguage::_finish() {
	// Reset sol2 refs before closing the state they belong to
	fennel_compile_stream = sol::protected_function{};
	fennel_string_stream = sol::protected_function{};
	if (fennel_lua_state) {
		lua_close(fennel_lua_state);
		fennel_lua_state = nullptr;
	}
}

String FennelScriptLanguage::compile_fennel(const String &source, const String &path, HashMap<int, int> &out_source_map) const {
	std::string src = source.utf8().get_data();
	std::string filepath = path.utf8().get_data();

	sol::table opts(fennel_compile_stream.lua_state(), sol::create);
	opts["filename"] = filepath;

	sol::protected_function_result stream_result = fennel_string_stream(src);
	if (!stream_result.valid()) {
		sol::error err = stream_result;
		ERR_PRINT(String("Fennel: failed to create string stream: ") + err.what());
		return String();
	}

	sol::protected_function_result compile_result = fennel_compile_stream(stream_result[0], opts);
	if (!compile_result.valid()) {
		sol::error err = compile_result;
		String err_msg = err.what();
		if (err_msg.contains("symbols may only be used at compile time")) {
			return String("return {}");
		}
		ERR_PRINT(err_msg);
		return String();
	}

	// compile_result[0] = lua source string
	// compile_result[1] = file_sourcemap table: file_sourcemap[lua_line] = {filename, fennel_line}
	std::string lua_src = compile_result[0];

	sol::optional<sol::table> sm_table = compile_result[1];
	if (sm_table) {
		for (int lua_line = 1; lua_line <= (int)sm_table->size(); lua_line++) {
			sol::optional<sol::table> entry = (*sm_table)[lua_line];
			if (entry) {
				sol::optional<int> fennel_line = (*entry)[2];
				if (fennel_line) {
					out_source_map[lua_line] = *fennel_line;
				}
			}
		}
	}

	return String::utf8(lua_src.c_str());
}

PackedStringArray FennelScriptLanguage::_get_reserved_words() const {
	return godot::helpers::append_all(PackedStringArray(),
		// Special forms
		"fn", "lambda", "let", "local", "global", "var", "set", "do",
		"if", "when", "unless", "each", "for", "while",
		"collect", "icollect", "accumulate", "fcollect",
		"match", "match-try", "case", "case-try", "with-open",
		"values", "not", "and", "or", "true", "false", "nil",
		"->", "->>", "-?>", "-?>>", "doto", "partial", "pick-values",
		"require", "include", "import-macros", "require-macros",
		"macro", "macros", "eval-compiler", "macrodebug",
		"lua", "hashfn", "#", "?"
	);
}

bool FennelScriptLanguage::_is_control_flow_keyword(const String &keyword) const {
	return godot::helpers::append_all(PackedStringArray(),
		"if", "when", "unless", "do", "each", "for", "while", "match", "case"
	).has(keyword);
}

PackedStringArray FennelScriptLanguage::_get_comment_delimiters() const {
	return godot::helpers::append_all(PackedStringArray(), "; ");
}

PackedStringArray FennelScriptLanguage::_get_doc_comment_delimiters() const {
	return godot::helpers::append_all(PackedStringArray(), ";; ");
}

PackedStringArray FennelScriptLanguage::_get_string_delimiters() const {
	return godot::helpers::append_all(PackedStringArray(), "\" \"");
}

Ref<Script> FennelScriptLanguage::_make_template(const String &_template, const String &class_name, const String &base_class_name) const {
	Ref<FennelScript> script = memnew(FennelScript);
	String source_code = _template
		.replace("_BASE_", base_class_name)
		.replace("_CLASS_", class_name)
		.replace("_TS_", "\t");
	script->set_source_code(source_code);
	return script;
}

TypedArray<Dictionary> FennelScriptLanguage::_get_built_in_templates(const StringName &p_object) const {
#ifdef DEBUG_ENABLED
	Dictionary base_template;
	base_template["inherit"] = "Node";
	base_template["id"] = 0;
	base_template["name"] = "default";
	base_template["description"] = "Default template";
	base_template["origin"] = 0;
	base_template["content"] =
R"({:extends _BASE_})";

	Dictionary empty_template;
	empty_template["inherit"] = "Object";
	empty_template["id"] = 1;
	empty_template["name"] = "empty";
	empty_template["description"] = "Empty template";
	empty_template["origin"] = 0;
	empty_template["content"] = "";

	return Array::make(base_template, empty_template);
#else
	return {};
#endif
}

bool FennelScriptLanguage::_is_using_templates() {
	return true;
}

Dictionary FennelScriptLanguage::_validate(const String &script, const String &path, bool validate_functions, bool validate_errors, bool validate_warnings, bool validate_safe_lines) const {
	Dictionary result;

	std::string src = script.utf8().get_data();
	std::string filepath = path.utf8().get_data();

	sol::table opts(fennel_compile_stream.lua_state(), sol::create);
	opts["filename"] = filepath;

	sol::protected_function_result stream_result = fennel_string_stream(src);
	if (!stream_result.valid()) {
		result["valid"] = false;
		return result;
	}

	sol::protected_function_result compile_result = fennel_compile_stream(stream_result[0], opts);
	if (!compile_result.valid()) {
		sol::error err = compile_result;
		if (String(err.what()).contains("symbols may only be used at compile time")) {
			result["valid"] = true;
			return result;
		}
		if (validate_errors) {
			String error_msg = err.what();
			Ref<RegEx> line_re = RegEx::create_from_string(R":(\d+):");
			Ref<RegExMatch> match = line_re->search(error_msg);
			Dictionary error_dict;
			error_dict["path"] = path;
			error_dict["line"] = match.is_valid() ? match->get_string().to_int() : 1;
			error_dict["column"] = 1;
			error_dict["message"] = error_msg;
			result["errors"] = Array::make(error_dict);
		}
		result["valid"] = false;
	}
	else {
		result["valid"] = true;
	}
	return result;
}

String FennelScriptLanguage::_validate_path(const String &path) const {
	return "";
}

Object *FennelScriptLanguage::_create_script() const {
	return memnew(FennelScript);
}

bool FennelScriptLanguage::_has_named_classes() const {
	return false;
}

bool FennelScriptLanguage::_supports_builtin_mode() const {
	return true;
}

bool FennelScriptLanguage::_supports_documentation() const {
	return false;
}

bool FennelScriptLanguage::_can_inherit_from_file() const {
	return false;
}

int32_t FennelScriptLanguage::_find_function(const String &p_function, const String &p_code) const {
	return -1;
}

String FennelScriptLanguage::_make_function(const String &p_class_name, const String &p_function_name, const PackedStringArray &p_function_args) const {
	return {};
}

bool FennelScriptLanguage::_can_make_function() const {
	return false;
}

Error FennelScriptLanguage::_open_in_external_editor(const Ref<Script> &p_script, int32_t p_line, int32_t p_column) {
	return OK;
}

bool FennelScriptLanguage::_overrides_external_editor() {
	return false;
}

ScriptLanguage::ScriptNameCasing FennelScriptLanguage::_preferred_file_name_casing() const {
	return SCRIPT_NAME_CASING_AUTO;
}

Dictionary FennelScriptLanguage::_complete_code(const String &p_code, const String &p_path, Object *p_owner) const {
	return {};
}

Dictionary FennelScriptLanguage::_lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner) const {
	Dictionary result;
	result["result"] = ERR_UNAVAILABLE;
	result["type"] = Variant::Type::NIL;
	return result;
}

String FennelScriptLanguage::_auto_indent_code(const String &p_code, int32_t p_from_line, int32_t p_to_line) const {
	return p_code;
}

void FennelScriptLanguage::_add_global_constant(const StringName &p_name, const Variant &p_value) {
	LuaScriptLanguage::get_singleton()->_add_global_constant(p_name, p_value);
}

void FennelScriptLanguage::_add_named_global_constant(const StringName &p_name, const Variant &p_value) {
	LuaScriptLanguage::get_singleton()->_add_named_global_constant(p_name, p_value);
}

void FennelScriptLanguage::_remove_named_global_constant(const StringName &p_name) {
	LuaScriptLanguage::get_singleton()->_remove_named_global_constant(p_name);
}

void FennelScriptLanguage::_thread_enter() {}
void FennelScriptLanguage::_thread_exit() {}

String FennelScriptLanguage::_debug_get_error() const { return {}; }
int32_t FennelScriptLanguage::_debug_get_stack_level_count() const { return {}; }
int32_t FennelScriptLanguage::_debug_get_stack_level_line(int32_t p_level) const { return {}; }
String FennelScriptLanguage::_debug_get_stack_level_function(int32_t p_level) const { return {}; }
String FennelScriptLanguage::_debug_get_stack_level_source(int32_t p_level) const { return {}; }
Dictionary FennelScriptLanguage::_debug_get_stack_level_locals(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) { return {}; }
Dictionary FennelScriptLanguage::_debug_get_stack_level_members(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) { return {}; }
void *FennelScriptLanguage::_debug_get_stack_level_instance(int32_t p_level) { return {}; }
Dictionary FennelScriptLanguage::_debug_get_globals(int32_t p_max_subitems, int32_t p_max_depth) { return {}; }
String FennelScriptLanguage::_debug_parse_stack_level_expression(int32_t p_level, const String &p_expression, int32_t p_max_subitems, int32_t p_max_depth) { return {}; }
TypedArray<Dictionary> FennelScriptLanguage::_debug_get_current_stack_info() { return {}; }

void FennelScriptLanguage::_reload_all_scripts() {}

void FennelScriptLanguage::_reload_scripts(const Array &p_scripts, bool p_soft_reload) {
	for (int i = 0; i < p_scripts.size(); i++) {
		Ref<FennelScript> script = p_scripts[i];
		if (script.is_valid()) {
			script->reload(p_soft_reload);
		}
	}
}

void FennelScriptLanguage::_reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {}

PackedStringArray FennelScriptLanguage::_get_recognized_extensions() const {
	return godot::helpers::append_all(PackedStringArray(), "fnl");
}

TypedArray<Dictionary> FennelScriptLanguage::_get_public_functions() const { return {}; }
Dictionary FennelScriptLanguage::_get_public_constants() const { return {}; }
TypedArray<Dictionary> FennelScriptLanguage::_get_public_annotations() const { return {}; }

void FennelScriptLanguage::_profiling_start() {}
void FennelScriptLanguage::_profiling_stop() {}
void FennelScriptLanguage::_profiling_set_save_native_calls(bool p_enable) {}
int32_t FennelScriptLanguage::_profiling_get_accumulated_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) { return {}; }
int32_t FennelScriptLanguage::_profiling_get_frame_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) { return {}; }

void FennelScriptLanguage::_frame() {}

bool FennelScriptLanguage::_handles_global_class_type(const String &type) const {
	return type == _get_type();
}

Dictionary FennelScriptLanguage::_get_global_class_name(const String &path) const {
	Ref<FennelScript> script = ResourceLoader::get_singleton()->load(path);
	Dictionary result;
	if (script.is_valid() && script->_is_valid()) {
		result["name"] = script->_get_global_name();
		result["base_type"] = script->_get_instance_base_type();
		result["icon_path"] = script->_get_class_icon_path();
		result["is_abstract"] = script->_is_abstract();
		result["is_tool"] = script->_is_tool();
	}
	return result;
}

FennelScriptLanguage *FennelScriptLanguage::get_singleton() {
	return instance;
}

FennelScriptLanguage *FennelScriptLanguage::get_or_create_singleton() {
	if (!instance) {
		instance = memnew(FennelScriptLanguage);
		Engine::get_singleton()->register_script_language(instance);
	}
	return instance;
}

void FennelScriptLanguage::delete_singleton() {
	if (instance) {
		Engine::get_singleton()->unregister_script_language(instance);
		memdelete(instance);
		instance = nullptr;
	}
}

void FennelScriptLanguage::_bind_methods() {}

FennelScriptLanguage *FennelScriptLanguage::instance = nullptr;

}
