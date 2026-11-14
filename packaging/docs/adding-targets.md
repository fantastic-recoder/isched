# Adding a Linux package target

Targets are explicit, immutable inputs—not distribution-name heuristics. The production set in
`packaging/targets.json` intentionally remains Ubuntu 26.04 and Fedora 45. An experiment belongs in
a separate target-set file, such as `tests/package/fixtures/additional-target.json`, and is passed
explicitly to each packaging helper with `--targets`.

## Compatibility and pin checklist

Before adding a target record, independently verify and record:

1. **Release and base image**: exact distribution release, `linux/amd64` platform, and the registry
   manifest digest for the matching image. Use `docker manifest inspect` and then pull/run that
   digest; never use a floating tag or substitute a nearby release when a digest is unavailable.
2. **Native build inputs**: query the target's native repositories from that image and pin every
   required OS package to its exact version (DEB package versions or RPM NEVRAs). Confirm GCC and
   G++ packages, executable paths, package version, and compiler major agree.
3. **Python and locked tools**: verify the image Python minor matches the entry in
   `packaging/tool-versions.json`. The Node.js, pnpm, Conan, CMake, Ninja, pip requirements and
   Conan dependency lock inputs remain checksum/version/lockfile controlled; do not add target
   exceptions that silently weaken those checks.
4. **C++ ABI/profile**: confirm C++23, `libstdc++11`, the target compiler version and executable
   paths. `package_identity.conan_profile()` derives both producer profiles and the installed SDK
   profile from these validated compiler fields.
5. **Native format/handler**: select an explicitly supported `ubuntu`/DEB or `fedora`/RPM Dockerfile
   family. Unsupported distributions, architectures, package formats, and builder families fail
   validation; no automatic substitutions are performed.
6. **Independent acceptance**: run the complete target build and nonempty CTest, package payload and
   native metadata validation, clean runtime install/smoke test, and isolated developer SDK
   dependency resolution plus external compile/link/run. Review notices, package identity and
   checksums. Pin inspection, schema tests, and an image-only smoke test do not establish full
   compatibility.

## Isolated experiment

Add the candidate record to a separate JSON file containing the normal schema/role map and the
production entries plus the candidate. Keep `packaging/targets.json` unchanged. The Dockerfile is
selected by the validated target family, while the helper passes the record's immutable image,
target ID, OS pins and target-set path as build arguments. Runtime and SDK verifier stages receive
that same selected target set. Build and verify with a clean committed checkout and a fresh output
directory:

```sh
python3 packaging/scripts/package_identity.py \
  --targets tests/package/fixtures/additional-target.json --matrix
python3 packaging/scripts/build_local.py \
  --target fedora-44 \
  --targets tests/package/fixtures/additional-target.json \
  --revision "$(git rev-parse HEAD)" \
  --output cmake-build-debug/us3-fedora-44
```

The output is inside the Docker-ignored build tree so its local BuildKit staging directory cannot
become part of the source context. The build command rejects dirty checkouts and refuses to
overwrite an existing output directory.
Do not bypass either check or manufacture a source revision. Confirm both roles and all three
verification evidence files are emitted under the new target identity. Re-run the production
identity tests and compare the default two-entry matrix before and after the experiment. Never
temporarily overwrite the production target file, and do not leave an experimental target in its
default matrix.

`fedora-44` in the fixture is an experimental, pinned candidate. On 2026-11-10, Docker inspection
confirmed the Fedora 44 amd64 image digest, GCC `16.2.1-3.fc44`, Python `3.14.10-1.fc44`, the exact
NEVRAs in the fixture, and `/etc/os-release` support end `2027-05-19`; installing the pinned OS
package set in a disposable container completed successfully. This pin/tool-install check is not
evidence that the full package, runtime, or SDK pipeline has passed. Keep the corresponding
validation task pending until the isolated end-to-end Docker run actually succeeds, and record its
revision, commands, package identities, evidence, and any blocker in the feature's US3 validation
record.
