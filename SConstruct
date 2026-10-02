import os


# Lua GDExtension specific command line options
# These should be dealt with before initializing godot-cpp, to avoid unknown options warnings
lua_runtime = ARGUMENTS.pop("lua_runtime", "lua")
if lua_runtime.lower() not in ["lua", "luajit"]:
    raise ValueError(f"Invalid lua_runtime: expected either 'lua' or 'luajit', got {lua_runtime}")
vcvarsall_path = ARGUMENTS.pop("vcvarsall_path", "")

# We really don't need deprecated stuff
ARGUMENTS["deprecated"] = False

env = SConscript("lib/godot-cpp/SConstruct").Clone()
env["lua_runtime"] = lua_runtime
env["vcvarsall_path"] = vcvarsall_path
env.Tool("apple", toolpath=["tools"])
env.Tool("utils", toolpath=["tools"])

if env["platform"] == "web" and lua_runtime == "luajit":
    print("LuaJIT doesn't support WebAssembly, building with Lua instead")
    lua_runtime = "lua"

# Setup variant build dir for each setup
build_dir = f"build/{lua_runtime}.{env["suffix"][1:]}"
env["build_dir"] = build_dir
VariantDir(build_dir, "src", duplicate=False)

# Add support for generating compilation database files
env.Tool("compilation_db")
compiledb = env.CompilationDatabase("compile_commands.json")
env.Alias("compiledb", compiledb)

# Generate sources
env.Tool("code_generator", toolpath=["tools"])

# Lua GDExtension uses C++20 instead of C++17 from godot-cpp
if env.remove_options(env["CXXFLAGS"], "-std=c++17"):
    env.Append(CXXFLAGS="-std=c++20")
elif env.remove_options(env["CXXFLAGS"], "/std:c++17"):
    env.Append(CXXFLAGS="/std:c++20")

# Avoid stripping all symbols, we need `fennelgdextension_entrypoint` exported
env.remove_options(env["LINKFLAGS"], "-s")

# Build with exceptions enabled
env.remove_options(env["CXXFLAGS"], "-fno-exceptions")
if env["platform"] == "windows" and not env["use_mingw"]:
    env.Append(CXXFLAGS="/EHsc")

# Setup Lua / LuaJIT, Sol2 and Tree-sitter
if lua_runtime == "lua":
    env.Tool("lua", toolpath=["tools"])
elif lua_runtime == "luajit":
    env.Tool("luajit", toolpath=["tools"])
env.Tool("sol2", toolpath=["tools"])
env.Tool("tree_sitter", toolpath=["tools"])

# Build Lua GDExtension
source_directories = [".", "luaopen", "utils", "script-language"]
sources = [
    Glob(f"{build_dir}/{directory}/*.cpp")
    for directory in source_directories
]

# Generate documentation source file
if env["target"] in ["editor", "template_debug"]:
    doc_data = env.GodotCPPDocData("src/generated-document/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
    sources.append(doc_data)

if env["platform"] == "ios":
    library = env.StaticLibrary(
        f"{build_dir}/libfennelgdextension{env["suffix"]}{env["LIBSUFFIX"]}",
        source=sources,
    )
    godotcpp_xcframework = env.XCFramework(
        f"addons/fennel-gdextension/build/libgodot-cpp{env["suffix"]}.xcframework",
        [
            f"lib/godot-cpp/bin/libgodot-cpp{env["suffix"]}{env["LIBSUFFIX"]}",
            *map(str, Glob(f"lib/godot-cpp/bin/libgodot-cpp{env["suffix"]}*{env["LIBSUFFIX"]}")),
        ],
    )
    fennelgdextension_xcframework = env.XCFramework(
        f"addons/fennel-gdextension/build/libfennelgdextension{env["suffix"]}.xcframework",
        [
            f"{build_dir}/libfennelgdextension{env["suffix"]}{env["LIBSUFFIX"]}",
            *map(str, Glob(f"{build_dir}/libfennelgdextension{env["suffix"]}*{env["LIBSUFFIX"]}")),
        ],
    )
    env.Depends(godotcpp_xcframework, library)
    env.Depends(fennelgdextension_xcframework, godotcpp_xcframework)
    Default(fennelgdextension_xcframework)
else:
    library = env.SharedLibrary(
        f"addons/fennel-gdextension/build/libfennelgdextension{env["suffix"]}{env["SHLIBSUFFIX"]}",
        source=sources,
    )
    Default(library)


# Copy files to addons folder
addons_source = ["CHANGELOG.md", "LICENSE", "README.md"]
addons_files = env.Command(
    [f"addons/fennel-gdextension/{f}" for f in addons_source],
    addons_source,
    Copy("addons/fennel-gdextension", addons_source),
)
addons_files.extend(env.Command(
    "addons/fennel-gdextension/fennel.lua",
    "vendor/fennel.lua",
    Copy("addons/fennel-gdextension", "vendor/fennel.lua"),
))
addons_files.extend(env.Command(
    "addons/fennel-gdextension/json.lua",
    "vendor/json.lua",
    Copy("addons/fennel-gdextension", "vendor/json.lua"),
))
addons_files.extend(env.Command(
    "addons/fennel-gdextension/extension_api.json",
    "lib/godot-cpp/gdextension/extension_api.json",
    Copy("addons/fennel-gdextension", "lib/godot-cpp/gdextension/extension_api.json"),
))
addons_files.extend([
    env.File("addons/fennel-gdextension/godot_api_docs.fnl"),
    env.File("addons/fennel-gdextension/globals.lua"),
])
if lua_runtime == "luajit":
    jit_source = Glob("lib/luajit/src/jit/*.lua")
    addons_files.extend(env.Command(
        [f"addons/fennel-gdextension/build/jit/{f}" for f in jit_source],
        jit_source,
        Copy("addons/fennel-gdextension/build/jit", jit_source),
    ))
Default(addons_files)
Alias("addons_files", addons_files)
