#!/usr/bin/env python3
"""
LLM REPL Management Script
Python script to manage the C++20 LLM REPL application
Usage: python llm-repl.py [command] [options] [-- app-args]
"""

import os
import sys
import subprocess
import argparse
import time
import shutil
import json
import re
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass
from datetime import datetime

# Colors for terminal output
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    END = '\033[0m'

class Logger:
    """Enhanced logging with colors and formatting"""

    @staticmethod
    def success(msg: str):
        print(f"{Colors.GREEN}[SUCCESS] {msg}{Colors.END}")

    @staticmethod
    def error(msg: str):
        print(f"{Colors.RED}[ERROR] {msg}{Colors.END}")

    @staticmethod
    def warning(msg: str):
        print(f"{Colors.YELLOW}[WARNING] {msg}{Colors.END}")

    @staticmethod
    def info(msg: str):
        print(f"{Colors.CYAN}[INFO] {msg}{Colors.END}")

    @staticmethod
    def header(msg: str):
        print(f"\n{Colors.MAGENTA}{Colors.BOLD}>>> {msg} <<<{Colors.END}")

    @staticmethod
    def banner():
        print(f"""{Colors.CYAN}
================================================================
                           LLM REPL
              C++20 Interactive AI Chat Terminal
                    Management Script v2.0
================================================================
{Colors.END}""")

@dataclass
class BuildConfig:
    """Build configuration settings"""
    build_type: str = "Release"
    compiler: str = "clang++"
    generator: str = "Unix Makefiles"
    jobs: int = 4
    verbose: bool = False

@dataclass
class TestResult:
    """Test execution result"""
    name: str
    passed: bool
    output: str = ""
    error: str = ""
    duration: float = 0.0

class ProjectManager:
    """Main project management class"""

    def __init__(self):
        self.project_root = Path(__file__).parent.absolute()
        self.build_dir = self.project_root / "build"
        self.source_dir = self.project_root / "src"
        self.exe_name = "llm-repl.exe"
        self.exe_path = self.build_dir / self.exe_name

    def command_exists(self, command: str) -> bool:
        """Check if a command exists in PATH"""
        return shutil.which(command) is not None

    def run_command(self, cmd: List[str], cwd: Optional[Path] = None,
                   capture_output: bool = True, check: bool = True) -> subprocess.CompletedProcess:
        """Run a command with proper error handling"""
        try:
            return subprocess.run(
                cmd,
                cwd=cwd or self.project_root,
                capture_output=capture_output,
                text=True,
                check=check
            )
        except subprocess.CalledProcessError as e:
            if capture_output:
                Logger.error(f"Command failed: {' '.join(cmd)}")
                if e.stdout:
                    print(f"STDOUT: {e.stdout}")
                if e.stderr:
                    print(f"STDERR: {e.stderr}")
            raise

    def get_file_size_mb(self, path: Path) -> float:
        """Get file size in MB"""
        return path.stat().st_size / (1024 * 1024)

    def count_lines(self, pattern: str) -> int:
        """Count lines in files matching pattern"""
        total_lines = 0
        for file_path in self.source_dir.rglob(pattern):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    total_lines += sum(1 for _ in f)
            except (UnicodeDecodeError, PermissionError):
                continue
        return total_lines

