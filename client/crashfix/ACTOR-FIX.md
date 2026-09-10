Valkyrie Crashfix 2.2.2

Fixes the investigated special-actor request path that crashed at the installed
Fastman 7.7 beta module +0xAA110 while requesting PSYCHO through opcode 023C.
Eagle registers psycho as model 48876, so it is absent from the special-object
directory even though its DFF and TXD exist in COMPONENTPEDS.img.

The new paired hooks preflight RequestSpecialModel at 0x409D10 and supply its
FLA directory lookup with resolved metadata. Existing directory entries and the
same-name fast path remain supported. Registered models of the same class can
supply their actual streaming metadata; no arbitrary replacement actor is used.
Requests that cannot be resolved return before destructive loader operations.
A caller that indefinitely waits for a genuinely absent asset may still wait;
this does not manufacture missing files or rewrite trainer script behavior.

Compatibility is limited to the verified loader instructions and metadata layout.
Both hooks install together in the existing transaction. Thread-local nested
contexts prevent one request from replacing another request's metadata.

Includes the 2.2.1 specular-material correction, radio fixes and audited guards.
The startup log confirms 34 hooks installed. The installed ASI SHA256 is
B5DF61B7A087683660FFD8BFF8CCA4FDFD22CA859418461F3530C4B2ED081866.

Validation: native actor tests; actual FLA metadata copier emulation using output
from the compiled resolver; 324 normal-path and 45 invalid-path guard cases;
native transaction and radio tests. Startup installation checked successfully.
Gameplay reproduction is pending the user's test. A hidden startup instance
was closed; it did not test actor selection or load a save.

New tests: source/tests/actor_tests.inc and source/tests/test_actor_fla.py.
The native tests produce actor-metadata-fixture.bin in their working directory;
pass it and the installed FLA ASI to test_actor_fla.py.

Previous installed 2.2 binary and log backed up in the game directory under
crashfix-backup-before-2.2.2. The prior release investigation remains historical.
