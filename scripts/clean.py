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

from pre_commit.env import get_nexilis_root

import os
import shutil
import fnmatch


def read_gitignore(gitignore_path):
    """Read the .gitignore file and return a list of patterns."""
    patterns = []
    if os.path.isfile(gitignore_path):
        with open(gitignore_path, "r") as file:
            for line in file:
                line = line.strip()
                if line and not line.startswith(
                    "#"
                ):  # Ignore comments and empty lines.
                    patterns.append(line)
    return patterns


def matches_any_pattern(path, patterns):
    """Check if the given path matches any of the patterns in .gitignore."""
    for pattern in patterns:
        if fnmatch.fnmatch(path, pattern) or fnmatch.fnmatch(path, f"*/{pattern}"):
            return True
    return False


def delete_ignored_files(base_dir, patterns):
    """Delete files and directories listed in .gitignore."""
    for root, dirs, files in os.walk(base_dir, topdown=False):
        # Delete files
        for file in files:
            file_path = os.path.join(root, file)
            relative_path = os.path.relpath(file_path, base_dir)
            if matches_any_pattern(relative_path, patterns) and file != ".env":
                print(f"Deleting file: {file_path}")
                os.remove(file_path)

        # Delete directories
        for dir in dirs:
            dir_path = os.path.join(root, dir)
            relative_path = os.path.relpath(dir_path, base_dir)
            if matches_any_pattern(relative_path, patterns):
                print(f"Deleting directory: {dir_path}")
                shutil.rmtree(dir_path)


def prompt_for_env_deletion(base_dir):
    """Ask the user if they want to delete the .env file."""
    env_path = os.path.join(base_dir, ".env")
    if os.path.isfile(env_path):
        response = input("Do you want to delete the .env file? (y/n): ").strip().lower()
        if response == "y":
            print(f"Deleting .env file: {env_path}")
            os.remove(env_path)
        else:
            print(".env file was not deleted.")


def find_all_gitignore_files(base_dir):
    """Find all .gitignore files in the directory tree."""
    gitignore_files = []
    for root, dirs, files in os.walk(base_dir):
        # Skip .git directory
        if '.git' in root.split(os.sep):
            continue
        if '.gitignore' in files:
            gitignore_files.append(os.path.join(root, '.gitignore'))
    return gitignore_files


def main():
    base_dir = get_nexilis_root()

    # Collect patterns from all .gitignore files in the repository.
    all_patterns = []
    gitignore_files = find_all_gitignore_files(base_dir)
    for gitignore_path in gitignore_files:
        patterns = read_gitignore(gitignore_path)
        if patterns:
            print(f"Found {len(patterns)} patterns in {gitignore_path}")
            all_patterns.extend(patterns)

    if not all_patterns:
        print("No patterns found in any .gitignore files.")
        return

    # Delete files and directories matching the patterns.
    delete_ignored_files(base_dir, all_patterns)

    # Prompt the user to delete the .env file.
    prompt_for_env_deletion(base_dir)
    print("Cleanup complete.")


if __name__ == "__main__":
    main()
