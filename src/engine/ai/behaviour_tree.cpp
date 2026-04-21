/**
 * @file behaviour_tree.cpp
 * @brief Behaviour Tree framework implementation.
 *
 * ============================================================================
 * TEACHING NOTE — Why so little code in the .cpp?
 * ============================================================================
 *
 * Most of the BT framework is implemented in the header as inline / template
 * code because:
 *
 *   1. The node types are template-free but very short — the compiler inlines
 *      them anyway.
 *   2. BtBlackboard::Set<T> and BtBlackboard::GetOr<T> are function templates
 *      and MUST be defined in the header so every translation unit that calls
 *      them gets a specialisation.
 *
 * This file exists as the canonical translation unit for any future non-inline
 * methods (e.g. debug pretty-printing, serialisation to JSON for the editor).
 *
 * TEACHING NOTE — Header-only vs. split compilation
 * ────────────────────────────────────────────────────
 * A "header-only" library (all code in .hpp) is convenient but slows
 * compile times: every .cpp that includes the header recompiles all the code.
 * Our approach (minimal .cpp with only the template-free non-trivial methods)
 * is a compromise: templates and short inlines live in the header; anything
 * non-trivial lives here.  For a teaching engine the compile-time difference
 * is negligible, but the pattern is worth knowing.
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 *
 * C++ Standard: C++17
 */

#include "behaviour_tree.hpp"

// The BtTree, BtSequence, BtSelector, BtCondition, and BtAction classes are
// fully implemented in the header.  No additional out-of-line definitions are
// needed at this time.
//
// TEACHING NOTE — Future extension points
// ─────────────────────────────────────────
// When you add features here, consider:
//   • BtTree::Serialize(std::ostream&) — emit JSON for the editor debugger.
//   • BtTree::BuildFromJson(nlohmann::json) — load a tree from an asset file.
//   • BtDebugVisitor — walk the tree and emit per-node last-status to ImGui.
//
// Each of these is a natural homework exercise for students who have mastered
// the core BT mechanics.
