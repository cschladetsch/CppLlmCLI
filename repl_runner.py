#!/usr/bin/env python3
"""
LLM REPL Runner Script
Simple Python script to launch the C++20 LLM REPL application
"""

import subprocess
import sys
import os
from pathlib import Path
import argparse


class REPLRunner:
    def __init__(self):
        self.project_root = Path(__file__).parent
        self.executable = self.project_root / "build" / "llm-repl.exe"

    def check_executable(self):
        """Check if the executable exists"""
        if not self.executable.exists():
            print(f"[ERROR] Executable not found: {self.executable}")
            print("[INFO] Run 'python llm-repl.py build' to build the application first")
            return False
        return True

    def run_repl(self, args=None):
        """Run the REPL with optional arguments"""
        if not self.check_executable():
            return False

        cmd = [str(self.executable)]
        if args:
            cmd.extend(args)

        print(f"[STARTING] LLM REPL...")
        print(f"[EXEC] Executable: {self.executable}")
        if args:
            print(f"[ARGS] Arguments: {' '.join(args)}")
        print("-" * 60)

        try:
            # Run the REPL interactively
            process = subprocess.run(cmd, cwd=self.project_root)
            return process.returncode == 0
        except KeyboardInterrupt:
            print("\n[STOP] REPL interrupted by user")
            return True
        except Exception as e:
            print(f"[ERROR] Error running REPL: {e}")
            return False


def main():
    parser = argparse.ArgumentParser(
        description="Launch the C++20 LLM REPL application",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python repl_runner.py                                    # Run with default config
  python repl_runner.py --api-key sk-xxx --model llama-3.1-70b-versatile
  python repl_runner.py --config custom.yaml --verbose
  python repl_runner.py --provider groq --temperature 0.7
  python repl_runner.py --help-repl                       # Show REPL help
        """
    )

    # REPL-specific arguments
    parser.add_argument("--api-key", "-k", help="API key for LLM provider")
    parser.add_argument("--provider", "-p", choices=["groq", "together", "ollama"],
                       help="LLM provider")
    parser.add_argument("--model", "-m", help="Model to use")
    parser.add_argument("--config", "-c", help="Configuration file path")
    parser.add_argument("--temperature", "-t", type=float,
                       help="Temperature (0.0 - 2.0)")
    parser.add_argument("--max-tokens", type=int,
                       help="Maximum tokens to generate")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Enable verbose logging")
    parser.add_argument("--help-repl", action="store_true",
                       help="Show REPL application help")

    args = parser.parse_args()

    runner = REPLRunner()

    # Show REPL help if requested
    if args.help_repl:
        return runner.run_repl(["--help"])

    # Build command arguments
    repl_args = []

    if args.config:
        repl_args.extend(["--config", args.config])
    if args.provider:
        repl_args.extend(["--provider", args.provider])
    if args.model:
        repl_args.extend(["--model", args.model])
    if args.api_key:
        repl_args.extend(["--api-key", args.api_key])
    if args.temperature is not None:
        repl_args.extend(["--temperature", str(args.temperature)])
    if args.max_tokens:
        repl_args.extend(["--max-tokens", str(args.max_tokens)])
    if args.verbose:
        repl_args.append("--verbose")

    # Run the REPL
    success = runner.run_repl(repl_args if repl_args else None)

    if not success:
        print("\n[ERROR] REPL execution failed")
        return 1

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n[STOP] Script interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n[ERROR] Script error: {e}")
        sys.exit(1)