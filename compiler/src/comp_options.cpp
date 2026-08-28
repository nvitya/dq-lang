/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    comp_options.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include "comp_options.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <print>
#include <set>

#include "executable_path.h"

OCompOptions  g_opt;

void OCompTarget::ConfigureHost()
{
  *this = OCompTarget();

#if defined(_WIN32) || defined(_WIN64)
  platform = TARGET_PLATFORM_WIN;
  platform_name = "win";
#else
  platform = TARGET_PLATFORM_LINUX;
  platform_name = "linux";
#endif

#if defined(__x86_64__) || defined(_M_X64)
  arch = "x86_64";
  pointer_size = 8;
  #if defined(_WIN32) || defined(_WIN64)
    name = "x86_64-win";
    llvm_triple = "x86_64-w64-windows-gnu";
  #else
    name = "x86_64-linux";
    llvm_triple = "x86_64-unknown-linux-gnu";
  #endif
#elif defined(__i386__) || defined(_M_IX86)
  arch = "x86";
  pointer_size = 4;
  #if defined(_WIN32) || defined(_WIN64)
    name = "x86-win";
    llvm_triple = "i686-w64-windows-gnu";
  #else
    name = "x86-linux";
    llvm_triple = "i386-unknown-linux-gnu";
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
  arch = "aarch64";
  pointer_size = 8;
  name = "aarch64-" + platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    llvm_triple = "aarch64-w64-windows-gnu";
  #else
    llvm_triple = "aarch64-unknown-linux-gnu";
  #endif
#elif defined(__arm__) || defined(_M_ARM)
  arch = "arm";
  pointer_size = 4;
  name = "arm-" + platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    llvm_triple = "armv7-w64-windows-gnu";
  #else
    llvm_triple = "arm-unknown-linux-gnueabihf";
  #endif
#elif defined(__riscv)
  #if __riscv_xlen == 64
    arch = "riscv64";
    pointer_size = 8;
    name = "riscv64-linux";
    llvm_triple = "riscv64-unknown-linux-gnu";
  #else
    arch = "riscv32";
    pointer_size = 4;
    name = "riscv32-linux";
    llvm_triple = "riscv32-unknown-linux-gnu";
  #endif
#endif
}

bool OCompTarget::Configure(const string & aname, string & rerror)
{
  ConfigureHost();
  if (aname.empty())
  {
    return true;
  }

  if ((aname == name)
      || (("x64-linux" == aname) && ("x86_64-linux" == name))
      || (("x64-win" == aname) && ("x86_64-win" == name)))
  {
    return true;
  }

  struct SArmBarePreset
  {
    const char * name;
    const char * arch;
    const char * triple;
    const char * cpu;
    const char * features;
    const char * clang_fpu;
    const char * gcc_multilib;
    ETargetFloatAbi float_abi;
    uint8_t default_float_bits;
  };

  static const SArmBarePreset arm_bare_presets[] = {
    {"arm_m0-bare", "arm_m0", "thumbv6m-none-eabi", "cortex-m0",
      "-fpregs", "", "thumb/v6-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m3-bare", "arm_m3", "thumbv7m-none-eabi", "cortex-m3",
      "-fpregs", "", "thumb/v7-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m4-bare", "arm_m4", "thumbv7em-none-eabi", "cortex-m4",
      "-fpregs", "", "thumb/v7e-m/nofp", TARGET_FLOAT_ABI_SOFT, 64},
    {"arm_m4f-bare", "arm_m4f", "thumbv7em-none-eabihf", "cortex-m4",
      "+vfp4d16sp,-fp64,-d32", "fpv4-sp-d16", "thumb/v7e-m+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m33f-bare", "arm_m33f", "thumbv8m.main-none-eabihf", "cortex-m33",
      "+fp-armv8d16sp,-fp64,-d32", "fpv5-sp-d16", "thumb/v8-m.main+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m7f-bare", "arm_m7f", "thumbv7em-none-eabihf", "cortex-m7",
      "+fp-armv8d16sp,-fp64,-d32", "fpv5-sp-d16", "thumb/v7e-m+fp/hard",
      TARGET_FLOAT_ABI_HARD, 32},
    {"arm_m7fd-bare", "arm_m7fd", "thumbv7em-none-eabihf", "cortex-m7",
      "+fp-armv8d16,+fp64,-d32", "fpv5-d16", "thumb/v7e-m+dp/hard",
      TARGET_FLOAT_ABI_HARD, 64},
  };

  for (const SArmBarePreset & preset : arm_bare_presets)
  {
    if (aname != preset.name) continue;

    *this = OCompTarget();
    name = preset.name;
    arch = preset.arch;
    platform_name = "bare";
    llvm_triple = preset.triple;
    llvm_cpu = preset.cpu;
    llvm_features = preset.features;
    clang_fpu = preset.clang_fpu;
    gcc_multilib = preset.gcc_multilib;
    llvm_backend = "ARM";
    pointer_size = 4;
    platform = TARGET_PLATFORM_BARE;
    float_abi = preset.float_abi;
    default_float_bits = preset.default_float_bits;
    return true;
  }

  rerror = "Unsupported target \"" + aname + "\"";
  return false;
}

