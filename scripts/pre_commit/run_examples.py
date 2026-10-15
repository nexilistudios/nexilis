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

"""Build every example and verify that it builds and runs correctly.

Each subdirectory of ``examples/`` that contains a ``CMakeLists.txt`` is
considered an example. Optional per-example behavior is read from an
``example.toml`` manifest inside the example directory::

    [run]
    role = "server"                    # "standalone" (default), "server", "client"
    wait_for = "/tmp/nexilis/stream"   # server role: path that must exist
    requires = "unix_stream_server"    # client role: provider example name
    startup_timeout = 15               # seconds (optional)
    run_timeout = 60                   # seconds (optional)

Verification rules:
    * standalone: exits 0 within ``run_timeout`` seconds.
    * server: becomes ready (``wait_for`` appears) within ``startup_timeout``
      seconds and stays alive while clients are verified.
    * client: exits 0 within ``run_timeout`` seconds while its required
      servers are running.
"""

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

from env import get_nexilis_root
from run_command import run_command

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None

DEFAULT_STARTUP_TIMEOUT = 30.0
DEFAULT_RUN_TIMEOUT = 60.0
POLL_INTERVAL = 0.1
TERMINATE_GRACE = 5.0


def _parse_simple_toml(text):
    """Minimal TOML subset parser used when tomllib is unavailable."""
    data = {}
    section = data
    for raw_line in text.splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            section = data.setdefault(line[1:-1].strip(), {})
            continue
        if "=" not in line:
            continue
        key, _, value = (part.strip() for part in line.partition("="))
        if value.startswith('"') and value.endswith('"'):
            parsed = value[1:-1]
        elif value.isdigit():
            parsed = int(value)
        else:
            parsed = value
        section[key] = parsed
    return data


def load_manifest(example_dir):
    manifest_path = example_dir / "example.toml"
    if not manifest_path.is_file():
        return {}
    text = manifest_path.read_text(encoding="utf-8")
    if tomllib is not None:
        return tomllib.loads(text)
    print(
        f"Warning: tomllib unavailable, using fallback parser for " f"{manifest_path}"
    )
    return _parse_simple_toml(text)


class Example:
    """A single example directory that can be built and run."""

    def __init__(self, directory):
        self.directory = Path(directory)
        self.name = self.directory.name
        self.manifest = load_manifest(self.directory)
        run_cfg = self.manifest.get("run", {})
        self.role = run_cfg.get("role", "standalone")
        self.wait_for = run_cfg.get("wait_for")
        self.requires = run_cfg.get("requires")
        self.startup_timeout = float(
            run_cfg.get("startup_timeout", DEFAULT_STARTUP_TIMEOUT)
        )
        self.run_timeout = float(run_cfg.get("run_timeout", DEFAULT_RUN_TIMEOUT))
        self.executable = None
        self.process = None
        self.log_file = None

    @property
    def build_dir(self):
        return self.directory / "build"

    def resolve_executable(self):
        cmake_lists = (self.directory / "CMakeLists.txt").read_text(encoding="utf-8")
        match = re.search(r"add_executable\s*\(\s*(\w+)", cmake_lists)
        if not match:
            raise RuntimeError(f"No add_executable target found for {self.name}")
        candidate = self.build_dir / match.group(1)
        if not candidate.is_file():
            raise RuntimeError(f"Executable {candidate} was not produced")
        self.executable = candidate
        return candidate

    def build(self, jobs):
        print(f"==> Building example: {self.name}")
        run_command("cmake -B build -S .", cwd=self.directory)
        run_command(
            f"cmake --build build --parallel {jobs}",
            cwd=self.directory,
        )
        self.resolve_executable()

    def start(self):
        """Start this example as a background server process."""
        # A wait_for path left behind by a previous run must not satisfy the
        # readiness check: the new server has not bound it yet, so clients that
        # start right away would connect before the server is listening. Remove
        # any stale socket file so readiness can only come from this instance.
        if self.wait_for:
            wait_path = Path(self.wait_for)
            if wait_path.exists():
                try:
                    if wait_path.is_socket():
                        wait_path.unlink()
                except OSError as error:
                    print(
                        f"Warning: could not remove stale wait_for "
                        f"'{self.wait_for}': {error}"
                    )
        self.log_file = open(self.build_dir / f"{self.name}_server.log", "w")
        print(f"==> Starting server example: {self.name}")
        self.process = subprocess.Popen(
            [str(self.executable), "--test-ports"],
            cwd=self.build_dir,
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
            text=True,
        )
        return self.wait_until_ready()

    def wait_until_ready(self):
        deadline = time.monotonic() + self.startup_timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                return False, (
                    f"Server '{self.name}' exited early with code "
                    f"{self.process.returncode}"
                )
            if self.wait_for is None or Path(self.wait_for).exists():
                return True, ""
            time.sleep(POLL_INTERVAL)
        self.terminate()
        return False, (
            f"Server '{self.name}' did not become ready within "
            f"{self.startup_timeout} seconds"
        )

    def run(self):
        """Run this example to completion and require exit code 0."""
        try:
            result = subprocess.run(
                [str(self.executable), "--test-ports"],
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=self.run_timeout,
            )
        except subprocess.TimeoutExpired:
            return False, (
                f"Example '{self.name}' did not exit within "
                f"{self.run_timeout} seconds"
            )
        if result.returncode != 0:
            output = (result.stdout or "") + (result.stderr or "")
            tail = "\n".join(output.splitlines()[-25:])
            return False, (
                f"Example '{self.name}' exited with code "
                f"{result.returncode}\n{tail}"
            )
        return True, ""

    def terminate(self):
        if self.process is None or self.process.poll() is not None:
            return
        self.process.terminate()
        try:
            self.process.wait(timeout=TERMINATE_GRACE)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()

    def close_log(self):
        if self.log_file:
            self.log_file.close()
            self.log_file = None


