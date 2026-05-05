#pragma once
#include <string>

const std::string DEFAULT_BLV_CONTENT = R"blv(
# ─────────────────────────────────────────────────────────
# Luv Default Orchestration Script (MASSIVE VERSION)
# ─────────────────────────────────────────────────────────

# ── 1. Setup Environment ──
let host_info = @host()
let target_info = @target()
let triple = target_info.triple

# ── 2. Parse CLI Arguments ──
if (@cli.has("help")) {
    @println("\e[1;35m  █    █  █ █   █")
    @println("  █    █  █  █ █ ")
    @println("  █▄▄▄ ▀▄▄▀   █  \e[0m")
    @println("\e[1;36m  Luv Build System (BLV Engine)\e[0m")
    @println("  ──────────────────────────────────")
    @println("  Usage: luv build [options] <src>")
    @println("")
    @println("\e[1;33m  Build Options:\e[0m")
    @println("    --app=<name>       Name of the output binary (default: luv_app)")
    @println("    --target=<triple>  Target triple (e.g., x86_64-apple-darwin)")
    @println("    --mode=debug|rel   Build mode (default: debug)")
    @println("    --lib              Build as a library instead of executable")
    @println("    --static           Prefer static linking")
    @println("    --o=<dir>          Output directory (default: build)")
    @println("    --D=<flags>        Extra defines for luvc (comma separated)")
    @println("    --X=<flags>        Extra excludes for luvc (comma separated)")
    @println("    --verbose          Print more info")
    @println("")
    @println("\e[1;33m  Diagnostic Flags:\e[0m")
    @println("    --lex, --parse, --ir, --asm, --dep-graph")
    @println("")
    @exit(0)
}

let app_name = @cli.get("app")
if (app_name == "") { let app_name = "luv_app" }

let mode = @cli.get("mode")
if (mode == "") { let mode = "debug" }

let is_lib = @cli.has("lib")
let is_static = @cli.has("static")
let is_verbose = @cli.has("verbose")

let out_dir = @cli.get("o")
if (out_dir == "") { let out_dir = "build" }

let defines = @cli.get("D")
let excludes = @cli.get("X")

let src_path = @cli.get("src")
if (src_path == "") {
    @println("\e[1;31m[!] Error:\e[0m No source file specified for build.")
    @println("    Use --src=<file> or pass the file as a positional argument.")
    @exit(1)
}

if (is_verbose) {
    let type_str = "Executable"
    if (is_lib) { let type_str = "Library" }
    let link_str = "Dynamic"
    if (is_static) { let link_str = "Static" }

    @println("\e[1;34m[*] Configuration Detail:\e[0m")
    @println("    - Host:    ", host_info.name)
    @println("    - Target:  ", triple)
    @println("    - Mode:    ", mode)
    @println("    - Type:    ", type_str)
    @println("    - Link:    ", link_str)
    @println("    - App:     ", app_name)
    @println("    - Source:  ", src_path)
    @println("    - Output:  ", out_dir)
}

# ── 3. Build Source ──
let target_flag = "-target=" + triple
let out_flag = "-o=" + out_dir
let build_flags = target_flag + " " + out_flag
if (defines != "") { let build_flags = build_flags + " -D=" + defines }
if (excludes != "") { let build_flags = build_flags + " -exclude=" + excludes }

@println("\e[1;34m[*] Compiling Luv modules...\e[0m")
let obj = @build(build_flags, src_path)

# ── 4. Linking ──
let output_path = out_dir + "/" + app_name

# Detect platform nuances
let is_windows = (triple == "x86_64-pc-windows-msvc" || triple == "x86_64-w64-mingw32" || triple == "i686-w64-mingw32")
let is_mac = (triple == "x86_64-apple-darwin" || triple == "aarch64-apple-darwin")

if (is_windows) {
    if (is_lib) { let output_path = output_path + ".lib" }
    else { let output_path = output_path + ".exe" }
} else {
    if (is_lib) {
        if (is_mac) { let output_path = output_path + ".dylib" }
        else { let output_path = output_path + ".so" }
    }
}

@println("\e[1;34m[*] Linking artifact: \e[0m", output_path)

# Independent linking logic
let link_cmd_flag = "-cmd=ld"
let link_out_flag = "-o=" + output_path

if (is_windows) {
    @link(link_cmd_flag, link_out_flag, obj, "-lkernel32", "-luser32", "-lshell32")
} else {
    if (is_mac) {
        @link(link_cmd_flag, link_out_flag, obj, "-lc", "-lSystem")
    } else {
        # Linux / Unix
        let base_link_flags = "-lc"
        if (is_static) { let base_link_flags = "-lc -static" }
        @link(link_cmd_flag, link_out_flag, obj, base_link_flags, "-lm", "-lpthread", "-no-pie")
    }
}

@println("\e[1;32m[+] Build sequence complete.\e[0m")
)blv";
