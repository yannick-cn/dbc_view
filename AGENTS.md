# Repository Guidelines

## Project Structure & Module Organization
Qt6/C++ sources live in `src/`, covering `main.cpp`, `MainWindow`, and the parser/data models. CMake writes artifacts to `build/`; leave it untracked and regenerate it whenever compiler or Qt options change. Scripts (`build.sh`, `run_dbc_viewer.sh`, `test_app.sh`) handle build, launch, and smoke checks, and desktop assets (`icon.png`, `icon.svg`, `DBCViewer.desktop`) support packaging. Treat `ADC321_CAN_ADASTORADAR_2025_08_25_V0.0.2.dbc` as the baseline sample and place any additional fixtures under `assets/`.

## Build, Test, and Development Commands
Run `./build.sh` for a Release build that configures CMake and compiles with full parallelism. For debugging, use `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`, then start `build/DBCViewer`. `./run_dbc_viewer.sh` performs environment checks (DISPLAY, X server, Qt plugins) before launching; keep using it when targeting end users. Execute `./test_app.sh` to confirm the binary exists, the sample DBC is present, and the viewer boots within a short timeout.

## Coding Style & Naming Conventions
Stick to C++17 with Qt idioms: classes use PascalCase (`CanMessage`), member variables keep the `m_` prefix, and prefer Qt containers/types (`QString`, `QList`) in UI paths. Indent with four spaces, drop braces to the next line for functions and classes, and group includes with Qt headers first, then locals. Name new source files after their primary class and keep related UI resources alongside the code that consumes them.

## Testing Guidelines
Extend `test_app.sh` or add new shell helpers when build verification must cover more scenarios (e.g., multiple DBC fixtures). For logic decoupled from the GUI, introduce Qt Test cases under `tests/` (create the directory if absent), name files `<Component>Test.cpp`, and register them with CMake so `ctest` runs after `cmake --build build`. Before opening a pull request, launch the viewer with the bundled sample, note any manual checks performed, and capture errors from the console.

## Commit & Pull Request Guidelines
Follow Conventional Commits (`feat:`, `fix:`, `chore:`) as seen in the current history, keeping subject lines under 72 characters and in the imperative. Each commit should encapsulate one cohesive change and reference related issues or tickets when relevant. Pull requests need a concise summary, explicit test results (command plus outcome), and screenshots or short clips for UI updates; call out environment details if a specific Qt or compiler version matters.
