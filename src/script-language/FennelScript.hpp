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
#ifndef __FENNEL_SCRIPT_EXTENSION_HPP__
#define __FENNEL_SCRIPT_EXTENSION_HPP__

#include <godot_cpp/templates/hash_map.hpp>

#include "LuaScript.hpp"

using namespace godot;

namespace luagdextension {

class FennelScript : public LuaScript {
	GDCLASS(FennelScript, LuaScript);

public:
	Error _reload(bool p_keep_state) override;
	void _set_source_code(const String &p_code) override;
	ScriptLanguage *_get_language() const override;
	String translate_error_message(const String &msg) const override;

protected:
	static void _bind_methods();

private:
	// lua_line (1-based) → fennel_line
	HashMap<int, int> source_map;

	// Evict all non-stdlib entries from package.loaded so that modules
	// required by this script are reloaded fresh on next require.
	void clear_required_modules();
};

}

#endif  // __FENNEL_SCRIPT_EXTENSION_HPP__
