# Implementation Tasks: Add Beads Project Tracking

## 1. Installation & Configuration
- [x] 1.1 Install bd CLI (v0.49.6) via official install script
- [x] 1.2 Run `bd init` in project root to create `.beads/` directory
- [x] 1.3 Configure role: `bd config set beads.role maintainer`
- [x] 1.4 Run `bd doctor --fix --yes` to resolve initialization warnings
- [x] 1.5 Install git hooks (pre-commit, pre-push, post-checkout, post-merge, prepare-commit-msg)

## 2. Epic & Task Structure
- [x] 2.1 Create top-level project tracking epic (workspace-sbe)
- [x] 2.2 Create "Beads Integration & Configuration" epic (workspace-1zw) with subtasks
- [x] 2.3 Create "Phase 2: Production-Ready Features" epic (workspace-n49) with subtasks
- [x] 2.4 Set up parent-child and blocking dependencies between tasks
- [x] 2.5 Close completed tasks (bd install/init)

## 3. Documentation Updates
- [x] 3.1 Update AGENTS.md with beads workflow protocol
- [x] 3.2 Update README.md with project tracking section
- [x] 3.3 Create OpenSpec proposal (this document)
- [x] 3.4 Create OpenSpec spec delta for new `project-tracking` capability

## 4. Validation
- [x] 4.1 Verify `bd ready --json` returns expected unblocked tasks
- [x] 4.2 Verify `bd graph workspace-sbe --compact` shows correct hierarchy
- [x] 4.3 Run `openspec validate add-beads-project-tracking --strict` (if available)
- [x] 4.4 Commit and push all changes
