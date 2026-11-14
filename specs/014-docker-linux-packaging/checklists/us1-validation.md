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

## T030 resumed local target execution — Fedora passed, Ubuntu pin blocked

Both commands below used the clean actual checkout
`565146df4c070eda07d492df980ddcd2729ed688` on 2026-11-10.
No target builds or test runners overlapped; producer/SDK/Conan/frontend
single-job bounds remained in effect.

```sh
cd /home/groby/dev/isched
CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 packaging/scripts/build_local.py --target fedora-45 \
  --revision 565146df4c070eda07d492df980ddcd2729ed688 \
  --output /home/groby/dev/isched/cmake-build-debug/package-validation-565146df/fedora-45
```

**Fedora exit 0**. Pinned Fedora 45/GCC 16.2.1-2.fc45.1, Conan 2.34.0,
Node 24.22.2, pnpm 10.30.3 and Conan CMake 3.22.6/Ninja 1.12.1
inputs passed the actual bootstrap/build path. Fresh frontend verification
passed 33 suites/238 tests, production build and 27-asset embed. Full target
Release CTest passed **61/61**, 0 failures. Notice collection, CPack, both-role
native validation, clean runtime, external SDK and host-native post-export
validation all passed. Exported JSON evidence and both RPMs are retained at
the output directory above.

Native inspection (`rpm -qp --qf '%{NAME} %{VERSION} %{RELEASE} %{ARCH}\n'`)
confirmed `isched-runtime` and `isched-devel`, version `0.0.0`, release
`0.git.565146df4c07`, architecture `x86_64`. Canonical identity is
`0.0.0+git.565146df4c07` with the full source SHA above and target `fedora-45`.
`rpm -qp --requires` showed native libc/libm/libgcc_s/libstdc++/loader
requirements; clean target loader resolution passed.

| Exported role | SHA-256 of actual RPM |
|---|---|
| runtime | `a2f073a29b24d95b8e0a86a951e7ea76735190061e86cbfac6bb26de091d95a8` |
| developer | `d57c5d5d72863ebee82a11eceff3acea97bb167c6817eb8f86cf891a2638f039` |

`runtime-verification.json` confirms resolved loader, no seeding,
GraphQL health `UP`, embedded `/isched` HTML and
`/isched/styles-25ORBMRP.css` (80,894 bytes).
`sdk-verification.json` confirms GCC 16, external consumer
`configured-built-linked-ran`, no runtime package, shipped Conan lock hash
`6de84c68c10906a4ba50da5e66729d3370f5f2951ddb002474640f588259e5b7`.

### Isolated native-inspection tooling, not target evidence

Host `dpkg-deb` is still not installed. An ignored local Python adapter at
`/home/groby/dev/isched/cmake-build-debug/package-native-tools/dpkg-deb`
invokes the real native `dpkg-deb` 1.23.7 from the exact pinned Ubuntu image.
It allows only metadata queries for Package/Version/Architecture/Depends
and filesystem tar reads. Each invocation mounts only the exact package
read-only, uses nobody UID, no network, read-only root, dropped capabilities,
no-new-privileges and a bounded 64 MB noexec tmpfs; no Docker socket mount,
host package install, SELinux disablement or real package relabeling.

The adapter was manually reviewed and tested before Ubuntu execution:
three tests passed in 2.241s after expected missing-adapter red, bounded
scratch correction, and matching only the synthetic test-owned fixture's
SELinux label to actual BuildKit-exported package labels. Tests cover strict
read operations/isolation, native failure propagation, actual native
metadata, binary tar contents and malformed archive rejection. These are
tooling tests, not successful Ubuntu package or target acceptance evidence.

### Ubuntu unavailable pinned input — stop

```sh
cd /home/groby/dev/isched
PATH="/home/groby/dev/isched/cmake-build-debug/package-native-tools:$PATH" \
  CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 packaging/scripts/build_local.py --target ubuntu-26.04 \
  --revision 565146df4c070eda07d492df980ddcd2729ed688 \
  --output /home/groby/dev/isched/cmake-build-debug/package-validation-565146df/ubuntu-26.04
```

**Ubuntu exit 1**. Docker `[build 5/6]` OS package installation exited 100:
`E: Version '5.40.1-7ubuntu0.3' for 'perl' was not found`.
The shared wrapper propagated target-qualified failure; other BuildKit stages
were canceled. No frontend, producer, Release CTest, native package, runtime,
SDK or final export gate was reached. The adapter was not yet invoked by
this target build. The final Ubuntu output directory is absent.

