# Initial target validation

Verified 2026-11-10 against successful hosted run
[42707740390](https://github.com/fantastic-recoder/isched/actions/runs/42707740390).
Branch head: `e9f8b0ff22c3e6ab4467e635591837d9fd375d6b`.
Actual PR merge checkout/package revision:
`d7771ccdd9b20bd2e298f173907587a036373b53`.

Both matrix jobs ran `packaging/scripts/build_local.py --target <target>
--revision <checkout-sha> --output dist/<target>`. This invokes the same Docker
path as local builds, with fresh UI install/test/build/embed, pinned tool
bootstrap, Conan install, Release configure/build, full CTest, notices,
CPack, native validation, clean runtime verification and external SDK checks.

| Target | Full Release CTest | Native roles | Clean runtime | External SDK |
|---|---|---|---|---|
| Ubuntu 26.04, x86_64, DEB | 61/61 | runtime + developer validated | passed | configured, built, linked, ran |
| Fedora 45, x86_64, RPM | 61/61 | runtime + developer validated | passed | configured, built, linked, ran |

Runtime verification includes loader resolution, unprivileged fresh/no-seed
startup, GraphQL health, embedded HTML and referenced asset fetch. SDK
verification installs only the developer package, checks runtime absence,
resolves shipped Conan inputs in an isolated cache, and calls out-of-line
library symbols from an external consumer. Source/cache paths in installed
exports are rejected.

Identity examples use version `0.0.0+git.d7771ccdd9b2`, full revision above,
roles `runtime`/`developer`, distro release and `x86_64` in each filename.
Native package names remain `isched-runtime` and `isched-devel`.
Tool inputs are `packaging/tool-versions.json`, `packaging/targets.json`,
Conan lockfiles and target profiles; the successful logs include activated
CMake 3.22.6. Package files were not uploaded by this historical workflow:
GitHub API reports zero artifacts. Download evidence belongs to US2.

Separate host executions of both complete Docker builds were not repeated;
the full pipelines above executed on GitHub runners. This distinction must
not be represented as additional local target-run evidence.
