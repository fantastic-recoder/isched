# Implementation review

Review date: 2026-11-10. Scope: package scripts, CMake/SDK exports,
`configure.py`, Docker stages and GitHub workflow. Final integrated regression
and hosted publication checks remain required after the current changes.

## Confirmed controls

- Package identity, target validation and native archive checks are shared
  rather than duplicated in workflow steps. Argument-list subprocess calls
  and explicit stage-labelled errors preserve failure attribution.
- CPack installs only runtime/SDK components. Native inspectors reject wrong
  format, ELF architecture, source/role identity, unwanted state, missing
  notices and producer/cache export paths.
- Runtime checks create disposable state and drop privileges. Origin handling
  is confined to the probe; application authentication/API/UI behavior was not
  changed. SDK checks resolve installed inputs with a fresh Conan cache and
  execute a real out-of-line symbol without installing the runtime package.
- Native fixture C code has no ownership-bearing allocations or manual
  resource cleanup. Product C++23 smart-pointer/RAII behavior is unchanged.
- Distro compilers, Python/venv, native package/ELF tools and Docker are boundary
  dependencies, not replacements for Conan-managed library dependencies.
- No shared writable build cache, release publication, signing credentials,
  host native installation, seeded state or privileged artifact consumer.

## Findings and resolutions

| Finding | Resolution |
|---|---|
| Generated frontend notice path differed from CMake gate | Root-output notice path unified and regression-tested |
| CPack private staging leaked into export | Copy only expected final native package files |
| Human-readable DEB listing treated as paths | Read tar member metadata directly |
| RPM hardlink members could lack data | Resolve by device/inode group before checksum checks |
| Same-named Conan notices overwrote one another | Preserve package/source/recipe namespaces |
| Test-only notice subtrees violated payload contract | Exclude test/tests, retain nested production notices |
| Fixture assumed system `/bin/true` and unversioned compiler | Compile executable with explicit compiler selection |
| Verifier confused native package name with artifact role | Match canonical developer filename, test against identities |
| Clean SDK dependency builds lacked activated CMake | Install/activate tools before dependencies and preserve isolated environment |
| Runtime health timeout hid HTTP origin rejection | Exact disposable loopback allowlist/header, report HTTP errors |
| Consumer omitted Conan Release configuration | Explicit Release configure argument and regression |
| Workflow had no downloadable role outputs | Add SHA-pinned uploads after a fail-closed both-role staging step |
| Verification filenames alone allowed reuse after package bytes changed | Bind runtime/SDK evidence to final package hashes; compare shipped SDK lockfile and require UP health |
| Initial-target IDs were embedded in reusable builders | Pass validated target/image/profile inputs while snapshotting unchanged production records |

This is a correctness/constitution review, not a clean vulnerability-scan
claim. Local clang-tidy `security_scan` remains blocked by absent compilation
database. Artifact integrity/evidence and target extension changes must retain
the above controls through final integration.
