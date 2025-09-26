#!/usr/bin/env python3
"""
LLM REPL Project Validation Script
Comprehensive validation workflow for the entire project
"""

import subprocess
import sys
import time
from pathlib import Path

def run_validation():
    """Run comprehensive project validation"""

    print("""
========================================================================
                     LLM REPL PROJECT VALIDATION
========================================================================
Running comprehensive validation of the C++20 LLM REPL project...
""")

    validation_steps = [
        ("Project Status Check", ["python", "llm-repl.py", "status"]),
        ("Clean Build Environment", ["python", "llm-repl.py", "clean"]),
        ("Full Release Build", ["python", "llm-repl.py", "build", "--release"]),
        ("Comprehensive Testing", ["python", "llm-repl.py", "test"]),
        ("Code Formatting Check", ["python", "llm-repl.py", "format"]),
        ("Application Version Test", ["python", "llm-repl.py", "run", "--", "--version"]),
        ("Application Help Test", ["python", "llm-repl.py", "run", "--", "--help"])
    ]

    results = []
    total_time = time.time()

    for step_name, command in validation_steps:
        print(f"\n[STEP] {step_name}")
        print(f"[CMD]  {' '.join(command)}")
        print("-" * 60)

        start_time = time.time()
        try:
            result = subprocess.run(command, check=True, capture_output=False)
            duration = time.time() - start_time
            results.append((step_name, True, duration))
            print(f"[SUCCESS] {step_name} completed in {duration:.1f}s")
        except subprocess.CalledProcessError as e:
            duration = time.time() - start_time
            results.append((step_name, False, duration))
            print(f"[FAILED] {step_name} failed after {duration:.1f}s")
            print(f"[ERROR] Exit code: {e.returncode}")

    total_duration = time.time() - total_time

    # Generate summary report
    print(f"""
========================================================================
                        VALIDATION SUMMARY
========================================================================
""")

    passed = sum(1 for _, success, _ in results if success)
    total = len(results)

    for step_name, success, duration in results:
        status = "[PASS]" if success else "[FAIL]"
        print(f"{status} {step_name:<30} ({duration:.1f}s)")

    print(f"""
------------------------------------------------------------------------
OVERALL RESULT: {'SUCCESS' if passed == total else 'FAILURE'}
Tests Passed: {passed}/{total}
Total Time: {total_duration:.1f}s
------------------------------------------------------------------------

PROJECT STATUS: {'✅ FULLY VALIDATED' if passed == total else '❌ VALIDATION FAILED'}
""")

    if passed == total:
        print("""
🎉 CONGRATULATIONS! 🎉

The LLM REPL C++20 project has been successfully validated!

✅ All components built successfully
✅ All tests passed
✅ Code formatting applied
✅ Application functionality verified

The project is ready for production use!
""")
        return 0
    else:
        print("""
⚠️  VALIDATION INCOMPLETE

Some validation steps failed. Please review the errors above
and fix any issues before proceeding.
""")
        return 1

if __name__ == "__main__":
    try:
        sys.exit(run_validation())
    except KeyboardInterrupt:
        print("\n[INTERRUPTED] Validation cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n[ERROR] Validation script failed: {e}")
        sys.exit(1)