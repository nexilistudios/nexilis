#     Copyright (C) 2026 Valtteri Viirret
#     This file is part of the Nexilis Project.
#
#     This file is free software: you can redistribute it and/or modify
#     it under the terms of the GNU Lesser General Public License as
#     published by the Free Software Foundation, either version 3 of the
#     License, or (at your option) any later version.
#
#     This file is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU Lesser General Public License for more details.
#
#     You should have received a copy of the GNU Lesser General Public License
#     along with this file.  If not, see <https://gnu.org>.

import os
import sys
import shutil
import platform
import argparse
import subprocess
import re
from pathlib import Path


def clean_output_dir(path):
    """Remove all plugin files"""
    path = Path(path)
    if not path.exists():
        return

    print(f"Cleaning {path}")
    for f in path.glob("**/*"):
        if f.is_file() and (
            f.suffix.lower() in (".dll", ".so", ".dylib", ".pdb", ".mdb")
            or ".so." in f.name
        ):
            f.unlink()
            print(f"Removed: {f}")


def build_native(nexilis_root, target_platform, build_type):
    """Build native library for specified platform"""
    build_dir = nexilis_root / f"build_{target_platform.lower()}"

    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(parents=True)

    print(f"\nBuilding for {target_platform}")
    print(f"Build directory: {build_dir}")
    os.chdir(build_dir)

    cmake_cmd = ["cmake", "..", f"-DCMAKE_BUILD_TYPE={build_type}"]
    if target_platform == "Windows":
        cmake_cmd.extend(["-DCMAKE_TOOLCHAIN_FILE=../mingw_toolchain.cmake"])

    print(f"CMake command: {' '.join(cmake_cmd)}")
    subprocess.run(cmake_cmd, check=True)
    subprocess.run(["make", "-j", str(os.cpu_count())], check=True)

    return build_dir


def boost_runtime_libraries(build_dir, native_libraries, target_platform):
    """Find dynamically linked Boost libraries needed beside the native binaries."""
    if target_platform == "Linux":
        dependencies = {}
        for library in native_libraries:
            result = subprocess.run(
                ["ldd", str(library)], check=True, capture_output=True, text=True
            )
            for line in result.stdout.splitlines():
                parts = line.strip().split()
                if not parts or not parts[0].startswith("libboost_"):
                    continue
                if len(parts) < 3 or parts[1] != "=>" or parts[2] == "not":
                    raise FileNotFoundError(
                        f"Boost dependency cannot be resolved: {line}"
                    )
                source = Path(parts[2])
                if not source.is_file():
                    raise FileNotFoundError(f"Boost dependency not found: {source}")
                dependencies[parts[0]] = source
        return dependencies

    if target_platform == "Windows":
        # The submodule build places DLLs in the build tree. System Boost DLLs
        # may instead be on PATH, so follow the imports reported by objdump.
        available = {
            path.name.lower(): path for path in build_dir.rglob("*.dll")
        }
        objdump = shutil.which("objdump")
        dumpbin = shutil.which("dumpbin") if not objdump else None
        dependencies = {}
        pending = list(native_libraries)
        while pending and (objdump or dumpbin):
            library = pending.pop()
            command = (
                [objdump, "-p", str(library)]
                if objdump
                else [dumpbin, "/DEPENDENTS", str(library)]
            )
            result = subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True,
            )
            imported = (
                re.findall(r"DLL Name:\s*(\S+)", result.stdout)
                if objdump
                else re.findall(r"^\s*(\S+\.dll)\s*$", result.stdout, re.I | re.M)
            )
            for name in imported:
                if "boost_" not in name.lower() or name.lower() in dependencies:
                    continue
                source = available.get(name.lower())
                if source is None:
                    source = next(
                        (
                            Path(folder) / name
                            for folder in os.get_exec_path()
                            if (Path(folder) / name).is_file()
                        ),
                        None,
                    )
                if source is None:
                    raise FileNotFoundError(f"Boost dependency not found: {name}")
                dependencies[name.lower()] = source
                pending.append(source)
        if not (objdump or dumpbin):
            dependencies = {
                name: path for name, path in available.items() if "boost_" in name
            }
        return {path.name: path for path in dependencies.values()}

    return {}


