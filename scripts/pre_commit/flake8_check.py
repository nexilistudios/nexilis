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

"""Run the flake8 linter over the project's Python scripts."""

import os
import shutil
import subprocess
import sys
import argparse


def run_flake8(nexilis_root: str) -> bool:
    """Run flake8 on the project's scripts directory.

    Args:
        nexilis_root: The project root directory.

    Returns:
        True if linting passes, False otherwise.
    """
    scripts_dir = os.path.join(nexilis_root, "scripts")

    if shutil.which("flake8") is None:
        print(
            "flake8 is not installed. Install it with: pip install 'scripts[dev]'",
            file=sys.stderr,
        )
        return False

    print(f"Linting Python scripts in: {scripts_dir}")

    # Run from the scripts directory so that flake8 picks up the .flake8
    # config file and reports paths relative to it.
    result = subprocess.run(
        ["flake8", "--jobs=1", "."],
        cwd=scripts_dir,
        capture_output=True,
        text=True,
    )

    output = result.stdout.strip()
    if output:
        print(output)
    if result.stderr.strip():
        print(result.stderr.strip(), file=sys.stderr)

    return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(
        description="Run the flake8 linter over the project's Python scripts."
    )
    parser.add_argument(
        "--root",
        type=str,
        default=None,
        help="Project root (default: auto-detected from this file's location)",
    )
    args = parser.parse_args()

    if args.root:
        root = os.path.abspath(args.root)
    else:
        root = os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        )

    passed = run_flake8(root)
    print("Flake8 check passed." if passed else "Flake8 check failed.")
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