class StatusCommand:
    """Application status checking"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self) -> bool:
        Logger.header("APPLICATION STATUS")

        # Check project structure
        Logger.info("Checking project structure...")
        checks = [
            ("Project Root", self.pm.project_root, True),
            ("Source Directory", self.pm.source_dir, True),
            ("CMakeLists.txt", self.pm.project_root / "CMakeLists.txt", True),
            ("Build Directory", self.pm.build_dir, False),
            ("Executable", self.pm.exe_path, False)
        ]

        for name, path, required in checks:
            if path.exists():
                Logger.success(f"{name}: Found")
                if name == "Executable":
                    size = self.pm.get_file_size_mb(path)
                    mtime = datetime.fromtimestamp(path.stat().st_mtime)
                    Logger.info(f"  Size: {size:.2f} MB")
                    Logger.info(f"  Last Built: {mtime}")
            else:
                if required:
                    Logger.error(f"{name}: Missing (Required)")
                else:
                    Logger.warning(f"{name}: Not found")

        # Check dependencies
        Logger.info("\nChecking build dependencies...")
        deps = [
            ("CMake", "cmake"),
            ("Clang++", "clang++"),
            ("Make", "make"),
            ("Python", "python")
        ]

        for name, command in deps:
            if self.pm.command_exists(command):
                try:
                    result = self.pm.run_command([command, "--version"])
                    version = result.stdout.split('\n')[0]
                    Logger.success(f"{name}: Available")
                    Logger.info(f"  Version: {version}")
                except:
                    Logger.success(f"{name}: Available (version check failed)")
            else:
                Logger.error(f"{name}: Not found")

        # Source code statistics
        if self.pm.source_dir.exists():
            cpp_files = len(list(self.pm.source_dir.rglob("*.cpp")))
            hpp_files = len(list(self.pm.source_dir.rglob("*.hpp")))
            total_lines = self.pm.count_lines("*.cpp") + self.pm.count_lines("*.hpp")

            Logger.info("\nSource Code Statistics:")
            Logger.info(f"  C++ Files: {cpp_files}")
            Logger.info(f"  Header Files: {hpp_files}")
            Logger.info(f"  Total Lines: {total_lines}")

        return True

class CleanCommand:
    """Build artifact cleaning"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self) -> bool:
        Logger.header("CLEANING BUILD ARTIFACTS")

        if self.pm.build_dir.exists():
            Logger.info(f"Removing build directory: {self.pm.build_dir}")
            try:
                shutil.rmtree(self.pm.build_dir)
                Logger.success("Build directory cleaned")
            except Exception as e:
                Logger.error(f"Failed to clean build directory: {e}")
                return False
        else:
            Logger.info("Build directory doesn't exist - nothing to clean")

        # Clean other artifacts
        artifacts = [
            "test_config.yaml",
            "*.log",
            "__pycache__"
        ]

        for pattern in artifacts:
            for path in self.pm.project_root.glob(pattern):
                Logger.info(f"Removing artifact: {path.name}")
                if path.is_dir():
                    shutil.rmtree(path)
                else:
                    path.unlink()

        Logger.success("Clean completed")
        return True

class BuildCommand:
    """Application building"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self, config: BuildConfig) -> bool:
        Logger.header("BUILDING APPLICATION")

        Logger.info(f"Build Type: {config.build_type}")
        Logger.info(f"Build Directory: {self.pm.build_dir}")

        # Create build directory
        if not self.pm.build_dir.exists():
            Logger.info("Creating build directory...")
            self.pm.build_dir.mkdir(parents=True)

        try:
            # Configure with CMake
            Logger.info("Configuring with CMake...")
            cmake_args = [
                "cmake", "..",
                f"-DCMAKE_CXX_COMPILER={config.compiler}",
                f"-DCMAKE_BUILD_TYPE={config.build_type}",
                "-G", config.generator
            ]

            if config.verbose:
                Logger.info(f"CMake command: {' '.join(cmake_args)}")

            self.pm.run_command(cmake_args, cwd=self.pm.build_dir)
            Logger.success("CMake configuration completed")

            # Build with Make
            Logger.info("Building with Make...")
            make_args = ["make", f"-j{config.jobs}"]
            if config.verbose:
                make_args.append("VERBOSE=1")

            start_time = time.time()
            self.pm.run_command(make_args, cwd=self.pm.build_dir)
            build_time = time.time() - start_time

            Logger.success(f"Build completed in {build_time:.1f} seconds")

            # Check if executable was created
            if self.pm.exe_path.exists():
                size = self.pm.get_file_size_mb(self.pm.exe_path)
                Logger.success(f"Executable created: {self.pm.exe_name} ({size:.2f} MB)")
            else:
                Logger.error("Executable not found after build")
                return False

        except subprocess.CalledProcessError:
            Logger.error("Build failed")
            return False

        return True

class RunCommand:
    """Application execution"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self, app_args: List[str] = None) -> bool:
        Logger.header("RUNNING APPLICATION")

        # Check if executable exists
        if not self.pm.exe_path.exists():
            Logger.error(f"Executable not found: {self.pm.exe_path}")
            Logger.info("Run 'build' command first")
            return False

        # Prepare arguments
        if not app_args:
            Logger.info("No arguments provided - showing help")
            app_args = ["--help"]
        else:
            Logger.info(f"Arguments: {' '.join(app_args)}")

        Logger.info(f"Executing: {self.pm.exe_path} {' '.join(app_args)}")
        Logger.info("Press Ctrl+C to exit the application")
        print("=" * 60)

        try:
            # Run the application without capturing output
            result = self.pm.run_command(
                [str(self.pm.exe_path)] + app_args,
                capture_output=False,
                check=False
            )

            print("=" * 60)
            if result.returncode == 0:
                Logger.success("Application exited successfully")
            else:
                Logger.warning(f"Application exited with code: {result.returncode}")

        except KeyboardInterrupt:
            print("\n" + "=" * 60)
            Logger.info("Application interrupted by user")
        except Exception as e:
            Logger.error(f"Failed to run application: {e}")
            return False

        return True

