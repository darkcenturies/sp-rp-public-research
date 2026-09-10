# SP-RP public-research repository structure

This repository is the public companion repository to the SP-RP server project.

## Canonical layers (GitHub view)

- `client/`
  - `sprp-blips`, `valkyrie-crashfix`, `valkyrie-repair` source snapshots for
    ASI tools
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

## Why names stay

Public documentation and tooling reference these names directly. Keeping
`sprp-blips`, `valkyrie-crashfix`, and `valkyrie-repair` unchanged keeps
release notes, tests, and historical archives aligned.

## Non-breaking organization plan

- Keep canonical path names above for day-to-day work.
- Use local workspace grouping outside this repo only if you want cleaner local
  navigation.
