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
#include "FennelScriptResourceFormatSaver.hpp"

#include "FennelScript.hpp"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

namespace luagdextension {

static Ref<FennelScriptResourceFormatSaver> fennel_saver;

Error FennelScriptResourceFormatSaver::_save(const Ref<Resource> &resource, const String &p_path, uint32_t flags) {
	Ref<FennelScript> fennel_script = resource;
	ERR_FAIL_COND_V(fennel_script.is_null(), ERR_INVALID_PARAMETER);

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::ModeFlags::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), FileAccess::get_open_error(), "Failed to save file at " + p_path);

	file->store_string(fennel_script->get_source_code());
	Error error = file->get_error();
	switch (error) {
		case OK:
		case ERR_FILE_EOF:
			return OK;
		default:
			return error;
	}
}

bool FennelScriptResourceFormatSaver::_recognize(const Ref<Resource> &resource) const {
	Ref<FennelScript> fennel_script = resource;
	return fennel_script.is_valid();
}

PackedStringArray FennelScriptResourceFormatSaver::_get_recognized_extensions(const Ref<Resource> &resource) const {
	PackedStringArray extensions;
	if (_recognize(resource)) {
		extensions.push_back("fnl");
	}
	return extensions;
}

void FennelScriptResourceFormatSaver::register_in_godot() {
	fennel_saver.instantiate();
	ResourceSaver::get_singleton()->add_resource_format_saver(fennel_saver);
}

void FennelScriptResourceFormatSaver::unregister_in_godot() {
	ResourceSaver::get_singleton()->remove_resource_format_saver(fennel_saver);
	fennel_saver.unref();
}

void FennelScriptResourceFormatSaver::_bind_methods() {}

}
