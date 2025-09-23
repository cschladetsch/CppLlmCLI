# LLM REPL Management Script
# PowerShell script to manage the C++20 LLM REPL application
# Usage: .\llm-repl.ps1 [command] [options]

param(
    [Parameter(Position=0)]
    [ValidateSet("status", "clean", "build", "run", "test", "format", "help")]
    [string]$Command = "help",

    [Parameter(Position=1, ValueFromRemainingArguments=$true)]
    [string[]]$AppArgs = @(),

    [switch]$Release,
    [switch]$DebugMode,
    [switch]$VerboseOutput,
    [switch]$Force
)

# Configuration
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$SourceDir = Join-Path $ProjectRoot "src"
$ExeName = "llm-repl.exe"
$ExePath = Join-Path $BuildDir $ExeName

#region Helper Functions

# Colors for output
function Write-ColorOutput {
    param([string]$Message, [ConsoleColor]$ForegroundColor = 'White')
    Write-Host $Message -ForegroundColor $ForegroundColor
}

function Write-Success { param([string]$Message) Write-ColorOutput "✅ $Message" Green }
function Write-Error { param([string]$Message) Write-ColorOutput "❌ $Message" Red }
function Write-Warning { param([string]$Message) Write-ColorOutput "⚠️  $Message" Yellow }
function Write-Info { param([string]$Message) Write-ColorOutput "ℹ️  $Message" Cyan }
function Write-Header { param([string]$Message) Write-ColorOutput "`n🚀 $Message" Magenta }

# Banner
function Show-Banner {
    Write-Host @"

██╗     ██╗     ███╗   ███╗    ██████╗ ███████╗██████╗ ██╗
██║     ██║     ████╗ ████║    ██╔══██╗██╔════╝██╔══██╗██║
██║     ██║     ██╔████╔██║    ██████╔╝█████╗  ██████╔╝██║
██║     ██║     ██║╚██╔╝██║    ██╔══██╗██╔══╝  ██╔═══╝ ██║
███████╗███████╗██║ ╚═╝ ██║    ██║  ██║███████╗██║     ███████╗
╚══════╝╚══════╝╚═╝     ╚═╝    ╚═╝  ╚═╝╚══════╝╚═╝     ╚══════╝

C++20 Interactive AI Chat Terminal - Management Script v1.0

"@ -ForegroundColor Cyan
}

