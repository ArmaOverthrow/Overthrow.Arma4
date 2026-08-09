# Release Automation — Requirements

**Epic:** dev-ops
**Created:** 2026-08-01

## Overview

Automates packing and publishing Overthrow to the Arma Reforger Workshop from the command line, using the Workbench's `-packAddon` / `-publishAddon*` flag family. Removes the manual GUI publish step from releases and makes version and changelog handling reproducible.

Sequenced last in the epic: it is the smallest feature, depends only on feature #1, and is the only one that acts **outward** — it publishes to a live Workshop listing that real players consume. That asymmetry drives most of the requirements below.

## Requirements

- **Verify the publish flags empirically first.** The family read from the binary is `-packAddon`, `-packAddonDir`, `-packAddonCacheDir`, `-publishAddon`, `-publishAddonDir`, `-publishAddonCacheDir`, `-publishAddonVersion`, `-publishAddonChangeNote`, `-publishAddonChangeNoteFile`, `-publishAddonPreviewImage`, `-publishAddonScreenshots`. **None has been executed.** Establish actual behaviour, required arguments and auth model before designing around them. Where possible, verify against a non-production target before touching the live listing.
- **Packing is separable from publishing.** It must be possible to pack and inspect the result without publishing anything.
- **Publishing is never automatic.** It is an explicit, deliberate, human-triggered action. CI (feature #4) must not be able to publish — not on merge, not on tag, not on any branch.
- **Version and changelog are supplied non-interactively**, from the repository rather than typed by hand, so a release is reproducible from a commit.
- **A dry-run / preview path exists** showing exactly what would be published — version, change note, files — before anything is sent.
- **Publish credentials are never committed** and never written into workflow files.
- **The published addon is verified before release**: it must have compiled and passed tests. Publishing an untested build is a worse failure than publishing manually.
- **The process is documented well enough to be run by someone other than its author**, including how to recover from a bad publish.
- **Reuses feature #1's launcher and path translation.** No second Workbench-invocation implementation.

## Definition of Done — documentation

- `docs/technical-design.md` — add a release/distribution section (currently the stack table names the Workshop but no release process is documented)
- `README.md` — maintainer-facing release steps
- `CLAUDE.md` — note that publishing is human-triggered only and out of the agent's remit

## Dependencies

- **`dev-ops/workbench-automation`** — the Workbench launcher and WSL↔Windows path translation
- Workshop publishing credentials for the Overthrow listing (`59B657D731E2A11D`)
- Maintainer authority to publish — this feature must not widen who can release
- Independent of features #2, #3 and #4; only sequenced after them by priority

## Out of Scope

- **Automatic publishing from CI.** Explicitly excluded, permanently — see requirements above.
- **Release-notes generation from commit history.** Changelog content is authored by a human; this feature only transports it.
- **Version-number policy.** How versions are chosen is a project decision, not this feature's.
- **Publishing to anywhere other than the Reforger Workshop.**
- **Rollback automation.** Recovery from a bad publish is documented as a manual procedure, not automated.
- **Managing Workshop metadata** — description, tags, preview images — beyond what the publish flags require per release.