def discover_examples(nexilis_root):
    examples_dir = Path(nexilis_root) / "examples"
    if not examples_dir.is_dir():
        raise RuntimeError(f"Examples directory not found: {examples_dir}")
    return [
        Example(entry)
        for entry in sorted(examples_dir.iterdir())
        if entry.is_dir() and (entry / "CMakeLists.txt").is_file()
    ]


def group_by_role(examples):
    groups = {"standalone": [], "server": [], "client": []}
    for example in examples:
        if example.role not in groups:
            raise RuntimeError(f"Unknown role '{example.role}' for {example.name}")
        groups[example.role].append(example)
    return groups


def _command_output_tail(error):
    output = (getattr(error, "stdout", "") or "") + (getattr(error, "stderr", "") or "")
    lines = output.splitlines()
    if not lines:
        return ""
    return "\n".join(lines[-25:])


def verify_examples(examples):
    failures = []

    for example in examples:
        try:
            example.build(jobs=os.cpu_count() or 1)
            print(f"[OK] Built {example.name}")
        except subprocess.CalledProcessError as error:
            message = f"Build failed for '{example.name}': {error}"
            tail = _command_output_tail(error)
            if tail:
                message += "\n" + tail
            print(f"[FAIL] Build failed for '{example.name}'")
            failures.append(message)
        except (RuntimeError, ValueError) as error:
            print(f"[FAIL] Build failed for '{example.name}'")
            failures.append(f"Build failed for '{example.name}': {error}")

    if failures:
        return failures

    groups = group_by_role(examples)

    for example in groups["standalone"]:
        success, message = example.run()
        print(f"[{'PASS' if success else 'FAIL'}] {example.name}")
        if not success:
            failures.append(message)

    servers_started = []
    for example in groups["server"]:
        success, message = example.start()
        print(f"[{'PASS' if success else 'FAIL'}] {example.name} (server)")
        if success:
            servers_started.append(example)
        else:
            example.terminate()
            failures.append(message)

    try:
        for example in groups["client"]:
            missing = [
                name
                for name in [example.requires]
                if name and name not in [s.name for s in servers_started]
            ]
            if missing:
                failures.append(
                    f"Client '{example.name}' requires unknown server "
                    f"examples: {', '.join(missing)}"
                )
                continue

            dead = [
                s.name
                for s in servers_started
                if s.process is not None and s.process.poll() is not None
            ]
            if dead:
                failures.append(
                    f"Servers died before client '{example.name}' ran: "
                    f"{', '.join(dead)}"
                )
                continue

            success, message = example.run()
            print(f"[{'PASS' if success else 'FAIL'}] {example.name}")
            if not success:
                failures.append(message)
    finally:
        for example in servers_started:
            example.terminate()
            example.close_log()

    return failures


def run_examples_check(root=None):
    """Build and verify all examples.

    Returns a tuple of (success, output).
    """
    output = []

    try:
        nexilis_root = Path(root or get_nexilis_root())
        examples = discover_examples(nexilis_root)
    except RuntimeError as error:
        return False, str(error)

    if not examples:
        return False, "No examples found."

    output.append(
        f"Found {len(examples)} examples: {', '.join(e.name for e in examples)}"
    )

    failures = verify_examples(examples)

    if failures:
        output.append("=== Example checks failed ===")
        output.extend(failures)
        return False, "\n".join(output)

    output.append("=== All example checks passed ===")
    return True, "\n".join(output)


def main():
    parser = argparse.ArgumentParser(
        description="Build all examples and verify that they run correctly."
    )
    parser.add_argument(
        "--root",
        type=str,
        default=None,
        help="Nexilis root directory (defaults to NEXILIS_ROOT/.env).",
    )
    args = parser.parse_args()

    success, output = run_examples_check(root=args.root)
    print(output)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
