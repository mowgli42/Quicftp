<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

## Beads (bd) — Persistent Task Tracking

This project uses **Beads** (`bd`) as its git-backed issue tracker for persistent task management across sessions. Beads complements OpenSpec: use OpenSpec for formal specifications and change proposals, use Beads for day-to-day task tracking.

### Prerequisites
- Install `bd` CLI: `curl -fsSL https://raw.githubusercontent.com/steveyegge/beads/main/scripts/install.sh | bash`
- Minimum version: v0.49.6+
- The `.beads/` directory is already initialized in this repository

### Session Protocol
Every work session MUST follow this protocol:

1. **Start**: `bd ready` — Find unblocked work
2. **Claim**: `bd update <id> --status in_progress` — Claim a task
3. **Context**: `bd show <id>` — Get full task details and history
4. **Progress**: `bd update <id> --notes "..."` — Record decisions and outcomes at milestones
5. **Complete**: `bd close <id> --reason "..."` — Close completed tasks
6. **Sync**: `bd sync` — Persist to git (always at session end)

### When to Use Beads vs OpenSpec vs TodoWrite

| Need | Tool | Why |
|------|------|-----|
| Formal spec / capability change | OpenSpec | Proposals, delta specs, validation |
| Persistent task with dependencies | Beads (bd) | Survives sessions, git-backed |
| Ephemeral session checklist | TodoWrite | Quick, disposable, current-session only |

### Key Commands
```bash
bd ready --json          # Find unblocked tasks
bd show <id>             # View task details
bd create "Title" -t task -p 1 --json  # Create new task
bd update <id> --status in_progress    # Claim task
bd close <id> --reason "Done"          # Complete task
bd graph <epic-id> --compact           # View dependency tree
bd sync                                # Sync to git
```

### Current Project Epics
- `workspace-sbe` — Top-level project tracking
  - `workspace-1zw` — Beads Integration & Configuration
  - `workspace-n49` — Phase 2: Production-Ready Features

### Creating New Tasks
When creating tasks:
1. Set descriptive title, priority (P0-P4), and type (epic/feature/task/bug)
2. Link to parent epic: `bd dep add <task-id> <epic-id> --type parent-child`
3. Add blocking deps where ordering matters: `bd dep add <blocked> <blocker> --type blocks`
4. Include acceptance criteria: `bd update <id> --acceptance "..."`

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create bd issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished bd issues, update in-progress items
4. **Sync and PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   bd sync
   git pull --rebase
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session via bd notes

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
- ALWAYS run `bd sync` before pushing to persist task updates