# Check if command exists
function Test-Command {
    param([string]$CommandName)
    try {
        Get-Command $CommandName -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

#endregion

#region Main Functions

# Get application status
function Get-AppStatus {
    Write-Header "APPLICATION STATUS"

    # Check project structure
    Write-Info "Checking project structure..."
    $checks = @(
        @{Name="Project Root"; Path=$ProjectRoot; Expected=$true},
        @{Name="Source Directory"; Path=$SourceDir; Expected=$true},
        @{Name="CMakeLists.txt"; Path=(Join-Path $ProjectRoot "CMakeLists.txt"); Expected=$true},
        @{Name="Build Directory"; Path=$BuildDir; Expected=$false},
        @{Name="Executable"; Path=$ExePath; Expected=$false}
    )

    foreach ($check in $checks) {
        $exists = Test-Path $check.Path
        if ($exists) {
            Write-Success "$($check.Name): Found"
            if ($check.Name -eq "Executable") {
                $size = [math]::Round((Get-Item $check.Path).Length / 1MB, 2)
                Write-Info "  Size: $size MB"
                $lastWrite = (Get-Item $check.Path).LastWriteTime
                Write-Info "  Last Built: $lastWrite"
            }
        } else {
            if ($check.Expected) {
                Write-Error "$($check.Name): Missing (Required)"
            } else {
                Write-Warning "$($check.Name): Not found"
            }
        }
    }

    # Check dependencies
    Write-Info "`nChecking build dependencies..."
    $deps = @(
        @{Name="CMake"; Command="cmake"},
        @{Name="Clang++"; Command="clang++"},
        @{Name="Make"; Command="make"}
    )

    foreach ($dep in $deps) {
        if (Test-Command $dep.Command) {
            try {
                $version = & $dep.Command --version 2>$null | Select-Object -First 1
                Write-Success "$($dep.Name): Available"
                Write-Info "  Version: $version"
            } catch {
                Write-Success "$($dep.Name): Available (version check failed)"
            }
        } else {
            Write-Error "$($dep.Name): Not found"
        }
    }

    # Count source files
    if (Test-Path $SourceDir) {
        $cppFiles = Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp" | Measure-Object
        $hppFiles = Get-ChildItem -Path $SourceDir -Recurse -Include "*.hpp" | Measure-Object
        $totalLines = Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp","*.hpp" |
                     Get-Content | Measure-Object -Line

        Write-Info "`nSource Code Statistics:"
        Write-Info "  C++ Files: $($cppFiles.Count)"
        Write-Info "  Header Files: $($hppFiles.Count)"
        Write-Info "  Total Lines: $($totalLines.Lines)"
    }
}

# Clean build artifacts
function Invoke-Clean {
    Write-Header "CLEANING BUILD ARTIFACTS"

    if (Test-Path $BuildDir) {
        Write-Info "Removing build directory: $BuildDir"
        try {
            Remove-Item -Path $BuildDir -Recurse -Force
            Write-Success "Build directory cleaned"
        } catch {
            Write-Error "Failed to clean build directory: $($_.Exception.Message)"
            return $false
        }
    } else {
        Write-Info "Build directory doesn't exist - nothing to clean"
    }

    # Clean other artifacts
    $artifacts = @(
        (Join-Path $ProjectRoot "test_config.yaml"),
        (Join-Path $ProjectRoot "*.log")
    )

    foreach ($pattern in $artifacts) {
        $files = Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue
        if ($files) {
            Write-Info "Removing artifacts: $pattern"
            Remove-Item -Path $files -Force
        }
    }

    Write-Success "Clean completed"
    return $true
}

# Build the application
function Invoke-Build {
    Write-Header "BUILDING APPLICATION"

    # Determine build type
    $buildType = "Release"
    if ($DebugMode) { $buildType = "Debug" }
    if ($Release) { $buildType = "Release" }

    Write-Info "Build Type: $buildType"
    Write-Info "Build Directory: $BuildDir"

    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        Write-Info "Creating build directory..."
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }

    # Change to build directory
    Push-Location $BuildDir

    try {
        # Configure with CMake
        Write-Info "Configuring with CMake..."
        $cmakeArgs = @(
            "..",
            "-DCMAKE_CXX_COMPILER=clang++",
            "-DCMAKE_BUILD_TYPE=$buildType",
            "-G", "Unix Makefiles"
        )

        if ($VerboseOutput) {
            Write-Info "CMake command: cmake $($cmakeArgs -join ' ')"
        }

        $configResult = & cmake @cmakeArgs 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed"
            Write-Host $configResult -ForegroundColor Red
            return $false
        }
        Write-Success "CMake configuration completed"

        # Build with Make
        Write-Info "Building with Make..."
        $makeArgs = @("-j4")
        if ($VerboseOutput) { $makeArgs += "VERBOSE=1" }

        $buildStart = Get-Date
        $buildResult = & make @makeArgs 2>&1
        $buildEnd = Get-Date
        $buildTime = ($buildEnd - $buildStart).TotalSeconds

        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed"
            Write-Host $buildResult -ForegroundColor Red
            return $false
        }

        Write-Success "Build completed in $([math]::Round($buildTime, 1)) seconds"

        # Check if executable was created
        if (Test-Path $ExePath) {
            $size = [math]::Round((Get-Item $ExePath).Length / 1MB, 2)
            Write-Success "Executable created: $ExeName ($size MB)"
        } else {
            Write-Error "Executable not found after build"
            return $false
        }

    } finally {
        Pop-Location
    }

    return $true
}

# Run the application
function Invoke-Run {
    Write-Header "RUNNING APPLICATION"

    # Check if executable exists
    if (-not (Test-Path $ExePath)) {
        Write-Error "Executable not found: $ExePath"
        Write-Info "Run 'build' command first"
        return $false
    }

    # Prepare arguments
    $runArgs = @()
    if ($AppArgs.Count -gt 0) {
        $runArgs = $AppArgs
        Write-Info "Arguments: $($runArgs -join ' ')"
    } else {
        Write-Info "No arguments provided - showing help"
        $runArgs = @("--help")
    }

    Write-Info "Executing: $ExePath $($runArgs -join ' ')"
    Write-Info "Press Ctrl+C to exit the application"
    Write-Host ("=" * 60) -ForegroundColor Gray

    try {
        # Run the application
        & $ExePath @runArgs
        $exitCode = $LASTEXITCODE

        Write-Host ("=" * 60) -ForegroundColor Gray
        if ($exitCode -eq 0) {
            Write-Success "Application exited successfully"
        } else {
            Write-Warning "Application exited with code: $exitCode"
        }
    } catch {
        Write-Error "Failed to run application: $($_.Exception.Message)"
        return $false
    }

    return $true
}

