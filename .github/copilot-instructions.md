# GitHub Copilot Instructions for SFRESP32

## 1. Project Architecture & Patterns
- **Framework:** ESP-IDF (C Language) for ESP32.
- **Custom Scheduler:** This project uses a custom cooperative/interrupt-driven scheduler, **NOT** standard FreeRTOS tasks.
  - **`app_main`**: Initializes the device and enters an infinite loop calling `task_BG()`.
  - **`task_BG`**: Background task run continuously in the main loop.
  - **`task_1ms` / `task_100ms`**: High-priority tasks triggered by hardware timer interrupts (`esp_timer`).
  - **Do NOT** create new FreeRTOS tasks using `xTaskCreate` unless strictly necessary. Hook into the existing `tasks.c` structure.
- **Task Watchdog:** The watchdog is manually kicked in `task_BG` only after all tasks report `eTASK_INACTIVE`, ensuring all tasks are serviced.

## 2. Naming Conventions (Strict)
This project enforces a specific Hungarian-like notation described in `Variable_Guidelines.txt`.
- **Variable Structure:** `[physical property][Part of car][Descriptor][unit(optional)]`
- **Prefixes:**
  - `st` : Structure or Handle (e.g., `stTaskInterrupt1ms`)
  - `e`  : Enumeration (e.g., `eResetReason`)
  - `by` : Byte (`uint8_t`)
  - `w`  : Word (`uint16_t`)
  - `dw` : Double Word (`uint32_t`)
  - `qw` : Quad Word (`uint64_t`)
  - `b`  : Boolean
  - `a`  : Array (e.g., `adwMaxTaskTime` is an Array of Dwords)
  - `p`  : Pointer
- **Examples:**
  - `nEngine` (Rotational velocity of Engine)
  - `dRadWidth` (Distance/width of Radiator)
  - `astTaskState` (Array of Struct/Enum TaskState)

## 3. Type System (`sfrtypes.h`)
- Use project-specific typedefs over standard C types where possible:
  - `byte` (unsigned char)
  - `word` (unsigned short)
  - `dword` (unsigned long)
  - `qword` (unsigned long long)
  - `boolean` (int, `TRUE`/`FALSE`)
- Use `CAN_frame_t` for CAN messages (aligned to 4 bytes).

## 4. Key Files
- **`main/sfrtypes.h`**: Critical definitions for types, enums (`eTasks_t`), and structs. **Read this first for type errors.**
- **`main/tasks.c`**: Implementation of the custom scheduler logic.
- **`main/main.c`**: hardware timer callbacks (`call_back_1ms`) and entry point.
- **`util/`**: Python scripts for CAN decoding and flashing.

## 5. Developer Workflow
- **Build:** Standard ESP-IDF build system (`idf.py build`, `idf.py flash`).
- **Mode Switching:** `eDeviceMode` global variable switches between `eNORMAL` and `eREFLASH` modes.
- **IRAM Safe:** Timer callbacks (`call_back_1ms`) are marked `IRAM_ATTR`. Ensure code called from these is ISR-safe and ideally in IRAM.
