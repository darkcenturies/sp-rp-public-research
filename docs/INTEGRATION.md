# Using public improvements in a private server

Public work follows fork → pull request → review/checks → main → versioned release.

The project uses BSD-3-Clause for original SP-RP contributions. Such code can be incorporated into a private gamemode while retaining required attribution and notices. This does not apply automatically to every file: Crashfix remains GPL-3.0, diagnostic resources retain their terms, and vendor-derived decompilation is not licensed under BSD. Check the specific component before importing it.

The private server consumes only the shared files it needs at an exact reviewed tag or commit. Make the version update in the private repository, test it on staging, and deploy using that repository's release process. Preserve licenses when incorporating a component.

Do not merge the private gamemode into this repository or push its branches/history here. Do not install a production runner, credentials or deployment webhook in this public repository. No automatic synchronization or live deployment has been configured.

Record each imported public version in the private repository so it can be upgraded or rolled back. Changes intended for public reuse should be developed here and then consumed privately.