bool OCompTarget::ConfigureFromCommandLine(int argc, char ** argv, string & rerror,
                                           const string & default_name)
{
  string command_line_target;

  for (int i = 1; i < argc; ++i)
  {
    string arg(argv[i]);
    string value;
    if (arg.starts_with("--target="))
    {
      value = arg.substr(9);
    }
    else if ("--target" == arg)
    {
      if (++i >= argc)
      {
        rerror = "Missing target name after --target";
        return false;
      }
      value = argv[i];
    }
    else
    {
      continue;
    }

    if (value.empty())
    {
      rerror = "Empty target name";
      return false;
    }
    if (!command_line_target.empty() && (command_line_target != value))
    {
      rerror = "Conflicting target options \"" + command_line_target + "\" and \"" + value + "\"";
      return false;
    }
    command_line_target = value;
  }

  return Configure(command_line_target.empty() ? default_name : command_line_target, rerror);
}

OCompOptions::OCompOptions()
{
  //
}

bool OCompOptions::IsValidDefineName(const string & name)
{
  if (name.empty()) return false;
  char c = name[0];
  if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_'))) return false;
  for (size_t i = 1; i < name.size(); ++i)
  {
    c = name[i];
    if (!(((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or (c == '_')
          or ((c >= '0') and (c <= '9')))) return false;
  }
  return true;
}

void OCompOptions::ParseModuleUseStack(const string & text, vector<string> & rstack)
{
  rstack.clear();
  size_t start = 0;
  while (start <= text.size())
  {
    size_t comma = text.find(',', start);
    string item = (comma == string::npos ? text.substr(start) : text.substr(start, comma - start));
    if (!item.empty()) rstack.push_back(item);
    if (comma == string::npos) break;
    start = comma + 1;
  }
}

bool OCompOptions::ParseDefineIntValue(const string & text, int64_t & rvalue)
{
  if (text.empty()) return false;
  size_t pos = 0;
  bool negative = false;
  if ((text[pos] == '+') or (text[pos] == '-'))
  {
    negative = (text[pos] == '-');
    ++pos;
  }
  if (pos >= text.size()) return false;

  uint64_t accum = 0;
  for (; pos < text.size(); ++pos)
  {
    char c = text[pos];
    if ((c < '0') or (c > '9')) return false;
    uint64_t digit = (c - '0');
    if (accum > ((numeric_limits<uint64_t>::max() - digit) / 10)) return false;
    accum = accum * 10 + digit;
  }
  if (negative)
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max()) + 1) return false;
    rvalue = (accum == uint64_t(numeric_limits<int64_t>::max()) + 1)
      ? numeric_limits<int64_t>::min() : -int64_t(accum);
  }
  else
  {
    if (accum > uint64_t(numeric_limits<int64_t>::max())) return false;
    rvalue = int64_t(accum);
  }
  return true;
}

bool OCompOptions::ParseDefineBoolValue(const string & text, bool & rvalue)
{
  if ("true" == text)
  {
    rvalue = true;
    return true;
  }
  if ("false" == text)
  {
    rvalue = false;
    return true;
  }
  return false;
}

