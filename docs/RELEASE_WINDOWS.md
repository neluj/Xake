# Windows Release Procedure

## Build and Test

Use a Release build with the same Qt toolchain intended for distribution:

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/mingw_64
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
cmake --build build-release --target package
```

The package target installs `Xake.exe`, invokes Qt's deployment tooling
(`windeployqt` on Windows), installs legal notices, and creates a ZIP archive.
Official 0.3.0 packages use Qt 6.10.3 with MinGW 13.1, matching CI.

## Isolated Smoke Test

Extract the archive and run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/test_windows_package.ps1 `
  -PackageDirectory C:\path\to\extracted-package
```

The script removes Qt and MinGW from `PATH`, clears Qt plugin environment
variables, uses an isolated application-data directory, and starts the packaged
binary with `--smoke-test`. This catches missing Qt plugins and runtime DLLs on
the build machine.

## Clean Windows Validation

Before publishing, copy the ZIP to a clean Windows 10 or Windows 11 x86-64
virtual machine that has neither Qt nor MinGW installed:

1. Extract the complete ZIP.
2. Confirm `where.exe qmake`, `where.exe windeployqt`, and
   `where.exe libstdc++-6.dll` find nothing outside the package.
3. Start `bin\Xake.exe`.
4. Open About and verify version 0.3.0.
5. Configure two known UCI engines and complete a short timed game.
6. Run a two-game tournament with color alternation and an opening file.
7. Replay the saved game from History and navigate First, Previous, Next, and
   Last.
8. Open the tournament report and PGN through Replay and switch games.
9. Verify History, PGN, JSON report, unified UCI log, pause, stop, and restart.
10. Reboot the VM and verify that the last engine paths and settings are restored.

The isolated smoke test is automated in CI. A genuinely clean VM remains a
manual release gate because the build host cannot prove the absence of system
runtime dependencies.

## Release Checklist

- All CTest tests pass in Release mode.
- The isolated package smoke test passes.
- The clean-VM validation above passes.
- The archive contains `LICENSE`, `THIRD_PARTY_NOTICES.md`,
  `QT_SOURCE_OFFER.md`, and `licenses`.
- The executable properties report product and file version 0.3.0.
- The final archive checksum is recorded in the release notes.
