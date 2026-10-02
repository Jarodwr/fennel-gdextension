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
#include "FennelParser.hpp"

#include "LuaAST.hpp"
#include "utils/tree_sitter_fennel.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace luagdextension {

static const char *read_func(void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read) {
	String *s = (String *) payload;
	if (byte_index == 0) {
		*bytes_read = s->length() * sizeof(char32_t);
		return (const char *) s->ptr();
	}
	else {
		*bytes_read = 0;
		return nullptr;
	}
}

static uint32_t decode_func(const uint8_t *string, uint32_t length, int32_t *code_point) {
	const char32_t *utf32_string = (const char32_t *) string;
	*code_point = *utf32_string;
	return sizeof(char32_t);
}

static void ts_log(void *payload, TSLogType log_type, const char *buffer) {
	switch (log_type) {
		case TSLogTypeParse:
			UtilityFunctions::print("[FennelParser][Parse] ", buffer);
			break;
		case TSLogTypeLex:
			UtilityFunctions::print("[FennelParser][Lex] ", buffer);
			break;
	}
}

FennelParser::FennelParser()
	: parser(ts_parser_new()) {
	ts_parser_set_language(parser, tree_sitter_fennel());
}

FennelParser::~FennelParser() {
	ts_parser_delete(parser);
}

Ref<LuaAST> FennelParser::parse_code(const String &code) const {
	TSInput input = {
		(void *) &code,
		read_func,
		TSInputEncodingCustom,
		decode_func,
	};
	if (TSTree *tree = ts_parser_parse(parser, nullptr, input)) {
		return memnew(LuaAST(tree, code));
	}
	return nullptr;
}

bool FennelParser::get_debug_log() const {
	return ts_parser_logger(parser).log != nullptr;
}

void FennelParser::set_debug_log(bool enabled) {
	ts_parser_set_logger(parser, {
		nullptr,
		enabled ? ts_log : nullptr,
	});
}

void FennelParser::setup_tree_sitter_allocator() {
	// Allocator is shared with LuaParser — no-op if already set.
}

void FennelParser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_code", "code"), &FennelParser::parse_code);
	ClassDB::bind_method(D_METHOD("set_debug_log", "enabled"), &FennelParser::set_debug_log);
	ClassDB::bind_method(D_METHOD("get_debug_log"), &FennelParser::get_debug_log);
	ADD_PROPERTY(PropertyInfo(Variant::Type::BOOL, "debug_log"), "set_debug_log", "get_debug_log");
}

String FennelParser::_to_string() const {
	return String("[%s:%d]") % Array::make(get_class_static(), get_instance_id());
}

}
