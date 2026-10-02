import os


def exists(env):
    return True


def generate(env):
    python_bin = os.getenv("PYTHON_BIN", "python")
    # C++ code generation
    env.Command(
        [
            "src/generated/global_enums.hpp",
            "src/generated/utility_functions.hpp",
            "src/generated/package_searcher.h",
            "src/generated/variant_type_constants.hpp",
        ],
        [
            "tools/code_generation/generate_cpp_code.py",
            "src/luaopen/package_searcher.lua",
            "lib/godot-cpp/gdextension/extension_api.json",
            "lib/godot-cpp/gen/include/godot_cpp/variant/utility_functions.hpp",
        ],
        action=python_bin + " $SOURCE",
    )
    # Embedded Fennel global macros (src/fennel-macros/*.fnl → src/generated/fennel_macros.gen.hpp)
    env.Command(
        ["src/generated/fennel_macros.gen.hpp"],
        [
            "tools/code_generation/embed_fennel_macros.py",
            *env.Glob("src/fennel-macros/*.fnl"),
        ],
        action=python_bin + " $SOURCE",
    )

    # Lua API metadata file to use in Lua Language Server
    godot_lua_api = env.Command(
        [
            "addons/fennel-gdextension/lua_api_definitions/builtin_classes.lua",
            "addons/fennel-gdextension/lua_api_definitions/classes.lua",
            "addons/fennel-gdextension/lua_api_definitions/global_enums.lua",
            "addons/fennel-gdextension/lua_api_definitions/utility_functions.lua",
        ],
        [
            "tools/code_generation/generate_lua_godot_api.py",
            "lib/godot-cpp/gdextension/extension_api.json",
        ],
        action=python_bin + " $SOURCE",
    )
    env.Default(godot_lua_api)
    env.Alias("lua_api", godot_lua_api)
