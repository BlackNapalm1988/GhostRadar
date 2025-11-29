# GhostRadar Codex Tasks Guide

This file describes common Codex workflows for the GhostRadar project and which agent to use for each.

## 1. Run an include / header discipline audit

**Agent:** `ghost_include_auditor`

**When to use:** After refactors, before releases, or when you suspect include/circular dependency problems.

**Suggested prompt:**

> Scan all files under `shared/include` and `shared/src` (and optionally `boards/*/src` and `boards/*/include`) for include and header discipline issues. Identify circular includes, unnecessary includes, headers that fail to include what they use, and any shared files that include board-specific headers. Provide: (1) a high-level summary, (2) file-by-file findings, and (3) a concrete refactor checklist I can follow. Do not modify any files, only report findings.

---

## 2. Refactor shared core logic

**Agent:** `ghost_shared_core_engineer`

**When to use:** Improving Display, Sensors, TouchUI, WifiRadar, Dictionary, Settings, SDManager, or other shared subsystems.

**Suggested prompt:**

> Refactor the shared core in `shared/src` and `shared/include` to [describe goal, e.g. reduce coupling between Display and Sensors]. Follow the existing architecture where the shared code remains board-agnostic and uses BoardConfig.h only for hardware details. Apply minimal, incremental changes and output a diff patch.

---

## 3. Bring up a new board

**Agent:** `ghost_board_bringup_engineer`

**When to use:** Adding a new ESP32/ESP32-S3 board, or adjusting pins/wiring for an existing board.

**Suggested prompt:**

> Using `boards/esp_wroom_32` as a reference, create a new board under `boards/<new_board_name>`. Copy the existing structure (platformio.ini, src/main.cpp, src/BoardConfig.cpp, include/BoardPins.h, data/). Update platformio.ini with the correct PlatformIO board ID, and adjust BoardPins.h and BoardConfig.cpp wiring for [describe hardware: display, touch, SD, sensors]. Keep all hardware-specific details inside the new board folder and do not modify shared code.

---

## 4. Adjust PlatformIO multi-board topology

**Agent:** `ghost_architect`

**When to use:** Changing how environments are structured, adding/removing PlatformIO projects, or restructuring boards.

**Suggested prompt:**

> Review the current PlatformIO configuration for all boards under `boards/`. Propose a clean, consistent strategy for multi-board support (one project per board, shared src_filter approach, etc.). Produce a short rationale and a step-by-step plan listing which platformio.ini files need changes and what those changes should be. Do not modify files directly; output a plan I or another agent can follow.

---

## 5. Implement a cross-cutting feature

**Agent:** `ghost_fullstack_feature_engineer`

**When to use:** Adding a feature that touches shared core, board glue, and possibly configuration or docs.

**Suggested prompt:**

> Implement a new feature for GhostRadar: [describe feature]. This may require updates to shared core, one or more boards, and documentation. First, analyze the repository to understand where this feature should live. Then: (1) propose a brief plan, (2) apply changes in small, coherent steps, and (3) output diff-style patches. Respect the rules that shared code must remain board-agnostic and board wiring stays in boards/<board> only.

---

## 6. Update or improve documentation

**Agent:** `ghost_architect` or `ghost_fullstack_feature_engineer`

**When to use:** Editing README, adding docs for new boards, or documenting new features.

**Suggested prompt:**

> Update the root README.md and any relevant docs under `docs/` to document [describe change: new board, new feature, config flow, etc.]. Keep the tone concise and developer-friendly, and ensure the multi-board architecture and build steps remain accurate.

---

You can extend this file with more specialized tasks over time as the project grows.