bool OCompOptions::ParseLtoMode(const string & text, string & rerror)
{
  if (text.empty() || ("full" == text))
  {
    lto_mode = LTOMODE_FULL;
    return true;
  }
  if ("off" == text)
  {
    lto_mode = LTOMODE_OFF;
    return true;
  }
  rerror = ("thin" == text)
    ? "ThinLTO is not supported yet; use --lto=full or --lto=off"
    : "Unknown LTO mode \"" + text + "\"; use --lto=full or --lto=off";
  return false;
}

bool OCompOptions::ProcessCommandLineOpts(int argc, char ** argv, string & rerror)
{
  build_tag = target.name;
  string build_tag_suffix;
  bool cli_link_mode = false;
  set<string> cli_define_names;

  auto require_value = [&](int & index, const string & option, const char * value_name) -> const char *
  {
    if (++index < argc) return argv[index];
    rerror = "Missing " + string(value_name) + " after " + option;
    return nullptr;
  };
  auto select_link_mode = [&](ECompLinkMode mode, const string & option) -> bool
  {
    if (!cli_link_mode)
    {
      link_mode = mode;
      cli_link_mode = true;
      return true;
    }
    if (mode == link_mode) return true;
    string previous = (DQC_LINK_COMPILE_ONLY == link_mode ? "-c" : "--link");
    rerror = "Conflicting link mode options: " + previous + " and " + option;
    return false;
  };

  for (int i = 1; i < argc; ++i)
  {
    string v(argv[i]);
    if (v.empty() || ('-' != v[0])) continue;
    if      ("--version" == v)  print_version = true;
    else if (v.starts_with("--target=")) { /* configured before option processing */ }
    else if ("--target" == v)
    {
      if (!require_value(i, v, "target name")) return false;
    }
    else if ("--ifgen" == v)  ifgen = true;
    else if ("--ifdump" == v)  ifdump = true;
    else if ("--no-use-sys" == v)  no_use_sys = true;
    else if ("--exceptions" == v)  SetExceptions(true);
    else if ("--no-exceptions" == v)  SetExceptions(false);
    else if ("--regen-if-stale" == v)  regen_if_stale = true;
    else if ("--link" == v)
    {
      if (!select_link_mode(DQC_LINK_FORCE, v)) return false;
    }
    else if (v.starts_with("--linker-arg="))
    {
      string linker_arg = v.substr(13);
      if (linker_arg.empty())
      {
        rerror = "Empty linker argument";
        return false;
      }
      linker_args.push_back(linker_arg);
    }
    else if ("--lto" == v)
    {
      if (!ParseLtoMode("", rerror)) return false;
    }
    else if (v.starts_with("--lto="))
    {
      if (!ParseLtoMode(v.substr(6), rerror)) return false;
    }
    else if ("--ifstack" == v)
    {
      const char * value = require_value(i, v, "module stack");
      if (!value) return false;
      ParseModuleUseStack(value, module_use_stack);
    }
    else if ("--pkg-path" == v)
    {
      const char * value = require_value(i, v, "path");
      if (!value) return false;
      package_paths.push_back(value);
    }
    else if ("--build" == v)
    {
      const char * value = require_value(i, v, "build tag");
      if (!value) return false;
      build_tag = value;
      if (build_tag.empty())
      {
        rerror = "Empty build tag after --build";
        return false;
      }
    }
    else if ("--build-suffix" == v)
    {
      const char * value = require_value(i, v, "build tag suffix");
      if (!value) return false;
      build_tag_suffix = value;
      if (build_tag_suffix.empty())
      {
        rerror = "Empty build tag suffix after --build-suffix";
        return false;
      }
    }
    else if ("--build-root" == v)
    {
      const char * value = require_value(i, v, "path");
      if (!value) return false;
      build_root_dir = value;
    }
    else if ("--mod-root" == v)
    {
      const char * value = require_value(i, v, "path");
      if (!value) return false;
      module_root_dir = value;
    }
    else if ("--mod-name" == v)
    {
      const char * value = require_value(i, v, "module name");
      if (!value) return false;
      module_name = value;
    }
    else if (("-v" == v) || ("-v1" == v))  verblevel = VERBLEVEL_STATUS;
    else if (("-vv" == v) || ("-v2" == v))  verblevel = VERBLEVEL_INFO;
    else if (("-vvv" == v) || ("-v3" == v))  verblevel = VERBLEVEL_DEBUG;
    else if ("-v0" == v)  verblevel = VERBLEVEL_NONE;
    else if ("-g" == v)  dbg_info = true;
    else if ("-ir" == v)  ir_print = true;
    else if ("-c" == v)
    {
      if (!select_link_mode(DQC_LINK_COMPILE_ONLY, v)) return false;
    }
    else if ((v.size() > 2) and ('D' == v[1]))
    {
      string defspec = v.substr(2);
      string defname = defspec;
      string defvalue;
      size_t eqpos = defspec.find('=');
      if (eqpos != string::npos)
      {
        defname = defspec.substr(0, eqpos);
        defvalue = defspec.substr(eqpos + 1);
      }
      if (!IsValidDefineName(defname))
      {
        rerror = "Invalid command line define name: " + defname;
        return false;
      }
      OCmdLineDefine def;
      def.name = defname;
      if (eqpos != string::npos)
      {
        if (ParseDefineBoolValue(defvalue, def.bool_value)) def.has_bool_value = true;
        else if (ParseDefineIntValue(defvalue, def.int_value)) def.has_int_value = true;
        else
        {
          rerror = "Invalid command line define value: " + v;
          return false;
        }
      }
      if (cli_define_names.insert(def.name).second)
      {
        erase_if(cmdline_defines, [&](const OCmdLineDefine & existing) { return existing.name == def.name; });
      }
      cmdline_defines.push_back(def);
    }
    else if ("-O0" == v)  optlevel = 0;
    else if ("-O1" == v)  optlevel = 1;
    else if ("-O2" == v)  optlevel = 2;
    else if ("-O3" == v)  optlevel = 3;
    else if ("-o" == v)
    {
      if (!require_value(i, v, "filename")) return false;
    }
    else
    {
      rerror = "Unknown command line switch: " + v;
      return false;
    }
  }
  if (!build_tag_suffix.empty()) build_tag += "-" + build_tag_suffix;
  if (ifgen && ifdump)
  {
    rerror = "--ifgen and --ifdump can not be used together.";
    return false;
  }
  return true;
}

