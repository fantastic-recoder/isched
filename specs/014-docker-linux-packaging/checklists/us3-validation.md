# US3 additional-target validation

Status: Fedora 44 pinned candidate; full pipeline pending a clean integration
commit.

`tests/package/fixtures/additional-target.json` adds Fedora 44 without modifying
`packaging/targets.json`. Initial target records have immutable snapshot tests;
the default matrix remains exactly Ubuntu 26.04 and Fedora 45.

On 2026-11-10, Docker manifest/image/package inspection and disposable pinned
OS-package installation passed for the Fedora 44 candidate. This is not a
package/runtime/SDK pass.

Run from the clean committed checkout:

```sh
python3 packaging/scripts/build_local.py --target fedora-44 \
  --targets tests/package/fixtures/additional-target.json \
  --revision "$(git rev-parse HEAD)" \
  --output cmake-build-debug/us3-fedora-44
```

Record the actual revision, full Release CTest count, both native identities,
runtime/SDK evidence, package hashes, and initial identity comparison after
execution. Keep T043 pending until all these checks pass.
