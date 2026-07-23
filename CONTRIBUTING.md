# Contributing to Hayday-Bot-Pro

First off, thank you for considering contributing to **Hayday-Bot-Pro**! It's contributors like you who make this tool reliable, performant, and accessible for the community.

---

## 🌿 Branching Policy & Pull Requests

To maintain codebase stability and code quality, please adhere strictly to our branching policy:

- **Branch Protection**: Never commit or push directly to the `main` branch.
- **Feature Branches**: Create a dedicated feature or fix branch for your work:
  - Features: `feature/your-feature-name`
  - Bug Fixes: `fix/issue-description`
- **Clean Commits**: Ensure commits are squashed or logical, clean, and pass local build verification before opening a Pull Request.
- **PR Process**: Submit your PR targeting `main`. Describe your changes clearly and link any associated issues.

---

## 🛠️ Development Setup & Prerequisites

1. **Development Environment**:
   - Visual Studio 2022 (Community or higher) with the **Desktop development with C++** workload.
   - Windows 10 / 11 64-bit.
   - C++20 language standard support.
2. **Android Emulator**:
   - MEmu Play or LDPlayer.
   - Emulator config: `640x480` resolution, `100 DPI`, **DirectX** render mode, **Root** enabled.

### Building Locally

- **Option A (One-Command Quickstart)**: Run [`quickstart.bat`](file:///d:/02_Projects/HD/HaydayMod/quickstart.bat) from the root directory.
- **Option B (Visual Studio)**:
  1. Open [`HD/Premium bot.sln`](file:///d:/02_Projects/HD/HaydayMod/HD/Premium%20bot.sln).
  2. Set active configuration to `Release` and platform to `x64`.
  3. Press `Ctrl+Shift+B` to build the solution.

---

## 🎯 Finding "Good First Issues"

If you're looking for an easy place to start contributing:

1. Check out issues tagged with [`good first issue`](https://github.com/issues?q=label%3A"good+first+issue") or [`help wanted`](https://github.com/issues?q=label%3A"help+wanted").
2. Examples of great beginner contributions:
   - UI polish for the ImGui control panel.
   - Extra OCR dataset training or alignment tweaks in [`HD/tessdata/`](file:///d:/02_Projects/HD/HaydayMod/HD/tessdata).
   - Logging, error reporting, and ADB recovery routine improvements.
   - Documentation additions or localized guides.

---

## 🎨 Code Style Guidelines

- **Standard**: C++20 standard syntax.
- **Architecture**:
  - Keep UI rendering (ImGui) strictly decoupled from background bot loop operations (`HD/cpp/bot` vs `HD/cpp/app`).
  - Account security & storage logic (`.Ahjie` DPAPI/AES-256 encryption) must remain isolated in `HD/cpp/infra`.
- **Naming Conventions**:
  - `PascalCase` for Class names and struct types.
  - `camelCase` for local variables and parameters.
  - `m_memberVar` for private/protected class member fields.

---

## ❓ Need Help?

Feel free to open a discussion, leave comments on issues, or refer to [docs/COMMUNITY_OUTREACH.md](file:///d:/02_Projects/HD/HaydayMod/docs/COMMUNITY_OUTREACH.md) for community discussions.
