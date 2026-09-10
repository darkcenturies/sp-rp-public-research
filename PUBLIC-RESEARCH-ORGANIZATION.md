# SP-RP public-research repository structure

This repository is the public companion repository to the SP-RP server project.

## Canonical layers (GitHub view)

- `client/`
  - `map-suite`, `crashfix`, `repair-tool` source snapshots for ASI tools
- `deploy/`
  - decompilation pipelines, binary analysis tools, and reverse-engineering
    utilities
- `docs/`
  - public-facing documentation, build notes, and integration guides
- `research/`
  - research findings, evidence standards, and index assets
- `third_party/`
  - embedded plugin-sdk and related external code required for ASI work
- repository root files
  - licensing, contributing guidance, and build entry scripts

## Scope boundaries

This repo intentionally stays separate from private runtime deployment secrets,
live server databases, and private build history. Those parts remain in the
private SP-RP server repository.

## Why names changed

Public documentation and tooling now reference canonical directory names to match
the GitHub structure: `map-suite`, `crashfix`, `repair-tool`. Old aliases are
not kept, and no compatibility redirections are relied on in this repository.

## Organization plan

- Use canonical path names above for day-to-day work.
- Use local workspace grouping outside this repo only if you want cleaner local
  navigation.