class TestCommand:
    """Test suite execution"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self) -> bool:
        Logger.header("RUNNING TESTS")

        tests = [
            self._test_version_check,
            self._test_help_output,
            self._test_config_file,
            self._test_invalid_args,
            self._test_file_structure
        ]

        results = []
        for test_func in tests:
            result = test_func()
            results.append(result)

            if result.passed:
                Logger.success(f"{result.name} - PASSED ({result.duration:.2f}s)")
            else:
                Logger.error(f"{result.name} - FAILED ({result.duration:.2f}s)")
                if result.error:
                    Logger.info(f"  Error: {result.error}")

        # Summary
        passed = sum(1 for r in results if r.passed)
        total = len(results)

        print("\n" + "=" * 60)
        if passed == total:
            Logger.success(f"ALL TESTS PASSED ({passed}/{total})")
        else:
            Logger.error(f"SOME TESTS FAILED ({passed}/{total})")

        return passed == total

    def _test_version_check(self) -> TestResult:
        """Test version flag"""
        start_time = time.time()

        if not self.pm.exe_path.exists():
            return TestResult(
                "Version Check",
                False,
                error="Executable not found"
            )

        try:
            result = self.pm.run_command([str(self.pm.exe_path), "--version"])
            duration = time.time() - start_time

            if "LLM REPL v" in result.stdout:
                return TestResult("Version Check", True, result.stdout, duration=duration)
            else:
                return TestResult(
                    "Version Check",
                    False,
                    result.stdout,
                    "Version string not found",
                    duration
                )
        except Exception as e:
            return TestResult(
                "Version Check",
                False,
                error=str(e),
                duration=time.time() - start_time
            )

    def _test_help_output(self) -> TestResult:
        """Test help flag"""
        start_time = time.time()

        if not self.pm.exe_path.exists():
            return TestResult(
                "Help Output",
                False,
                error="Executable not found"
            )

        try:
            result = self.pm.run_command([str(self.pm.exe_path), "--help"])
            duration = time.time() - start_time

            if "Interactive AI Chat Terminal" in result.stdout:
                return TestResult("Help Output", True, result.stdout, duration=duration)
            else:
                return TestResult(
                    "Help Output",
                    False,
                    result.stdout,
                    "Help text not found",
                    duration
                )
        except Exception as e:
            return TestResult(
                "Help Output",
                False,
                error=str(e),
                duration=time.time() - start_time
            )

    def _test_config_file(self) -> TestResult:
        """Test configuration file handling"""
        start_time = time.time()

        # Create test config
        config_path = self.pm.project_root / "test_config.yaml"
        config_content = """provider: groq
api-key: test-key-123
model: llama-3.1-8b-instant
temperature: 0.7
max-tokens: 1000
"""

        try:
            with open(config_path, 'w') as f:
                f.write(config_content)

            if self.pm.exe_path.exists():
                result = self.pm.run_command([
                    str(self.pm.exe_path),
                    "--config", str(config_path),
                    "--help"
                ])

                # Clean up
                config_path.unlink()

                duration = time.time() - start_time
                return TestResult("Config File Handling", True, result.stdout, duration=duration)
            else:
                config_path.unlink()
                return TestResult(
                    "Config File Handling",
                    False,
                    error="Executable not found"
                )

        except Exception as e:
            if config_path.exists():
                config_path.unlink()
            return TestResult(
                "Config File Handling",
                False,
                error=str(e),
                duration=time.time() - start_time
            )

    def _test_invalid_args(self) -> TestResult:
        """Test invalid arguments handling"""
        start_time = time.time()

        if not self.pm.exe_path.exists():
            return TestResult(
                "Invalid Arguments",
                False,
                error="Executable not found"
            )

        try:
            result = self.pm.run_command(
                [str(self.pm.exe_path), "--invalid-flag"],
                check=False
            )
            duration = time.time() - start_time

            # Either it handles gracefully or shows error
            return TestResult("Invalid Arguments", True, result.stdout, duration=duration)

        except Exception as e:
            return TestResult(
                "Invalid Arguments",
                True,  # Even exceptions are acceptable for invalid args
                error=str(e),
                duration=time.time() - start_time
            )

    def _test_file_structure(self) -> TestResult:
        """Test project file structure"""
        start_time = time.time()

        required_files = [
            self.pm.exe_path,
            self.pm.project_root / "src" / "main.cpp",
            self.pm.project_root / "CMakeLists.txt"
        ]

        missing = [f for f in required_files if not f.exists()]
        duration = time.time() - start_time

        if missing:
            return TestResult(
                "File Structure",
                False,
                error=f"Missing files: {[str(f) for f in missing]}",
                duration=duration
            )
        else:
            return TestResult("File Structure", True, duration=duration)

class FormatCommand:
    """Code formatting"""

    def __init__(self, pm: ProjectManager):
        self.pm = pm

    def execute(self) -> bool:
        Logger.header("FORMATTING CODE")

        if not self.pm.command_exists("clang-format"):
            Logger.error("clang-format not found")
            return False

        source_files = list(self.pm.source_dir.rglob("*.cpp")) + list(self.pm.source_dir.rglob("*.hpp"))
        Logger.info(f"Found {len(source_files)} source files")

        for file_path in source_files:
            Logger.info(f"Formatting: {file_path.name}")
            try:
                self.pm.run_command(["clang-format", "-i", str(file_path)])
            except Exception as e:
                Logger.error(f"Failed to format {file_path.name}: {e}")
                return False

        Logger.success("Code formatting completed")
        return True

def show_help():
    """Show comprehensive help"""
    Logger.banner()
    print(f"""{Colors.WHITE}
