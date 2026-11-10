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

## Local resource-bound correction — 2026-11-10

Before attempting T030 from checkout
`10ae436b212042023ece07526b072f04d014313e`, inspection found unrestricted
producer CMake jobs, two SDK build jobs, and independently scheduled runtime/SDK
Docker verification stages. Corrective T006/T020/T023/T024/T025 changes now use
explicit `-j1` for producer/SDK builds and target CTest, Conan
`tools.build:jobs=1` in both dependency contexts, one frontend test/build worker,
and a runtime-evidence dependency before SDK verification.

TDD regression tests failed before the fix, then passed all 8 focused tests.
With `/usr/bin/python3` 3.14:

```sh
cd /home/groby/dev/isched
CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 -m unittest discover -s tests/package -p 'test_*.py' -v
cd /home/groby/dev/isched/cmake-build-debug
CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1 ctest --output-on-failure
```

Results: 71 helper tests, OK with 4 environment-dependent skips (two missing
Debian-tool fixtures and two opt-in container integrations); all 61 Debug CTest
entries passed, 0 failures, 154.71 seconds. Native RPM fixtures ran; target
identity snapshots and ordinary configurator defaults passed. These host checks
are not completed target-container evidence. Host Docker is 29.9.2, Buildx
0.38.1; `dpkg-deb` is absent, so host-native Ubuntu export inspection still needs
a safe tooling solution. No system package installation was performed.

T030 remains unchecked until both real local target pipelines and aggregate
validation pass from a clean committed source revision.

## T030 local Fedora attempt — stopped at frontend command parsing

Executed serially on 2026-11-10 from the clean correction commit:

```sh
cd /home/groby/dev/isched
CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 packaging/scripts/build_local.py --target fedora-45 \
  --revision f910b252b0791a4840811b734b8175003e03c712 \
  --output /home/groby/dev/isched/cmake-build-debug/package-validation-f910b252/fedora-45
```

Exit status: **1**. Pinned Fedora OS packages, tool bootstrap, and frozen pnpm
install succeeded. `frontend-test` failed with
`Error: Unknown argument: maxWorkers`; Angular CLI requires kebab-case
`--max-workers=1` even though its builder schema spells the property
`maxWorkers`. The outer wrapper reports `stage=docker-build`; the inner
target-qualified log identifies `stage=frontend-test`.

No final packages were exported. Producer C++ build, full target Release CTest,
native validation, clean runtime and SDK gates were not reached. T030 and
dependent task execution remain incomplete; no other Docker target was started.

The narrow repair uses `--max-workers=1`. Two new tests cover the constructed
command and actual installed pnpm/Angular `--help` parsing without executing UI
tests; the focused six-test suite passes. Schema-only inspection is no longer
the compatibility proof. This repair still needs a clean-revision Docker rerun;
it is not a successful Fedora package result.

After the repair, the same full helper command above ran 73 tests in 5.373s,
OK with the same four environment-dependent skips. The exact mandatory Debug
CTest command above passed all 61/61 entries with no exclusions or failures
in 154.46 seconds before the repair commit. These results validate the host
regressions only; no successful local target pipeline is claimed.
