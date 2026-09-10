# SP-RP Public Research

The [GTA plugin-sdk guide](docs/PLUGIN_SDK.md) explains our pinned DK22Pac/plugin-sdk dependency, initialization and source layout. Clone with `--recurse-submodules` to include its source.

A community workshop for GTA: San Andreas, SA-MP and S&SMP interoperability research, reusable tools, and Valkyrie's released mods.

Everyone can propose improvements through a pull request. Maintainers review changes before merging. This repository has no connection or deployment credentials to the live SP-RP server.

Original SP-RP code is offered under [BSD-3-Clause](LICENSE), which permits use in private projects with the required notices. Third-party components retain their own licenses; the decompilation archive is not relicensed as SP-RP-owned code. See [credits and licensing](THIRD_PARTY_NOTICES.md).

## Start here

- **AI agents: read [AGENTS.md](AGENTS.md) and [the agent start guide](docs/AGENT_START.md) first.**
- [ASI builder and compiler setup](docs/BUILDING.md) — `./build.ps1 -CheckEnvironment`, then `./build.ps1 -Release`.
- [Mod source and build guide](docs/MODS.md)
- [Full decompilation archive and coverage](docs/reverse-engineering/README.md)
- [Research index and evidence standards](research/README.md)
- [How to contribute](CONTRIBUTING.md)
- [How accepted changes reach a private server](docs/INTEGRATION.md)
- [Credits and licensing](THIRD_PARTY_NOTICES.md)

## Included

| Project | Source |
| --- | --- |
| Valkyrie Map and Doctor Valkyrie | [client/sprp-blips](client/sprp-blips) |
| Valkyrie Crashfix 2.3.0-test | [client/valkyrie-crashfix](client/valkyrie-crashfix) |
| Valkyrie Repair 1.2 | [client/valkyrie-repair](client/valkyrie-repair) |
| Binary-analysis and pedestrian-path research tools | [deploy](deploy) |

The mod folders contain development source snapshots; versions and build instructions are in the mod guide. The separate [decompilation archive](docs/reverse-engineering/README.md) contains the actual generated research output, with exact binary hashes, recovery counts and file checksums.

The public research collection includes full generated decompilation and disassembly for GTA SA 1.0 US, PECore, and S&SMP client/server, plus reconstructed pseudocode, symbols, strings, call maps and export tooling. The private gamemode, player databases, server configuration, credentials, game assets, executable inputs and private Git history remain excluded.

This is an unofficial community project, not affiliated with Rockstar Games, Take-Two, SA-MP, S&SMP or Project Eagle. Obtain game prerequisites separately.

Unreleased Valkyrie Radar work is private and is not included in this repository.