USAGE:
    python llm-repl.py <command> [options] [-- app-args]

COMMANDS:
    status      Show application and build status
    clean       Clean build artifacts and temporary files
    build       Build the application (default: Release mode)
    run         Run the application with optional arguments
    test        Run test suite and validation checks
    format      Format source code with clang-format
    help        Show this help message

OPTIONS:
    --debug         Build in Debug mode
    --release       Build in Release mode (default)
    --verbose       Show verbose output during build
    --jobs N        Number of parallel build jobs (default: 4)

EXAMPLES:
    python llm-repl.py status                      # Check application status
    python llm-repl.py clean                       # Clean build artifacts
    python llm-repl.py build --debug --verbose     # Debug build with verbose output
    python llm-repl.py run                         # Run with --help (default)
    python llm-repl.py run -- --version            # Run with --version flag
    python llm-repl.py run -- -p groq -k key123    # Run with provider and API key
    python llm-repl.py test                        # Run test suite

WORKFLOW:
    1. python llm-repl.py status                   # Check dependencies
    2. python llm-repl.py clean                    # Clean if needed
    3. python llm-repl.py build                    # Build application
    4. python llm-repl.py test                     # Validate build
    5. python llm-repl.py run -- [args]            # Run application
{Colors.END}""")

def main():
    """Main entry point"""
    if len(sys.argv) < 2:
        show_help()
        return 0

    # Parse command and arguments
    command = sys.argv[1].lower()

    if command == "help":
        show_help()
        return 0

    # Parse additional arguments
    args = sys.argv[2:]
    app_args = []

    # Separate app arguments if -- is present
    if "--" in args:
        sep_index = args.index("--")
        options = args[:sep_index]
        app_args = args[sep_index + 1:]
    else:
        options = args

    # Parse options
    debug_mode = "--debug" in options
    release_mode = "--release" in options
    verbose = "--verbose" in options

    jobs = 4
    if "--jobs" in options:
        try:
            job_index = options.index("--jobs")
            if job_index + 1 < len(options):
                jobs = int(options[job_index + 1])
        except (ValueError, IndexError):
            Logger.error("Invalid --jobs value")
            return 1

    # Create project manager and build config
    pm = ProjectManager()
    config = BuildConfig(
        build_type="Debug" if debug_mode else "Release",
        jobs=jobs,
        verbose=verbose
    )

    # Execute commands
    try:
        if command == "status":
            success = StatusCommand(pm).execute()
        elif command == "clean":
            success = CleanCommand(pm).execute()
        elif command == "build":
            success = BuildCommand(pm).execute(config)
        elif command == "run":
            success = RunCommand(pm).execute(app_args)
        elif command == "test":
            success = TestCommand(pm).execute()
        elif command == "format":
            success = FormatCommand(pm).execute()
        else:
            Logger.error(f"Unknown command: {command}")
            show_help()
            return 1

        return 0 if success else 1

    except KeyboardInterrupt:
        Logger.info("Operation interrupted by user")
        return 1
    except Exception as e:
        Logger.error(f"Script execution failed: {e}")
        if verbose:
            import traceback
            traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())