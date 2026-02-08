## ADDED Requirements

### Requirement: Beads Issue Tracker Integration
The project SHALL use Beads (bd) as the persistent, git-backed issue tracker for task management alongside OpenSpec for specification management.

#### Scenario: Beads initialization
- **WHEN** a developer or agent clones the repository
- **THEN** the `.beads/` directory SHALL exist with a valid SQLite database
- **THEN** the `bd` CLI SHALL be able to query issues with `bd ready --json`

#### Scenario: Session workflow protocol
- **WHEN** an AI agent begins a work session
- **THEN** the agent SHALL run `bd ready` to find unblocked work
- **THEN** the agent SHALL claim tasks with `bd update <id> --status in_progress`
- **THEN** the agent SHALL close completed tasks with `bd close <id> --reason "..."`
- **THEN** the agent SHALL run `bd sync` at the end of the session

### Requirement: Epic and Task Hierarchy
The project SHALL maintain a hierarchical task structure using Beads epics, tasks, and subtasks with dependency relationships.

#### Scenario: Project epic structure
- **WHEN** project tasks are queried with `bd graph`
- **THEN** tasks SHALL be organized under epics with parent-child relationships
- **THEN** blocking dependencies SHALL prevent work on tasks whose prerequisites are incomplete
- **THEN** `bd ready` SHALL return only tasks with no open blockers

#### Scenario: Task creation standards
- **WHEN** a new task is created
- **THEN** the task SHALL have a descriptive title, priority (P0-P4), and type (epic/feature/task/bug)
- **THEN** the task SHALL be linked to its parent epic via parent-child dependency
- **THEN** blocking dependencies SHALL be added where task ordering matters

### Requirement: OpenSpec and Beads Coexistence
The project SHALL maintain clear separation between OpenSpec (specifications) and Beads (task tracking) with documented guidelines.

#### Scenario: Choosing the right tool
- **WHEN** formal capability changes, API changes, or architecture decisions are needed
- **THEN** developers SHALL use OpenSpec change proposals
- **WHEN** day-to-day task tracking, progress reporting, or session handoffs are needed
- **THEN** developers SHALL use Beads (bd)

#### Scenario: Cross-referencing
- **WHEN** an OpenSpec proposal generates implementation tasks
- **THEN** corresponding Beads tasks SHALL be created with references to the proposal
- **THEN** Beads task notes SHALL link to relevant OpenSpec spec paths