void OCompOptions::PrintUsage()
{
  print("Usage:\n");
  print("  dq-comp [options] <file.dq|file.dqproj>\n");
  print("Options:\n");
  print("  -o <file> : set output filename\n");
  print("  -c        : compile only (do not link)\n");
  print("  --link    : force linking even without a hosted Main function\n");
  print("  --linker-arg=<arg> : pass one argument directly to the linker (repeatable)\n");
  print("  --ifgen   : generate module interface file (.dqm_if)\n");
  print("  --ifdump  : dump module interface artifact (.dqm_if)\n");
  print("  --no-use-sys : do not add the implicit merged sys module\n");
  print("  --exceptions : enable exception handling\n");
  print("  --no-exceptions : disable exception handling\n");
  print("  --target=<name> : select the compiler target\n");
  print("  --pkg-path <path> : add a package search root (repeatable, last wins)\n");
  print("  --build <tag> : select .dqbuild build tag\n");
  print("  --build-suffix <suffix> : append to the selected .dqbuild build tag\n");
  print("  --lto[=full|off] : emit and link LLVM bitcode sidecars for full LTO\n");
  print("  --version : print compiler version\n");
  print("  -D<name>  : defines the <name> symbol with boolean true\n");
  print("  -D<name>=<value> : defines the <name> symbol with the <value> (int/bool)\n");
  print("  -On       : optimization level, n=0-3\n");
  print("  -g        : generate debug info\n");
  print("  -v,-v1    : print compile status messages\n");
  print("  -vv,-v2   : print detailed compiler information\n");
  print("  -vvv,-v3  : print compiler internal trace messages\n");
  print("  -v0       : no extra output (default)\n");
  print("  -ir       : print LLVM IR code\n");
}

