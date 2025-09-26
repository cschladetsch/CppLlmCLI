#!/usr/bin/env python3
"""Build script for llm-repl C++ application."""

import os
import sys
import subprocess
import argparse
from pathlib import Path

def run_command(cmd, cwd=None):
    """Run a command and return success status."""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode == 0

def main():
    parser = argparse.ArgumentParser(description="Build llm-repl C++ application")
    parser.add_argument("--clean", action="store_true", help="Clean build directory first")
    parser.add_argument("--debug", action="store_true", help="Build in debug mode")
    parser.add_argument("--jobs", "-j", type=int, default=None, help="Number of parallel jobs")

    args = parser.parse_args()

    project_root = Path(__file__).parent
    build_dir = project_root / "build"

    # Clean if requested
    if args.clean and build_dir.exists():
        print("Cleaning build directory...")
        import shutil
        shutil.rmtree(build_dir)

    # Create build directory
    build_dir.mkdir(exist_ok=True)

    # Configure with CMake
    build_type = "Debug" if args.debug else "Release"
    cmake_cmd = [
        "cmake",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        str(project_root)
    ]

    if not run_command(cmake_cmd, cwd=build_dir):
        print("CMake configuration failed!")
        return 1

    # Build
    build_cmd = ["cmake", "--build", "."]
    if args.jobs:
        build_cmd.extend(["--parallel", str(args.jobs)])

    if not run_command(build_cmd, cwd=build_dir):
        print("Build failed!")
        return 1

    print(f"Build successful! Executable: {build_dir}/llm-repl")
    return 0

if __name__ == "__main__":
    sys.exit(main())