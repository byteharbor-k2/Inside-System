# Repository Guidelines

## Project Structure & Module Organization

This repository is a knowledge base for system notes and small Ubuntu utilities, not an application with a build output. Keep topic content grouped by domain:

- `Linux/ubuntu/`: Ubuntu setup, networking, DNS, Docker, and input-method notes
- `Linux/ubuntu/scripts/`: Bash helpers related to the Ubuntu docs
- `hardware/`: hardware and computer architecture notes
- `README.md`: top-level index; update it when adding or moving major documents

Place new Markdown files beside related notes, and place executable helpers in the nearest `scripts/` subdirectory.

## Build, Test, and Development Commands

There is no build step. Use lightweight checks before opening a PR:

- `rg --files`: inspect the repository quickly
- `bash -n Linux/ubuntu/scripts/*.sh`: validate Bash syntax
- `shellcheck Linux/ubuntu/scripts/*.sh`: optional lint pass if `shellcheck` is installed
- `git diff --check`: catch trailing whitespace and malformed patches

Run system-changing scripts only after reviewing them fully, for example: `sudo bash Linux/ubuntu/scripts/apply_v2rayn_dns_frontend.sh`.

## Coding Style & Naming Conventions

Use clear, task-focused Markdown with short sections and explicit command examples in fenced `bash` blocks. Prefer descriptive file names; for new docs, use lowercase hyphenated names when practical. For shell scripts, follow the existing pattern: lowercase snake_case names ending in `.sh`.

In Bash, keep `#!/usr/bin/env bash` and `set -euo pipefail`, quote variables, use uppercase names for constants, and make privileged or destructive actions obvious.

## Testing Guidelines

There is no automated test suite or coverage target. For documentation changes, verify links, paths, and command accuracy manually. For script changes, at minimum run `bash -n` and, if available, `shellcheck`. PRs that touch system scripts should include manual validation steps and expected results on Ubuntu.

## Commit & Pull Request Guidelines

Recent history uses short prefix-style subjects such as `docs: add ubuntu network notes and repo index` and `doc: computer performance improvement`. Follow that pattern with concise, imperative summaries, preferably `docs:`, `scripts:`, or `chore:`.

PRs should state the purpose, list the affected paths, summarize manual verification, and call out any risk around `sudo`, `/etc`, `systemd`, Docker, or data deletion. Include terminal output when it helps reviewers confirm behavior.
