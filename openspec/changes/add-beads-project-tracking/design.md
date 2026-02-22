## Context
The Quicftp project uses OpenSpec for spec-driven development with formal change proposals, but lacks granular task tracking that persists across agent sessions. Beads (bd) is a distributed, git-backed graph issue tracker designed specifically for AI agent workflows.

## Goals / Non-Goals
- **Goals**:
  - Provide persistent task memory across agent sessions
  - Enable dependency-aware task management with blocking relationships
  - Support epic → task → subtask hierarchy for complex features
  - Integrate with existing git workflow (beads stores data in `.beads/` as JSONL)
  - Complement (not replace) OpenSpec for specification management

- **Non-Goals**:
  - Replace OpenSpec for formal change proposals and specifications
  - Require external services or databases beyond git
  - Modify any application source code

## Decisions
- **Decision**: Use Beads (bd) as the persistent task tracker alongside OpenSpec
  - **Why**: Beads is git-native, agent-optimized, supports dependency graphs, and survives conversation compaction
  - **Alternatives considered**:
    - GitHub Issues: Requires API access, not git-native, no offline support
    - Plain markdown checklists: No dependency tracking, no structured queries, no persistence across sessions
    - TodoWrite only: Ephemeral, lost after session ends

- **Decision**: Maintain both OpenSpec and Beads with clear separation of concerns
  - OpenSpec: Formal specifications, change proposals, capability definitions
  - Beads: Day-to-day task tracking, progress visibility, session handoffs, dependency graphs

- **Decision**: Use `maintainer` role for beads configuration
  - Project has direct write access, not a fork/contribution model

## Risks / Trade-offs
- **Risk**: Two tracking systems could cause confusion → Mitigation: Clear documentation on when to use which (OpenSpec for specs, bd for tasks)
- **Risk**: `.beads/` directory adds files to git → Mitigation: JSONL files are small, merge-friendly, and provide value
- **Risk**: bd CLI version drift → Mitigation: Pin minimum version (v0.49.6+) in docs

## Migration Plan
1. Install bd CLI (no project code changes)
2. Initialize `.beads/` directory
3. Create initial epic/task structure
4. Update documentation
5. All changes are additive; rollback = delete `.beads/` and revert doc changes

## Open Questions
- None at this time; beads integration is straightforward and additive
