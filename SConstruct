#!/usr/bin/env python
import os
import sys

if "build_profile" not in ARGUMENTS:
    ARGUMENTS["build_profile"] = "build_profile.json"

env = SConscript("godot-cpp/SConstruct")

# --- Configuração do Projeto ---
libname = "godot_dualsense"
plugin_path = "demo/bin"

# --- Includes ---
env.Append(CPPPATH=[
    ".",
    "src/",
    "src/Public",
    "src/Private",
    "src/GamepadCore/Source/Public",
    "src/GamepadCore/Source/Private"
])

# --- Flags de Compilação (C++20 Obrigatório) ---
if env["platform"] == "windows":
    # Força C++20 no MSVC
    env.Append(CXXFLAGS=["/std:c++20"])
    env.Append(CPPDEFINES=["UNICODE", "_UNICODE"])
    env.Append(LIBS=["setupapi", "hid"])

elif env["platform"] == "linux":
    env.Append(CXXFLAGS=["-std=c++20"])
    env.ParseConfig("pkg-config --cflags --libs hidapi-hidraw")

# --- Busca Recursiva de Fontes ---
sources = []
for root, dirs, files in os.walk("src"):
    # Skip the GamepadCore Examples/ and Tests/ directories entirely — both are
    # reference code, not part of the shipped binary. Platform-specific
    # production code lives in src/{Public,Private}/Platforms/.
    #
    # Tests/ in particular ships a Tests/Common/Platform/windows/ subtree
    # whose lowercase "windows" segment does not match the case-sensitive
    # platform-name substring filter below, so without this exclusion those
    # test sources would be compiled into the addon's .so on every platform.
    parts = root.split(os.sep)
    if "Examples" in parts or "Tests" in parts:
        continue
    for file in files:
        if file.endswith(".cpp"):
            file_path = os.path.join(root, file)
            # Per-platform source filter
            if env["platform"] == "windows" and ("Linux" in file_path or "Mac" in file_path or "Android" in file_path):
                continue
            if env["platform"] == "linux" and ("Windows" in file_path or "Mac" in file_path or "Android" in file_path):
                continue
            sources.append(file_path)

# --- Compilação ---
library = env.SharedLibrary(
    target=os.path.join(plugin_path, env["platform"], libname),
    source=sources
)

Default(library)