No versions/releases/repositories were silently changed. T030 remains
incomplete pending approved pin/repository-retention recovery, a successful
Ubuntu run and aggregate validation; T031/T032 and later phase-dependent
work remain halted. The aggregate command and current host helper/full Debug
CTest were not rerun after this target failure. Prior host results above
remain historical, not new results. No hosted work, push or host installation
was performed, and no new task was checked or commit created by this resumption.

## Approved Perl-only recovery — 2026-11-10

The user approved verifying available Ubuntu versions and changing **only**
the unavailable Perl pin if compatible, stopping if broader changes are needed.
Queries used the actual unchanged pinned Ubuntu image
`ubuntu:26.04@sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725`
and its unchanged `ubuntu.sources`: archive.ubuntu.com resolute,
resolute-updates, resolute-backports and security.ubuntu.com resolute-security.

After `apt-get update`, `apt-cache policy` / `apt-cache madison perl`
showed `5.40.1-7ubuntu0.4` in updates/security, `5.40.1-7build1` in
the base archive, and no downloadable old `.3` Perl. Image-installed
`perl-base` was `.3`. A first capability-dropped query failed before index
fetch because apt could not switch to `_apt`; repeating with ordinary Docker
capabilities and `no-new-privileges` succeeded. No privileged mode, source
mount, source-list change or host package installation was used.

The full 24-entry `os_packages` list, with only the in-memory Perl replacement,
passed `apt-get --simulate install -y --no-install-recommends "$@"`.
A second fresh disposable same-image container performed the actual install.
`dpkg-query -W -f='${Version}'` asserted **all 24 exact requests** after install:
GCC/G++ stayed `15.2.0-16ubuntu1`; all other explicit package pins were retained.
No removals or broader pin substitutions were needed. Normal dependency
resolution included:

| Dependency | Resolved version |
|---|---|
| perl, perl-base, perl-modules-5.40, libperl5.40 | `5.40.1-7ubuntu0.4` |
| libgdbm6t64, libgdbm-compat4t64 | `1.26-1build1` |
| m4 | `1.4.21-1` |
| libdpkg-perl | `1.23.7ubuntu1` |
| liberror-perl | `0.17030-1` |

Unpinned Python interpreter dependencies resolve `3.14.4-1ubuntu0.2`, while
the explicitly pinned python3/python3-venv metapackages remain
`3.14.3-0ubuntu2`. Perl JSON::PP smoke passed at `v5.40.1`; autoconf 2.72
and automake 1.18.1 executed. This establishes narrow install compatibility,
not yet complete Ubuntu producer/SDK acceptance.

Only production `packaging/targets.json` Perl changed `.3`→`.4`, with the
same duplicated Ubuntu fixture pin and immutable Ubuntu snapshot test updated.
Fedora's target snapshot remains unchanged; no compiler/base/release/tool,
other explicit OS pin, repository source or concurrency default changed.
Snapshot TDD ran before the pin edit: six tests, one expected mismatch;
after the edit all six passed.

```sh
cd /home/groby/dev/isched
CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 -m unittest tests.package.test_target_extension -v
CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 -m unittest discover -s tests/package -p 'test_*.py' -v
cd /home/groby/dev/isched/cmake-build-debug
CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 ctest --output-on-failure
```

Helper result: **73 tests in 5.327s, OK, four environment-dependent skips**
(host Debian fixture tools absent, runtime/SDK integration opt-in).
Mandatory full Debug gate: **61/61, 0 failures, 155.50s**, no exclusions.
All runs serial; explicit one-job packaging helpers retain the safer lower
bound despite the session's maximum-three allowance. These host results
permit the corrective clean-source commit, not T030 completion. Both target
pipelines must be rerun at that same new clean SHA and aggregate-validated.
Exact probe commands/results and temporary log paths are retained in the
absolute durable implementation recovery protocol.

## T030 passed — same corrected clean source for both local targets

Source **`410fd0342dbc9f59f7556c0ff8c4b33231182a95`**, canonical version
`0.0.0+git.410fd0342dbc`. Corrective commit changed only the approved Perl
production pin and directly coupled tests/evidence. Both real pipelines ran
serially from this clean checkout with fresh ignored output directories:

