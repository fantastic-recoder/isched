# Failure validation

Host-safe tests exercise invalid internal format, empty/missing/duplicate
outputs, wrong architecture/revision/role/release, notice absence/corruption,
state-contaminated payloads, and target-attributed orchestration failures.
See `tests/package/test_native_packages.py`, `test_build_pipeline.py`,
`test_runtime_install.py` and `test_sdk_install.py`.

The orchestration test injects every pipeline gate failure and requires
nonzero status with no exported packages. Native tests include real CPack
RPM/DEB fixtures when corresponding inspection tools are available.
Mocks establish failure propagation, not target-container compatibility.

Historical hosted failures are independently attributable to target/stage:

| Run | Observed failure |
|---|---|
| [42688923785](https://github.com/fantastic-recoder/isched/actions/runs/42688923785) | Ubuntu fixture compiler discovery; Fedora SDK artifact selector |
| [42694065395](https://github.com/fantastic-recoder/isched/actions/runs/42694065395) | Both SDK dependency builds lacked activated CMake |
| [42698485955](https://github.com/fantastic-recoder/isched/actions/runs/42698485955) | Both runtime probes omitted allowlisted Origin |
| [42702920074](https://github.com/fantastic-recoder/isched/actions/runs/42702920074) | Both SDK consumer configurations lacked Release build type |

These are real regression evidence, not the controlled single-target
failure/publication experiment required by T032/T038. That experiment remains
pending until one target fails while another publishes its validated roles.

## T031 local negative execution — 2026-11-10

Corrected package source:
`410fd0342dbc9f59f7556c0ff8c4b33231182a95`. T030's actual same-source
Ubuntu/Fedora flows and four-package aggregate passed before this run.

```sh
cd /home/groby/dev/isched
CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 /usr/bin/python3 -m unittest \
  tests.package.test_native_packages \
  tests.package.test_build_pipeline.BuildPipelineFailureTests \
  tests.package.test_build_pipeline.PackageGenerationTests.test_missing_or_unexpected_packages_are_not_exported \
  tests.package.test_runtime_install.RuntimeFailureTests \
  tests.package.test_sdk_install.SdkFailureTests -v
```

Exit 0: **16 tests, 3.364s, OK, one host Debian-fixture-tool skip**. Real
CPack RPM notice/identity/dependency tests executed. Native invalid fixtures
were rejected; because some fixture failures may occur before the intended
field check, the explicit same-source acceptance probe below independently
checked each intended diagnostic.

Controlled orchestration injected **14 gates** (image, frontend
install/test/build/embed, Conan/configure/build, CTest/empty CTest,
package generation/validation, runtime install, SDK consumption).
Each returned nonzero with Ubuntu target/stage attribution and **no successful
package export**. Runtime mocks cover six stages for each target
(install/loader/start/health/UI/asset); SDK failure mocks cover both targets.
Missing or unexpected generated role sets leave export empty.
These are controlled unit-level failure injections, not failed real Docker
builds or hosted isolation evidence.

Additional ignored local acceptance harness:

```sh
cd /home/groby/dev/isched
PATH="/home/groby/dev/isched/cmake-build-debug/package-native-tools:$PATH" \
  CMAKE_BUILD_PARALLEL_LEVEL=3 CTEST_PARALLEL_LEVEL=1 \
  /usr/bin/python3 cmake-build-debug/package-native-tools/check_negative_acceptance.py
```

Exit 0: **26 explicit negative cases**, 13 per target, each asserting
nonzero status, target attribution and the intended error text:

| Case per target | Evidence type / actual rejection |
|---|---|
| wrong architecture | Native-decoded metadata injection; `native architecture mismatch` |
| wrong native version | Native-decoded metadata injection; `native version mismatch` |
| wrong role or revision | Decoded embedded identity injection; `role/revision identity mismatch` (two cases) |
| wrong distribution release | Decoded embedded identity injection; `target dimensions mismatch` |
| missing notice | Decoded real notice removal; `notice missing or checksum mismatch` |
| missing runtime payload | Decoded payload path removal; `missing payload` |
| contaminated state | Decoded allowed-prefix SQLite path injection; `forbidden state` |
| missing runtime dependencies | Native-decoded dependency injection; `no declared native dependencies` |
| invented SDK→runtime dependency | Native-decoded dependency injection; `invents isched-runtime dependency` |
| no output | Actual empty temporary directory; `found 0` |
| bad package checksum | Actual temporary evidence changed, original packages referenced read-only; `package checksum mismatch` |
| wrong internal format | Actual invalid temporary bytes under canonical suffixes; native RPM/DEB inspection failed |

The harness first read both roles through real native tools and validated
their original baselines, then injected only decoded representations or
test-owned temporary files. Actual original packages were **not** rewritten
or repacked. Only synthetic files received SELinux labels as needed; no host
tool install or security disablement. Both original target role sets were
revalidated unchanged afterward. The harness is local ignored execution
tooling, not a production source change.

T031 is complete for local negative contracts/propagation. T032/T038 remain
separate: current authorization prohibits new hosted triggers, push or upload,
and no actual controlled single-target hosted run/download evidence was
provided. No hosted failure isolation, human timings, extra target or final
feature completion is inferred from these local results.
