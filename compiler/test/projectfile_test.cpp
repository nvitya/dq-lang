/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    projectfile_test.cpp
 * authors: Codex
 * created: 2026-08-10
 * brief:   Focused DQ project file parser tests
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "projectfile.h"

using namespace std;
namespace fs = std::filesystem;

static int failures = 0;

static void Expect(bool condition, const string & message)
{
  if (!condition)
  {
    cerr << "projectfile test failed: " << message << "\n";
    ++failures;
  }
}

static void WriteFile(const fs::path & filename, const string & text)
{
  fs::create_directories(filename.parent_path());
  ofstream output(filename, ios::binary);
  output << text;
}

static bool HasDiagnostic(const ODqProject & project, const string & id)
{
  return !project.diagnostics.empty() && (project.diagnostics.front().id == id);
}

static bool LoadProject(ODqProject & project, const fs::path & filename,
                        const vector<string> & command_line_package_paths = {})
{
  g_opt = OCompOptions();
  return project.Load(filename, command_line_package_paths);
}

int main()
{
  struct STargetExpectation
  {
    const char * name;
    const char * arch;
    const char * triple;
    const char * cpu;
    const char * features;
    const char * clang_fpu;
    const char * gcc_toolchain;
    const char * gcc_multilib;
    ETargetFloatAbi float_abi;
    uint8_t default_float_bits;
  };
  const STargetExpectation target_expectations[] = {
    {"arm_m0-bare", "arm_m0", "thumbv6m-none-eabi", "cortex-m0",
      "-fpregs", "", "arm-none-eabi", "thumb/v6-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m3-bare", "arm_m3", "thumbv7m-none-eabi", "cortex-m3",
      "-fpregs", "", "arm-none-eabi", "thumb/v7-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m4-bare", "arm_m4", "thumbv7em-none-eabi", "cortex-m4",
      "-fpregs", "", "arm-none-eabi", "thumb/v7e-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m4f-bare", "arm_m4f", "thumbv7em-none-eabihf", "cortex-m4",
      "+vfp4d16sp,-fp64,-d32", "fpv4-sp-d16", "arm-none-eabi", "thumb/v7e-m+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m33f-bare", "arm_m33f", "thumbv8m.main-none-eabihf", "cortex-m33",
      "+fp-armv8d16sp,-fp64,-d32", "fpv5-sp-d16", "arm-none-eabi", "thumb/v8-m.main+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m7f-bare", "arm_m7f", "thumbv7em-none-eabihf", "cortex-m7",
      "+fp-armv8d16sp,-fp64,-d32", "fpv5-sp-d16", "arm-none-eabi", "thumb/v7e-m+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m7fd-bare", "arm_m7fd", "thumbv7em-none-eabihf", "cortex-m7",
      "+fp-armv8d16,+fp64,-d32", "fpv5-d16", "arm-none-eabi", "thumb/v7e-m+dp/hard",
      TARGET_FLOAT_ABI_HARD, 64},
  };
  for (const STargetExpectation & expected : target_expectations)
  {
    OCompTarget target;
    string error;
    Expect(target.Configure(expected.name, error), string("configure target ") + expected.name);
    Expect(target.name == expected.name, string("target name ") + expected.name);
    Expect(target.arch == expected.arch, string("target architecture ") + expected.name);
    Expect(target.llvm_triple == expected.triple, string("target triple ") + expected.name);
    Expect(target.llvm_cpu == expected.cpu, string("target CPU ") + expected.name);
    Expect(target.llvm_features == expected.features, string("target features ") + expected.name);
    Expect(target.clang_fpu == expected.clang_fpu, string("Clang FPU ") + expected.name);
    Expect(target.gcc_toolchain == expected.gcc_toolchain,
           string("GCC toolchain ") + expected.name);
    Expect(target.gcc_multilib == expected.gcc_multilib, string("GCC multilib ") + expected.name);
    Expect(target.float_abi == expected.float_abi, string("target float ABI ") + expected.name);
    Expect(target.default_float_bits == expected.default_float_bits,
           string("default float width ") + expected.name);
    Expect(target.IsArm() && target.IsBare() && (target.pointer_size == 4),
           string("common ARM bare properties ") + expected.name);
  }

  OCompTarget wasm_wasi;
  string wasm_wasi_error;
  Expect(wasm_wasi.Configure("wasm32-wasi", wasm_wasi_error), "configure WASI target");
  Expect(wasm_wasi.arch == "wasm32" && wasm_wasi.platform_name == "wasi",
         "WASI architecture and platform");
  Expect(wasm_wasi.llvm_triple == "wasm32-unknown-wasi"
         && wasm_wasi.llvm_backend == "WebAssembly" && wasm_wasi.llvm_cpu == "generic",
         "WASI LLVM target metadata");
  Expect(wasm_wasi.pointer_size == 4 && wasm_wasi.default_float_bits == 64
         && wasm_wasi.static_relocation && wasm_wasi.IsWasi() && wasm_wasi.IsWasm(),
         "WASI representation and relocation");
  Expect(!wasm_wasi.exceptions_supported && !wasm_wasi.default_exceptions
         && wasm_wasi.default_dynstrings && !wasm_wasi.IsBare(),
         "WASI target defaults");

  OCompTarget wasm_bare;
  string wasm_bare_error;
  Expect(wasm_bare.Configure("wasm32-bare", wasm_bare_error), "configure bare WebAssembly target");
  Expect(wasm_bare.llvm_triple == "wasm32-unknown-unknown" && wasm_bare.IsWasm()
         && wasm_bare.IsBare() && wasm_bare.pointer_size == 4 && wasm_bare.static_relocation,
         "bare WebAssembly target metadata");

  OCompTarget rv32;
  string rv32_error;
  Expect(rv32.Configure("rv32imac-bare", rv32_error), "configure RV32IMAC target");
  Expect(rv32.arch == "rv32imac" && rv32.llvm_triple == "riscv32-unknown-elf"
         && rv32.llvm_cpu == "generic-rv32" && rv32.llvm_features == "+m,+a,+c,+zicsr",
         "RV32IMAC LLVM target metadata");
  Expect(rv32.clang_arch == "rv32imac_zicsr" && rv32.llvm_abi == "ilp32"
         && rv32.gcc_toolchain == "riscv-none-elf"
         && rv32.gcc_multilib == "rv32imac_zicsr"
         && rv32.IsRiscV() && rv32.IsBare() && rv32.pointer_size == 4
         && rv32.static_relocation,
         "RV32IMAC ABI and target properties");

  vector<OCompTarget> canonical_targets = OCompTarget::CanonicalTargets();
  Expect(canonical_targets.size() == 11, "canonical target count");
  OCompTarget canonical_host;
  canonical_host.ConfigureHost();
  Expect(canonical_targets[0].name == canonical_host.name, "host target is listed first");
  Expect(canonical_targets[8].name == "wasm32-wasi"
         && canonical_targets[9].name == "wasm32-bare"
         && canonical_targets[10].name == "rv32imac-bare",
         "new canonical target ordering");

  OCompOptions hosted_defaults;
  hosted_defaults.target.ConfigureHost();
  hosted_defaults.ApplyTargetDefaults();
  Expect(hosted_defaults.exceptions, "hosted exceptions default");
  Expect(hosted_defaults.dynstrings, "hosted dynamic strings default");

  OCompOptions wasi_defaults;
  string wasi_default_error;
  Expect(wasi_defaults.target.Configure("wasm32-wasi", wasi_default_error), "WASI default target");
  wasi_defaults.ApplyTargetDefaults();
  Expect(!wasi_defaults.exceptions && wasi_defaults.dynstrings,
         "WASI exception and dynamic-string defaults");
  Expect(wasi_defaults.ValidateTargetSettings(wasi_default_error),
         "WASI defaults satisfy target capabilities");
  wasi_defaults.SetExceptions(true);
  Expect(!wasi_defaults.ValidateTargetSettings(wasi_default_error)
         && (wasi_default_error.find("does not support exceptions") != string::npos),
         "WASI rejects explicit exceptions");

  OCompOptions bare_defaults;
  string bare_default_error;
  Expect(bare_defaults.target.Configure("arm_m0-bare", bare_default_error), "bare default target");
  bare_defaults.ApplyTargetDefaults();
  Expect(!bare_defaults.exceptions, "bare exceptions default");
  Expect(!bare_defaults.dynstrings, "bare dynamic strings default");
  bare_defaults.SetExceptions(true);
  bare_defaults.SetDynStrings(true);
  bare_defaults.ApplyTargetDefaults();
  Expect(bare_defaults.exceptions, "explicit bare exceptions override");
  Expect(bare_defaults.dynstrings, "explicit bare dynamic strings override");

  OCompOptions command_line_options;
  string command_line_error;
  Expect(command_line_options.target.Configure("arm_m0-bare", command_line_error),
         "command line target setup");
  command_line_options.SetExceptions(false);  // Simulate a project setting.
  command_line_options.SetDynStrings(false);  // Simulate a project setting.
  char opt_arg0[] = "dq-comp";
  char opt_arg1[] = "--exceptions";
  char opt_arg2[] = "-O3";
  char opt_arg3[] = "--lto=full";
  char opt_arg4[] = "--build-suffix";
  char opt_arg5[] = "test";
  char opt_arg6[] = "-DVALUE=-42";
  char opt_arg7[] = "main.dq";
  char opt_arg8[] = "--dynstrings";
  char opt_arg9[] = "--cpu-features=+no-movt";
  char opt_arg10[] = "--package-build-root";
  char opt_arg11[] = "package-cache";
  char * opt_argv[] = {opt_arg0, opt_arg1, opt_arg2, opt_arg3, opt_arg4, opt_arg5, opt_arg6,
                       opt_arg8, opt_arg9, opt_arg10, opt_arg11, opt_arg7};
  command_line_error = command_line_options.ProcessCommandLineOpts(12, opt_argv);
  Expect(command_line_error.empty(),
         "command line options should parse");
  Expect(command_line_options.exceptions && command_line_options.exceptions_explicit,
         "command line exceptions overrides project setting");
  Expect(command_line_options.dynstrings && command_line_options.dynstrings_explicit,
         "command line dynamic strings override project setting");
  Expect(command_line_options.optlevel == OPTLEVEL_O3, "command line optimization level");
  Expect(command_line_options.lto_mode == LTOMODE_FULL, "command line LTO mode");
  Expect(command_line_options.build_tag == "arm_m0-bare-test", "command line build tag suffix");
  Expect(command_line_options.package_build_root_dir == "package-cache",
         "command line package build root");
  command_line_options.target.AppendCpuFeatures(command_line_options.cpu_features);
  Expect(command_line_options.target.llvm_features == "-fpregs,+no-movt",
         "command line CPU features append to target defaults");
  Expect((command_line_options.cmdline_defines.size() == 1)
         && command_line_options.cmdline_defines[0].has_int_value
         && (command_line_options.cmdline_defines[0].int_value == -42),
         "command line define");

  OCompOptions size_options;
  size_options.target.ConfigureHost();
  char size_arg0[] = "dq-comp";
  char size_arg1[] = "-Os";
  char size_arg2[] = "-Oz";
  char size_arg3[] = "main.dq";
  char * size_argv[] = {size_arg0, size_arg1, size_arg2, size_arg3};
  string size_error = size_options.ProcessCommandLineOpts(4, size_argv);
  Expect(size_error.empty(), "size optimization options should parse");
  Expect(size_options.optlevel == OPTLEVEL_OZ, "last size optimization option wins");
  Expect(size_options.OptimizesForSize(), "minimum-size mode optimizes for size");
  Expect(string(size_options.OptimizationLevelName()) == "z", "minimum-size option name");

  OCompTarget feature_target;
  feature_target.llvm_features = "+base";
  feature_target.AppendCpuFeatures("+extra");
  Expect(feature_target.llvm_features == "+base,+extra", "CPU features add a separator");
  feature_target.AppendCpuFeatures(",+last");
  Expect(feature_target.llvm_features == "+base,+extra,+last", "CPU features avoid duplicate separators");

  fs::path root = fs::temp_directory_path() / "dq-projectfile-test";
  error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "packages" / "sdk" / "project");
  fs::create_directories(root / "packages" / "sdk" / "ld");

  WriteFile(root / "packages" / "sdk" / "project" / "common.dqproj", R"(
/* include-local paths use the fragment directory */
var SDK_LD = '../ld'
define FROM_INCLUDE = 17
linkerpath = '${SDK_LD}'
linkscript = '${SDK_LD}/board.ld'
)");
  WriteFile(root / "main.dq", "// parser fixture\n");
  WriteFile(root / "full.dqproj", R"(
// comments, includes, variables, functions, and semicolon-separated statements
packagepath = 'packages'
var SDK = PackagePath('sdk')
include '${SDK}/project/common.dqproj'
main = '${PROJECT_DIR}/main.dq'; output = '${THIS_DIR}/out.elf'
target = 'arm_m7f-bare'
cpu_features = '+no-movt'
exceptions = false
dynstrings = false
compiler_runtime = 'libgcc'
c_runtime = 'newlib-nano'
link = true
optlevel = 2
debuginfo = true
define FEATURE
define COUNT = -42
linkobject = '${SDK}/objects/startup.o'
linkoption = '--gc-sections'
)");

  ODqProject project;
  Expect(LoadProject(project, root / "full.dqproj"), "full project should parse");
  Expect(g_opt.project_main_filename == fs::absolute(root / "main.dq").lexically_normal().string(), "main path");
  Expect(g_opt.project_has_output
         && (g_opt.project_output_filename == fs::absolute(root / "out.elf").lexically_normal().string()),
         "output path");
  Expect(g_opt.target.name == "arm_m7f-bare", "target value");
  Expect(g_opt.cpu_features == "+no-movt", "project CPU features value");
  g_opt.target.AppendCpuFeatures(g_opt.cpu_features);
  Expect(g_opt.target.llvm_features == "+fp-armv8d16sp,-fp64,-d32,+no-movt",
         "project CPU features append to target defaults");
  Expect(!g_opt.exceptions && g_opt.exceptions_explicit, "exceptions value");
  Expect(!g_opt.dynstrings && g_opt.dynstrings_explicit, "dynamic strings value");
  Expect(g_opt.compiler_runtime == "libgcc", "compiler runtime value");
  Expect(g_opt.c_runtime == "newlib-nano", "C runtime value");
  Expect(g_opt.link_mode == DQC_LINK_FORCE, "link value");
  Expect(g_opt.optlevel == OPTLEVEL_O2, "optimization value");
  Expect(g_opt.dbg_info, "debug value");
  Expect(g_opt.cmdline_defines.size() == 3, "included and local defines");
  Expect(g_opt.package_paths.size() == 1, "project package path");
  Expect(g_opt.link_objects.size() == 1, "link object");
  Expect(g_opt.linker_args.size() == 3, "ordered linker arguments");
  if (g_opt.linker_args.size() == 3)
  {
    Expect(g_opt.linker_args[0].starts_with("--library-path="), "linker path translation");
    Expect(g_opt.linker_args[1].starts_with("--script="), "linker script translation");
    Expect(g_opt.linker_args[2] == "--gc-sections", "raw linker option");
  }

  fs::create_directories(root / "project-packages" / "selected");
  fs::create_directories(root / "cli-packages" / "selected");
  WriteFile(root / "package-precedence.dqproj", R"(
packagepath = 'project-packages'
var SELECTED = PackagePath('selected')
main = '${SELECTED}/main.dq'
)");
  Expect(LoadProject(project, root / "package-precedence.dqproj", {(root / "cli-packages").string()}),
         "package precedence project should parse");
  Expect(g_opt.project_main_filename
             == fs::absolute(root / "cli-packages" / "selected" / "main.dq").lexically_normal().string(),
         "command-line package root precedence");

  WriteFile(root / "duplicate.dqproj", "main='main.dq'\nmain='other.dq'\n");
  Expect(!LoadProject(project, root / "duplicate.dqproj") && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate scalar diagnostic");

  WriteFile(root / "duplicate-exceptions.dqproj",
            "main='main.dq'\nexceptions=true\nexceptions=false\n");
  Expect(!LoadProject(project, root / "duplicate-exceptions.dqproj")
             && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate exceptions diagnostic");

  WriteFile(root / "duplicate-dynstrings.dqproj",
            "main='main.dq'\ndynstrings=true\ndynstrings=false\n");
  Expect(!LoadProject(project, root / "duplicate-dynstrings.dqproj")
             && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate dynamic strings diagnostic");

  WriteFile(root / "duplicate-variable.dqproj", "var ROOT='one'\nvar ROOT='two'\nmain='main.dq'\n");
  Expect(!LoadProject(project, root / "duplicate-variable.dqproj") && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate variable diagnostic");

  WriteFile(root / "duplicate-define.dqproj", "main='main.dq'\ndefine SAME\ndefine SAME=2\n");
  Expect(!LoadProject(project, root / "duplicate-define.dqproj") && HasDiagnostic(project, "ProjectDuplicate"),
         "duplicate define diagnostic");

  WriteFile(root / "unknown-variable.dqproj", "main='${UNKNOWN}/main.dq'\n");
  Expect(!LoadProject(project, root / "unknown-variable.dqproj") && HasDiagnostic(project, "ProjectVariable"),
         "unknown variable diagnostic");

  WriteFile(root / "missing-package.dqproj", "var SDK=PackagePath('missing')\nmain='main.dq'\n");
  Expect(!LoadProject(project, root / "missing-package.dqproj") && HasDiagnostic(project, "ProjectPackage"),
         "missing package diagnostic");

  WriteFile(root / "missing-main.dqproj", "define FEATURE\n");
  Expect(!LoadProject(project, root / "missing-main.dqproj") && HasDiagnostic(project, "ProjectMain"),
         "missing main diagnostic");

  WriteFile(root / "unknown-property.dqproj", "main='main.dq'\nunknown='value'\n");
  Expect(!LoadProject(project, root / "unknown-property.dqproj") && HasDiagnostic(project, "ProjectProperty"),
         "unknown property diagnostic");

  WriteFile(root / "bad-optlevel.dqproj", "main='main.dq'\noptlevel=4\n");
  Expect(!LoadProject(project, root / "bad-optlevel.dqproj") && HasDiagnostic(project, "ProjectValue"),
         "invalid optimization diagnostic");

  WriteFile(root / "size-optlevel-s.dqproj", "main='main.dq'\noptlevel='s'\n");
  Expect(LoadProject(project, root / "size-optlevel-s.dqproj")
             && (g_opt.optlevel == OPTLEVEL_OS),
         "project size optimization level");

  WriteFile(root / "size-optlevel-z.dqproj", "main='main.dq'\noptlevel='z'\n");
  Expect(LoadProject(project, root / "size-optlevel-z.dqproj")
             && (g_opt.optlevel == OPTLEVEL_OZ),
         "project minimum-size optimization level");

  WriteFile(root / "quoted-numeric-optlevel.dqproj", "main='main.dq'\noptlevel='2'\n");
  Expect(LoadProject(project, root / "quoted-numeric-optlevel.dqproj")
             && (g_opt.optlevel == OPTLEVEL_O2),
         "quoted numeric project optimization level");

  WriteFile(root / "bad-compiler-runtime.dqproj",
            "main='main.dq'\ntarget='arm_m0-bare'\ncompiler_runtime='compiler-rt'\n");
  Expect(!LoadProject(project, root / "bad-compiler-runtime.dqproj")
             && HasDiagnostic(project, "ProjectValue"),
         "invalid compiler runtime diagnostic");

  WriteFile(root / "bad-c-runtime.dqproj",
            "main='main.dq'\ntarget='arm_m0-bare'\nc_runtime='newlib'\n");
  Expect(!LoadProject(project, root / "bad-c-runtime.dqproj")
             && HasDiagnostic(project, "ProjectValue"),
         "invalid C runtime diagnostic");

  OCompOptions runtime_options;
  string runtime_error;
  Expect(runtime_options.target.Configure("arm_m0-bare", runtime_error), "runtime default target");
  Expect(runtime_options.EffectiveCompilerRuntime() == "libgcc", "bare compiler runtime default");
  Expect(runtime_options.EffectiveCRuntime() == "newlib-nano", "bare C runtime default");
  Expect(runtime_options.ValidateRuntimeSettings(runtime_error), "bare runtime settings validation");

  runtime_options.target.ConfigureHost();
  Expect(runtime_options.SetCompilerRuntime("libgcc", runtime_error), "set hosted compiler runtime");
  Expect(!runtime_options.ValidateRuntimeSettings(runtime_error), "hosted runtime setting rejection");

  WriteFile(root / "trailing.dqproj", "main='main.dq' extra\n");
  Expect(!LoadProject(project, root / "trailing.dqproj") && HasDiagnostic(project, "ProjectTrailingToken"),
         "trailing token diagnostic");

  WriteFile(root / "bad-string.dqproj", "main='unterminated\n");
  Expect(!LoadProject(project, root / "bad-string.dqproj") && HasDiagnostic(project, "ProjectString"),
         "malformed string diagnostic");

  WriteFile(root / "bad-comment.dqproj", "/* unterminated\nmain='main.dq'\n");
  Expect(!LoadProject(project, root / "bad-comment.dqproj") && HasDiagnostic(project, "ProjectComment"),
         "malformed comment diagnostic");

  WriteFile(root / "cycle-a.dqproj", "include 'cycle-b.dqproj'\nmain='main.dq'\n");
  WriteFile(root / "cycle-b.dqproj", "include 'cycle-a.dqproj'\n");
  Expect(!LoadProject(project, root / "cycle-a.dqproj") && HasDiagnostic(project, "ProjectIncludeCycle"),
         "include cycle diagnostic");

  WriteFile(root / "unsupported.dqproj", "main='main.dq'\ncpu='cortex-m7'\n");
  Expect(!LoadProject(project, root / "unsupported.dqproj") && HasDiagnostic(project, "ProjectUnsupported"),
         "deferred target tuning diagnostic");

  WriteFile(root / "legacy.dqprj", "main='main.dq'\n");
  Expect(!LoadProject(project, root / "legacy.dqprj") && HasDiagnostic(project, "ProjectExtension"),
         "legacy extension diagnostic");

  fs::remove_all(root, ec);
  if (failures)
  {
    cerr << failures << " projectfile test(s) failed\n";
    return 1;
  }
  cout << "Project file tests passed.\n";
  return 0;
}