# Run tests
function Invoke-Test {
    Write-Header "RUNNING TESTS"

    $testScript = Join-Path $ProjectRoot "test_suite.sh"
    if (Test-Path $testScript) {
        Write-Info "Running test suite..."
        if (Test-Command "bash") {
            & bash $testScript
        } else {
            Write-Warning "Bash not available - skipping shell test suite"
        }
    } else {
        Write-Warning "Test suite not found: $testScript"
    }

    # Basic executable tests
    if (Test-Path $ExePath) {
        Write-Info "Running basic executable tests..."

        $tests = @(
            @{Name="Version Check"; Args=@("--version"); ExpectedPattern="LLM REPL v"},
            @{Name="Help Output"; Args=@("--help"); ExpectedPattern="Interactive AI Chat Terminal"}
        )

        foreach ($test in $tests) {
            Write-Info "Testing: $($test.Name)"
            try {
                $output = & $ExePath @($test.Args) 2>&1
                if ($output -match $test.ExpectedPattern) {
                    Write-Success "  ✅ $($test.Name) - PASSED"
                } else {
                    Write-Error "  ❌ $($test.Name) - FAILED"
                }
            } catch {
                Write-Error "  ❌ $($test.Name) - ERROR: $($_.Exception.Message)"
            }
        }
    } else {
        Write-Error "Executable not found - build first"
        return $false
    }

    return $true
}

# Format code
function Invoke-Format {
    Write-Header "FORMATTING CODE"

    if (-not (Test-Command "clang-format")) {
        Write-Error "clang-format not found"
        return $false
    }

    $sourceFiles = Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp","*.hpp"
    Write-Info "Found $($sourceFiles.Count) source files"

    foreach ($file in $sourceFiles) {
        Write-Info "Formatting: $($file.Name)"
        & clang-format -i $file.FullName
    }

    Write-Success "Code formatting completed"
    return $true
}

# Show help
function Show-Help {
    Show-Banner
    Write-Host @"
USAGE:
    .\llm-repl.ps1 <command> [options] [-- app-args]

COMMANDS:
    status      Show application and build status
    clean       Clean build artifacts and temporary files
    build       Build the application (default: Release mode)
    run         Run the application with optional arguments
    test        Run test suite and validation checks
    format      Format source code with clang-format
    help        Show this help message

OPTIONS:
    -Release    Build in Release mode (default)
    -DebugMode  Build in Debug mode
    -VerboseOutput Show verbose output during build
    -Force      Force operations (where applicable)

EXAMPLES:
    .\llm-repl.ps1 status                   # Check application status
    .\llm-repl.ps1 clean                    # Clean build artifacts
    .\llm-repl.ps1 build -DebugMode -VerboseOutput # Debug build with verbose output
    .\llm-repl.ps1 run                      # Run with --help (default)
    .\llm-repl.ps1 run -- --version         # Run with --version flag
    .\llm-repl.ps1 run -- -p groq -k key123 # Run with provider and API key
    .\llm-repl.ps1 test                     # Run test suite

WORKFLOW:
    1. .\llm-repl.ps1 status               # Check dependencies
    2. .\llm-repl.ps1 clean                # Clean if needed
    3. .\llm-repl.ps1 build                # Build application
    4. .\llm-repl.ps1 test                 # Validate build
    5. .\llm-repl.ps1 run -- [args]        # Run application

"@ -ForegroundColor White
}

#endregion

#region Main Execution

# Main execution
function Main {
    try {
        switch ($Command.ToLower()) {
            "status" { Get-AppStatus }
            "clean" {
                $result = Invoke-Clean
                if (-not $result) { exit 1 }
            }
            "build" {
                $result = Invoke-Build
                if (-not $result) { exit 1 }
            }
            "run" {
                $result = Invoke-Run
                if (-not $result) { exit 1 }
            }
            "test" {
                $result = Invoke-Test
                if (-not $result) { exit 1 }
            }
            "format" {
                $result = Invoke-Format
                if (-not $result) { exit 1 }
            }
            "help" { Show-Help }
            default {
                Write-Error "Unknown command: $Command"
                Show-Help
                exit 1
            }
        }
    } catch {
        Write-Error "Script execution failed: $($_.Exception.Message)"
        if ($VerboseOutput) {
            Write-Host $_.ScriptStackTrace -ForegroundColor Red
        }
        exit 1
    }
}

# Execute main function
Main

#endregion