def deploy_native(output_path, build_dir, target_platform, arch):
    """Deploy build artifacts to output directory"""
    libs = (
        ["libnexilis.dll", "libnexilisc.dll"]
        if target_platform == "Windows"
        else ["libnexilis.so", "libnexilisc.so"]
    )

    target_dir = output_path / target_platform / arch
    target_dir.mkdir(parents=True, exist_ok=True)

    native_libraries = [build_dir / name for name in libs]
    for library in native_libraries:
        if not library.is_file():
            raise FileNotFoundError(f"Native library not found at {library}")

    runtime_libraries = boost_runtime_libraries(
        build_dir, native_libraries, target_platform
    )
    deployed = []
    for p in libs:
        source_path = build_dir / p

        target_path = target_dir / p

        if target_path.exists():
            target_path.unlink()

        shutil.copy2(source_path, target_path)

        if not target_path.exists():
            raise RuntimeError(f"Failed to copy library to {target_path}")

        print(f"Successfully deployed {p} to {target_path}")
        deployed.append(target_path)

    for name, source_path in runtime_libraries.items():
        target_path = target_dir / name
        shutil.copy2(source_path, target_path)
        print(f"Successfully deployed {name} to {target_path}")

    return deployed[0]


def main():
    parser = argparse.ArgumentParser(
        description="Build and deploy Nexilis native library and C# bindings."
    )
    parser.add_argument(
        "--input",
        required=True,
        type=str,
        help="Path to the Nexilis source root.",
    )
    parser.add_argument(
        "--output",
        required=True,
        type=str,
        help="Output directory for the built .dll and other artifacts.",
    )
    parser.add_argument("--clean", action="store_true", help="Full clean before build")
    parser.add_argument(
        "--arch",
        choices=["x86", "x86_64"],
        default="x86_64",
        help="Target architecture",
    )
    parser.add_argument(
        "--configuration",
        choices=["Debug", "Release"],
        default="Release",
        help="Build configuration (Debug/Release)",
    )
    args = parser.parse_args()

    original_dir = Path.cwd()
    current_os = platform.system()

    nexilis_root = Path(args.input).resolve()
    output_path = Path(args.output).resolve()

    if not nexilis_root.exists():
        print(f"Input path does not exist: {nexilis_root}")
        sys.exit(1)

    if args.clean:
        clean_output_dir(output_path)
        shutil.rmtree(nexilis_root / "build_windows", ignore_errors=True)
        shutil.rmtree(nexilis_root / "build_linux", ignore_errors=True)

    # Build native library.
    build_dir = build_native(nexilis_root, current_os, args.configuration)
    os.chdir(original_dir)

    # Deploy native library.
    try:
        native_lib_path = deploy_native(output_path, build_dir, current_os, args.arch)
        print(f"Native library deployed: {native_lib_path}")
    except Exception as e:
        print(f"Failed to deploy native library: {e}")
        sys.exit(1)

    # Build C# bindings.
    csharp_path = nexilis_root / "../bindings/csharp"
    os.chdir(csharp_path)
    print("\nBuilding C# bindings...")
    subprocess.run(
        ["dotnet", "build", "--configuration", args.configuration], check=True
    )
    os.chdir(original_dir)

    # Deploy C# bindings.
    csharp_dll = csharp_path / f"bin/{args.configuration}/netstandard2.0/Nexilis.dll"
    target_dll = output_path / "Nexilis.dll"

    if target_dll.exists():
        target_dll.unlink()
    shutil.copy2(csharp_dll, target_dll)

    if not target_dll.exists():
        print(f"Failed to deploy C# bindings to {target_dll}")
        sys.exit(1)

    print(f"Deployed C# bindings to: {target_dll}")


if __name__ == "__main__":
    main()