bool OCompOptions::CommandLineOptionHasValue(const string & option)
{
  return (option == "--target") || (option == "--pkg-path") || (option == "--build")
         || (option == "--build-suffix") || (option == "--build-root")
         || (option == "--mod-root") || (option == "--mod-name") || (option == "--ifstack")
         || (option == "-o");
}

void OCompOptions::InitializeCompilerExecutable(const string &argv0)
{
  compiler_executable = CurrentExecutablePath(argv0, "dq-comp");
  filesystem::path path(compiler_executable);
  compiler_executable_dir = path.has_parent_path() ? path.parent_path().lexically_normal().string() : "";
}

vector<string> OCompOptions::DefaultPackagePaths() const
{
  vector<string> result;
  result.push_back("/usr/lib/dq/stdpkg");

  if (!compiler_executable_dir.empty())
  {
    filesystem::path executable_dir(compiler_executable_dir);
    result.push_back((executable_dir / ".." / "lib" / "dq" / "stdpkg").lexically_normal().string());
    result.push_back((executable_dir / ".." / "stdpkg").lexically_normal().string());
  }

  result.push_back("/usr/lib/dq/packages");
  if (!compiler_executable_dir.empty())
  {
    filesystem::path executable_dir(compiler_executable_dir);
    result.push_back((executable_dir / ".." / "lib" / "dq" / "packages").lexically_normal().string());
  }

  const char * user_home = getenv("HOME");
  if (user_home && user_home[0])
  {
    result.push_back((filesystem::path(user_home) / ".dq" / "packages").lexically_normal().string());
  }
  return result;
}

bool OCompOptions::SetCompilerRuntime(const string & value, string & rerror)
{
  if (("libgcc" != value) && ("none" != value))
  {
    rerror = "compiler_runtime must be 'libgcc' or 'none'";
    return false;
  }
  compiler_runtime = value;
  return true;
}

bool OCompOptions::SetCRuntime(const string & value, string & rerror)
{
  if (("newlib-nano" != value) && ("none" != value))
  {
    rerror = "c_runtime must be 'newlib-nano' or 'none'";
    return false;
  }
  c_runtime = value;
  return true;
}

bool OCompOptions::ValidateRuntimeSettings(string & rerror) const
{
  if (!target.IsBare() && (!compiler_runtime.empty() || !c_runtime.empty()))
  {
    rerror = "compiler_runtime and c_runtime are supported only for bare targets";
    return false;
  }
  return true;
}

string OCompOptions::EffectiveCompilerRuntime() const
{
  return (compiler_runtime.empty() && target.IsBare()) ? "libgcc" : compiler_runtime;
}

string OCompOptions::EffectiveCRuntime() const
{
  return (c_runtime.empty() && target.IsBare()) ? "newlib-nano" : c_runtime;
}

bool OCompOptions::ResolveGccMultilibDir(string & rpath, string & rerror) const
{
  if (target.gcc_multilib.empty())
  {
    rerror = "Target \"" + target.name + "\" has no GCC multilib mapping";
    return false;
  }

  vector<filesystem::path> roots;
  if (!compiler_executable_dir.empty())
  {
    filesystem::path bin_dir(compiler_executable_dir);
    filesystem::path install_root = (bin_dir / "..").lexically_normal();
    roots.push_back(install_root / "gcclibs");
    roots.push_back(install_root / "lib" / "dq" / "gcclibs");
  }
  roots.push_back("/usr/lib/dq/gcclibs");
#ifdef DQ_DEFAULT_GCCLIBS_DIR
  roots.push_back(DQ_DEFAULT_GCCLIBS_DIR);
#endif

  error_code ec;
  for (const filesystem::path & root : roots)
  {
    filesystem::path candidate = root / "arm-none-eabi" / "lib" / target.gcc_multilib;
    ec.clear();
    if (filesystem::is_directory(candidate, ec) && !ec)
    {
      rpath = candidate.lexically_normal().string();
      return true;
    }
  }

  rerror = "Can not find bundled GCC libraries for target \"" + target.name + "\"";
  return false;
}
