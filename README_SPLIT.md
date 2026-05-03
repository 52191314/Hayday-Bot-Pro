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
