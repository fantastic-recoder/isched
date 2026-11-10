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
