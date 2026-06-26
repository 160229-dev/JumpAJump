<img width="646" height="763" alt="gameplay" src="https://github.com/user-attachments/assets/377e782c-e518-4e18-8436-8230d10def36" />
# **JumpAJump**

```text
     ██╗██╗   ██╗███╗   ███╗██████╗  █████╗      ██╗██╗   ██╗███╗   ███╗██████╗
     ██║██║   ██║████╗ ████║██╔══██╗██╔══██╗     ██║██║   ██║████╗ ████║██╔══██╗
     ██║██║   ██║██╔████╔██║██████╔╝███████║     ██║██║   ██║██╔████╔██║██████╔╝
██   ██║██║   ██║██║╚██╔╝██║██╔══██╗██╔══██║██   ██║██║   ██║██║╚██╔╝██║██╔══██╗
╚█████╔╝╚██████╔╝██║ ╚═╝ ██║██████╔╝██║  ██║╚█████╔╝╚██████╔╝██║ ╚═╝ ██║██████╔╝
 ╚════╝  ╚═════╝ ╚═╝     ╚═╝╚═════╝ ╚═╝  ╚═╝ ╚════╝  ╚═════╝ ╚═╝     ╚═╝╚═════╝
```

> A **tightly polished**, Qt6-powered tribute to the viral WeChat mini-game **Jump Jump**.
> Hold `SPACE` (or mouse) to charge, release to jump, land on the center for bonus points.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Made with Qt6](https://img.shields.io/badge/Made%20with-Qt6-41CD52?logo=qt)](https://www.qt.io/)

---

## ✨ Highlights

- **🎯 Progressively Harder**  
  Distance grows, platforms shrink, and angles become trickier as your score climbs. Easy to start, hard to master.

- **🎨 Polished Visuals**  
  Isometric 2.5D perspective, wood-grain platforms, soft pastel palette, ground layer, shadows, dust particles, landing ripples, jump trails, and screen shake.

- **🔊 Procedural Audio**  
  No external sound files! Pitch-bending charge tone, jump/land SFX, satisfying perfect-landing chord, and a soothing C-Am-F-G background loop — all synthesized in real time via `QAudioSink`.

- **🕹️ Classic Stickman**  
  Black sphere head + slim rectangular body, just like the original icon.

- **🏆 Best Score Persistence**  
  Your personal best is saved locally, so you always have a reason to beat it.

- **🖥️ Native Windows Installer**  
  One-click installer built with **Qt Installer Framework**. Creates desktop & Start Menu shortcuts and ships all required Qt6 runtime DLLs.

---

### Install
1. Download `JumpAJumpInstaller.exe` from the [Releases](https://github.com/160229-dev/JumpAJump.git/releases) page.
2. Run it and follow the wizard.
3. Launch **JumpAJump** from the desktop or Start Menu.

### Controls

| Action | Key / Mouse |
|--------|-------------|
| Charge & Start | Hold `SPACE` or hold Left Mouse Button |
| Jump | Release |
| Toggle Sound | `M` |
| Restart | `SPACE` after Game Over |

---

## 🛠️ Build from Source

### Requirements
- Qt 6.x (`Widgets`, `Multimedia`)
- CMake 3.16+
- A C++17 compiler (MSVC / MinGW / Clang)

### Build
```bash
cd JumpGame
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/your_toolchain -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/JumpAJump.exe
```

To create the installer locally, run `windeployqt` on the built executable and use **Qt Installer Framework**'s `binarycreator`:
```bash
windeployqt --release build/JumpAJump.exe
binarycreator -c installer/config/config.xml -p installer/packages JumpAJumpInstaller.exe
```

---

## 🏅 Show Off Your Score

Got a new high score? We want to see it!

1. Take a screenshot of your **Game Over** screen.
2. Open a [Discussion](../../discussions) or tweet with `#JumpAJumpHighScore`.
3. Beat the dev's score — or your friends'!

> **Current challenge:** Can you reach **50**? Drop a screenshot if you do!

---

## 🖼️ Screenshots


```markdown

```
<img width="646" height="763" alt="gameplay" src="https://github.com/user-attachments/assets/3413151f-5a18-451c-aa72-2948495dd6b3" />
<img width="659" height="768" alt="gameover" src="https://github.com/user-attachments/assets/3aa592d5-e57f-40cf-8753-b68855c7e9e8" />

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).

---

## ⭐ Star This Project

If JumpAJump made you smile, **give it a star** — it helps more people discover the game and keeps the updates coming!

```text
★ Star → Share → Jump again!
```