```sh
cd /home/groby/dev/isched
PATH="/home/groby/dev/isched/cmake-build-debug/package-native-tools:$PATH" \
  CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 packaging/scripts/build_local.py --target ubuntu-26.04 \
  --revision 410fd0342dbc9f59f7556c0ff8c4b33231182a95 \
  --output /home/groby/dev/isched/cmake-build-debug/package-validation-410fd034/ubuntu-26.04
CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 packaging/scripts/build_local.py --target fedora-45 \
  --revision 410fd0342dbc9f59f7556c0ff8c4b33231182a95 \
  --output /home/groby/dev/isched/cmake-build-debug/package-validation-410fd034/fedora-45
PATH="/home/groby/dev/isched/cmake-build-debug/package-native-tools:$PATH" \
  CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 tools/package/validate_artifacts.py \
  --ubuntu /home/groby/dev/isched/cmake-build-debug/package-validation-410fd034/ubuntu-26.04 \
  --fedora /home/groby/dev/isched/cmake-build-debug/package-validation-410fd034/fedora-45 \
  --revision 410fd0342dbc9f59f7556c0ff8c4b33231182a95
```

**All three commands exited 0**. Each wrapper used `--pull --no-cache` and
performed bootstrap, frozen frontend install, fresh frontend tests/build/embed,
producer Release build, full nonempty CTest, notice collection, CPack,
both-role native validation, clean runtime and external SDK verification,
export and host-native post-export validation. Existing explicit `-j1` /
Conan jobs1 / frontend worker limits remained unchanged; env3 did not
override those lower limits. No simultaneous target/heavyweight test runners.
The reviewed ignored adapter was reread and its three tests rerun successfully
in 2.173s before PATH use; host packages were not installed.

| Target | Frontend | Release CTest | Native roles/version/architecture |
|---|---|---|---|
| Ubuntu 26.04 | 33 suites / 238 tests, 27 assets | 61/61, 0 failures, 145.09s | `isched-runtime`, `isched-devel`; `0.0.0+git.410fd0342dbc`; `amd64` |
| Fedora 45 | 33 suites / 238 tests, 27 assets | 61/61, 0 failures, 146.47s | `isched-runtime`, `isched-devel`; version `0.0.0`, release `0.git.410fd0342dbc`; `x86_64` |

Pinned compiler inputs remain Ubuntu GCC/G++ `15.2.0-16ubuntu1` and Fedora
`16.2.1-2.fc45.1`. Both bootstrapped Conan 2.34.0, Node 24.22.2, pnpm 10.30.3
and Conan CMake 3.22.6/Ninja 1.12.1 without substitution.

| Target / role | Actual final-package SHA-256 |
|---|---|
| Ubuntu runtime | `5f88ba53226f9c9d609b027c16c01f7dfc5559428f5b7063e203ae098c7a6c19` |
| Ubuntu developer | `b419e3856a3e290ff56b1851fd821811348fe50493c6397bf5fe0090a84923ec` |
| Fedora runtime | `a9afa1160c2a64ebe1eb87c3c0661545f63883c023c97969551d6194a9f07c10` |
| Fedora developer | `07c60543f4dbbefed445d8fa2d7c62285698848ab2b0425f4530a303d357d1c4` |

Native inspection used adapter `dpkg-deb -f FILE FIELD` for
Package/Version/Architecture/Depends, `rpm -qp --qf
'%{NAME} %{VERSION} %{RELEASE} %{ARCH}\n'`, `rpm -qp --requires`, and
`sha256sum`. Ubuntu runtime declares `libc6 (>= 2.38), libgcc-s1 (>= 4.3),
libstdc++6 (>= 14)`. Fedora auto-requires libc/libm/libgcc_s/libstdc++/loader
ELF capabilities. Neither SDK invents an executable-package dependency.

Both `runtime-verification.json` files confirm loader `resolved`, `seeded:
false`, GraphQL health `UP`, embedded `/isched` UI and
`/isched/styles-25ORBMRP.css` **80,894 bytes**. Both
`sdk-verification.json` files confirm their respective compiler 15/16,
`configured-built-linked-ran`, `runtime_package: false`, and shipped Conan
lock SHA-256 `6de84c68c10906a4ba50da5e66729d3370f5f2951ddb002474640f588259e5b7`.
Build/runtime/SDK JSON and exactly two native packages remain in each
directory above. Aggregate validator rechecked all four roles successfully.

T030 is now complete. T031 negative evidence and T032 authorized hosted
failure-isolation evidence are separate gates; no hosted trigger/download,
push, extra target, human selection, final feature closure or merge is claimed.

After recording T030/T031 evidence, the mandatory full Debug command above
was rerun before the evidence-only task commit: **61/61, 0 failures, 155.97s**.
No exclusions or concurrent test/build runners. Target package identity
remains the actual tested clean `410fd0342dbc9f59f7556c0ff8c4b33231182a95`;
a later evidence-only commit is not a relabeling or rebuilt source claim.
