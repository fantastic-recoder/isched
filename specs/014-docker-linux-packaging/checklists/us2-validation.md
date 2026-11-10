# US2 hosted artifact validation

Status: pending first upload-enabled hosted run.

The earlier successful run 42707740390 has zero artifacts and is not download
evidence. The new workflow stages both roles only after producer, full CTest,
native, runtime and SDK gates. Runtime/SDK evidence is bound to package bytes;
staging checks checksums and the actual shipped SDK lockfile.

Required passing-run record: URL, actual checkout SHA, four distinct artifact
names, package/metadata contents, SHA-256 comparison and native target/role
inspection. Required controlled-failure record: URL, failed target/stage,
independent passing target, exactly its two role artifacts, no failed-target
role artifact, and overall failed workflow conclusion.

Do not treat static workflow tests or successful upload steps alone as download
verification. Downloads are unsigned validation outputs, not release assets.
