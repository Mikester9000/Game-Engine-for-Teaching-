# Copilot Continuation Plan

## 1. Project Mission and Non-Goals
- **Mission:** Develop a game engine tailored for teaching purposes, focusing on a modular architecture similar to FF15.
- **Non-goals:**  
  - Creating a content-heavy game;  
  - Focusing on performance optimizations typically desired in commercial engines.

## 2. Coding Standards
- Use the **TEACHING NOTE** comment style for clarity.
- Follow a **file-per-feature** structure. Example:
  - `water_physics.cpp`
  - `weather_system.cpp`
  - `quest_01.cpp`

## 3. Repo Structure Conventions
- Group files by features/modules.
- Maintain clear naming conventions that reflect functionality.

## 4. Strict PR Sequence Roadmap
1. Vulkan sandbox  
2. Triangle  
3. AssetDB and cooker  
4. Hello texture  
5. Audio backend  
6. Animation runtime  
7. Physics integration  
8. Editor shell  
9. Streaming

## 5. Acceptance Criteria and Automated Test Requirements
- For each milestone:
  - Define acceptance criteria clearly.  
  - Implement automated tests with strict headless validation modes.

## 6. Windows/Vulkan Build Assumptions
- **IDE:** Visual Studio 2022  
- **SDK:** Vulkan SDK  
- **CMake Options:**  
  - `ENGINE_ENABLE_TERMINAL`  
  - `ENGINE_ENABLE_VULKAN`

## 7. Integrating External Tool Repositories
- Use shared asset manifest contracts for integration.  
- Ensure proper management of cooked outputs from:
  - Creation-Engine  
  - Audio-Engine  
  - Animation-Engine

## 8. Instructions for Writing Issues/PR Descriptions
- **Commands to Build/Run:**  
  - Provide exact commands here.  
- **What Changed:**  
  - Describe the changes made succinctly.  
- **What to Test:**  
  - Clearly specify testing requirements based on the changes.