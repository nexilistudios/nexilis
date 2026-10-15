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

from env import get_nexilis_root

import argparse
import os
import re
import shutil
import subprocess
import sys
from typing import Optional


def get_clang_format() -> Optional[str]:
    for name in ("clang-format-23", "clang-format"):
        executable = shutil.which(name)
        if executable is None:
            continue
        result = subprocess.run(
            [executable, "--version"], capture_output=True, text=True
        )
        if result.returncode == 0 and re.search(
            r"\bversion 23\.1\.1\b", result.stdout
        ):
            return executable

    print("Clang-format 23.1.1 is required (install clang-format==23.1.1).")
    return None


def format_all_files(apply_only: bool = False) -> int:
    clang_format = get_clang_format()
    if clang_format is None:
        return 1

    # Define the directories to run Clang-Format recursively.
    directories = ["nexilis", "examples", "tests"]
    changes_detected = False

    # Run Clang-Format recursively in the specified directories.
    for directory in directories:
        target_dir = os.path.join(get_nexilis_root(), directory)
        if not os.path.isdir(target_dir):
            continue

        print(f"Formatting files in directory: {target_dir}")

        # Check if files need formatting.
        find_check_command = (
            rf'find "{target_dir}"'
            r' -not -path "*/third-party/*"'
            r' -type f \( -name "*.hh" -o -name "*.cc" \)'
            rf' -exec "{clang_format}" --dry-run --Werror {{}} +'
        )
        result = subprocess.run(
            find_check_command, shell=True, capture_output=True, text=True
        )

        if result.returncode != 0:
            if not apply_only:
                print("::error::Formatting changes required. Diff:")
            else:
                print("Formatting changes needed:")
            print(result.stderr)
            changes_detected = True

            # Actually apply formatting.
            find_apply_command = (
                rf'find "{target_dir}"'
                r' -not -path "*/third-party/*"'
                r' -type f \( -name "*.hh" -o -name "*.cc" \)'
                rf' -exec "{clang_format}" -i {{}} +'
            )
            subprocess.run(find_apply_command, shell=True, check=True)

    if changes_detected:
        if not apply_only:
            print(
                "::error::Formatting changes were required "
                "and have been applied"
            )
            return 1
        print("Formatting changes applied")
        return 0
    else:
        print("No formatting changes needed")
        return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Apply formatting and exit successfully",
    )
    args = parser.parse_args()
    f = format_all_files(apply_only=args.apply)
    sys.exit(f)
