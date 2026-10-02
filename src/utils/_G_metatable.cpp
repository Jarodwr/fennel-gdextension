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
#include "_G_metatable.hpp"

#include "convert_godot_lua.hpp"
#include "../script-language/LuaScriptLanguage.hpp"

#include <godot_cpp/classes/resource_loader.hpp>

using namespace godot;

namespace luagdextension {

// __index for the `gd` namespace table.
// Singletons and ClassDB classes are populated eagerly at module load time.
// This handles only named globals (dynamic) and user global class paths (lazy-loaded scripts).
sol::object gd_index(sol::this_state state, sol::table gd, sol::stack_object key) {
	static ResourceLoader *resource_loader = ResourceLoader::get_singleton();

	if (key.get_type() != sol::type::string) {
		return sol::nil;
	}

	auto key_str = key.as<StringName>();

	Variant named_global = LuaScriptLanguage::get_singleton()->get_named_globals().get(key_str, nullptr);
	if (named_global.get_type() != Variant::NIL) {
		return gd[key] = to_lua(state, named_global);
	}

	auto registry = sol::state_view(state).registry();
	if (const char *global_class_path = registry.get<sol::table>("_GDEXTENSION_GLOBAL_CLASS_PATHS").get_or(key, (const char *) nullptr)) {
		Ref<Resource> res = resource_loader->load(global_class_path);
		return gd[key] = to_lua(state, res);
	}

	return sol::nil;
}

void setup_G_metatable(sol::state_view& state) {
	// All Godot types live under the `gd` namespace table.
	// _G's metatable is intentionally not set — bare class names are not globals.
	sol::table gd = state.create_table();
	gd[sol::metatable_key] = state.create_table_with(
		sol::meta_function::index, &gd_index
	);
	state.globals()["gd"] = gd;
}

}
