# Change: Add Beads (bd) Project Tracking Integration

## Why
The project lacks a persistent, structured task tracking system that survives across AI agent sessions and supports dependency-aware work management. OpenSpec handles specification-level proposals well, but there is no mechanism for granular task tracking, progress visibility, or multi-session context preservation. Beads (bd) fills this gap as a git-backed, agent-optimized issue tracker with dependency graphs, compaction survival, and zero-conflict hash-based IDs.

## What Changes
- **New capability**: `project-tracking` — defines how the project uses Beads (bd) for task management
- Install and configure `bd` CLI (v0.49.6+) in the repository
- Initialize `.beads/` directory with SQLite-backed issue database
- Create epic/task/subtask hierarchy for project workstreams
- Update `AGENTS.md` with beads workflow instructions for AI agents
- Update `README.md` with project tracking documentation
- Integrate beads session protocol (ready → show → update → close → sync) into development workflow

## Impact
- Affected specs: None modified (new capability added)
- Affected code: No application code changes
- New files: `.beads/` directory, updated `AGENTS.md`, updated `README.md`
- New dependency: `bd` CLI tool (installed system-wide, not in project)
- Workflow: AI agents and developers gain persistent task context across sessions
