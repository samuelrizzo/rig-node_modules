# rip-node_modules

A C CLI that finds `node_modules` directories and measures their storage impact.

## What It Does

- Lists active volumes at startup.
- Lets the user select one or more disks, `all`, or enter a directory manually.
- Scans the tree for `node_modules`.
- Generates a report with matches, total bytes, errors, and elapsed time.

## Build

### CMake

```bash
cmake -S . -B build
cmake --build build --config Release
```

### Visual Studio

Open `rip-node_modules.sln` or run:

```powershell
msbuild rip-node_modules.sln /p:Configuration=Release /p:Platform=x64
```

## Usage

### Interactive

```bash
./build/Release/rip-node_modules
```

The program shows active volumes and accepts:

- a disk number
- multiple numbers separated by commas
- `all`
- the option to enter a directory manually

### Direct CLI

```bash
./build/Release/rip-node_modules --scan-path "D:\projetos"
```

Options:

- `--scan-path <path>`: scan a specific path
- `--report-path <file>`: set the output file
- `--benchmark-out <file>`: save execution metrics
- `--no-progress`: disable the progress bar

## Output

By default, the report is saved as `node_modules_report.txt`.

The summary includes:

- scanned volumes
- found `node_modules`
- total bytes and human-readable size
- visited directories and files
- skipped duplicates
- errors by category
- total time, scan time, and report generation time

## Notes

- The project is cross-platform and uses native APIs per platform.
- On Windows, junctions and symlinks may appear in the error breakdown or skipped-item counts, depending on the case.
- On WSL, scanning `/mnt/*` is usually slower than running the Windows binary on the same disk.
