# Hay Day Mod/Bot Repository Split

This folder is the clean Hay Day-specific repository.

Included:
- `HD/`: C++ bot/mod source and Visual Studio project
- `scripts/`: Hay Day asset/mod generation helper scripts
- `runtime/`: runtime folders copied from the built release tree:
  - `runtime/templates`
  - `runtime/icons`
  - `runtime/tessdata`
  - `runtime/injecthacks`
- small tag/config artifacts used by the mod workflow

Excluded from Git:
- build outputs such as `.exe`, `.dll`, `.pdb`, `x64/`, `Release/`
- Visual Studio local state such as `.vs/`
- large raw/generated asset dumps such as `.zip`, `.fla`, `Decoded/`, `Extracted/`, `codex_tmp/`

After building `HD/Premium bot.sln`, copy the runtime folders next to the built `premium.exe` if the build output directory does not already contain them.

Verified local build command:

```powershell
& 'E:/Programs/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' '.\HD\Premium bot.sln' /p:Configuration=Release /p:Platform=x64 /m
```

Build dependency note:
- The Visual Studio project links Tesseract/Leptonica from vcpkg. Install the dependencies in `HD/vcpkg.json` or keep `E:/vcpkg/installed/x64-windows` available when building with the current project settings.
