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
#include "LuaScriptMetadata.hpp"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>

#include "LuaScriptInstance.hpp"
#include "../utils/Class.hpp"
#include "../utils/VariantType.hpp"
#include "../utils/convert_godot_lua.hpp"
#include "../utils/string_names.hpp"

namespace luagdextension {

void LuaScriptMetadata::setup(const sol::table& t) {
	is_valid = true;

	// Built-in methods always available on every instance
	methods[string_names->rawget] = LuaScriptMethod(string_names->rawget, LuaScriptInstance::rawget);
	methods[string_names->rawset] = LuaScriptMethod(string_names->rawset, LuaScriptInstance::rawset);

	// Scalar metadata keys
	sol::object extends_obj = t["extends"];
	if (extends_obj.get_type() != sol::type::lua_nil) {
		StringName extends = to_variant(extends_obj);
		if (!ClassDB::class_exists(extends)) {
			WARN_PRINT(String("Specified base class '%s' does not exist, using RefCounted") % Array::make(extends));
		} else {
			base_class = extends;
		}
	}

	sol::object class_name_obj = t["class_name"];
	if (class_name_obj.get_type() != sol::type::lua_nil) {
		class_name = StringName(to_variant(class_name_obj).operator String());
	}

	sol::object icon_obj = t["icon"];
	if (icon_obj.get_type() != sol::type::lua_nil) {
		icon_path = to_variant(icon_obj).operator String();
	}

	sol::object tool_obj = t["tool"];
	if (tool_obj.get_type() != sol::type::lua_nil) {
		is_tool = to_variant(tool_obj).booleanize();
	}

	// __signals: { signal_name = [{name=argname, type=VariantType|Class}, ...], ... }
	sol::object signals_obj = t["__signals"];
	if (signals_obj.get_type() == sol::type::table) {
		for (auto& [key, val] : signals_obj.as<sol::table>()) {
			if (key.get_type() != sol::type::string) continue;
			StringName sig_name = key.as<StringName>();
			LuaScriptSignal sig;
			sig.name = sig_name;
			if (val.get_type() == sol::type::table) {
				sol::table arg_table = val.as<sol::table>();
				// Flat array: [name1, type1, name2, type2, ...]
				for (int j = 1; arg_table.get<sol::object>(j).get_type() == sol::type::string; j += 2) {
					PropertyInfo pi;
					pi.name = arg_table.get<String>(j);
					sol::object type_val = arg_table.get<sol::object>(j + 1);
					if (auto vt = type_val.as<sol::optional<VariantType>>()) {
						pi.type = vt->get_type();
					} else if (auto cls = type_val.as<sol::optional<Class>>()) {
						pi.type = Variant::OBJECT;
						pi.class_name = cls->get_name();
					}
					sig.arguments.push_back(pi);
				}
			}
			signals.insert(sig_name, sig);
		}
	}

	// __exports: { prop_name = LuaScriptProperty | VariantType | Class, ... }
	sol::object exports_obj = t["__exports"];
	if (exports_obj.get_type() == sol::type::table) {
		for (auto& [key, val] : exports_obj.as<sol::table>()) {
			if (key.get_type() != sol::type::string) continue;
			StringName prop_name = key.as<StringName>();
			if (auto prop = val.as<sol::optional<LuaScriptProperty>>()) {
				prop->name = prop_name;
				properties.insert(prop_name, *prop);
			} else if (auto vt = val.as<sol::optional<VariantType>>()) {
				LuaScriptProperty prop;
				prop.name = prop_name;
				prop.type = vt->get_type();
				if (vt->has_type_hints()) {
					prop.hint = vt->get_property_hint();
					prop.hint_string = vt->get_property_hint_string();
				}
				prop.default_value = vt->construct_default();
				prop.usage |= PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
				properties.insert(prop_name, prop);
			} else if (auto cls = val.as<sol::optional<Class>>()) {
				LuaScriptProperty prop;
				prop.name = prop_name;
				prop.type = Variant::OBJECT;
				prop.class_name = cls->get_name();
				if (ClassDB::is_parent_class(prop.class_name, Node::get_class_static())) {
					prop.hint = PROPERTY_HINT_NODE_TYPE;
					prop.hint_string = prop.class_name;
				} else if (ClassDB::is_parent_class(prop.class_name, Resource::get_class_static())) {
					prop.hint = PROPERTY_HINT_RESOURCE_TYPE;
					prop.hint_string = prop.class_name;
				}
				prop.usage |= PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
				properties.insert(prop_name, prop);
			}
		}
	}

	// __methods: { method_name = { fn = function, rpc = nil|Dictionary }, ... }
	sol::object methods_obj = t["__methods"];
	if (methods_obj.get_type() == sol::type::table) {
		Dictionary rpc_dict;
		for (auto& [key, val] : methods_obj.as<sol::table>()) {
			if (key.get_type() != sol::type::string) continue;
			String name = key.as<String>();
			if (val.get_type() != sol::type::table) continue;

			sol::table entry = val.as<sol::table>();
			sol::optional<sol::protected_function> fn = entry["fn"];
			if (!fn) continue;

			LuaScriptMethod script_method(name, *fn);
			sol::object async_val = entry["async"];
			if (async_val.get_type() == sol::type::boolean && async_val.as<bool>()) {
				script_method.is_async = true;
			}
			sol::object override_val = entry["override"];
			if (override_val.get_type() == sol::type::boolean && override_val.as<bool>()) {
				script_method.is_override = true;
				// Cache argument types from ClassDB once at definition time.
				// Used by to_method_info() to report correct types to Godot, and
				// reserved for future typed fast-path in call_func (see README).
				TypedArray<Dictionary> method_list = ClassDBSingleton::get_singleton()->class_get_method_list(base_class, false);
				for (int i = 0; i < method_list.size(); i++) {
					Dictionary minfo = method_list[i];
					if (StringName(minfo["name"]) != StringName(name)) {
						continue;
					}
					Array args = minfo["args"];
					for (int j = 0; j < args.size(); j++) {
						Dictionary arg = args[j];
						PropertyInfo pinfo;
						pinfo.name = arg["name"];
						pinfo.type = (Variant::Type)(int64_t)arg["type"];
						pinfo.class_name = arg["class_name"];
						script_method.classdb_args.push_back(pinfo);
					}
					break;
				}
			}
			methods.insert(name, script_method);

			sol::object rpc_val = entry["rpc"];
			if (rpc_val.get_type() != sol::type::lua_nil) {
				rpc_dict[String(name)] = to_variant(rpc_val);
			}
		}
		if (!rpc_dict.is_empty()) {
			rpc_config = rpc_dict;
		}
	}
}

void LuaScriptMetadata::clear() {
	is_valid = false;
	is_tool = false;
	base_class = RefCounted::get_class_static();
	class_name = StringName();
	icon_path = String();
	rpc_config = Variant();
	properties.clear();
	signals.clear();
	methods.clear();
}


}
