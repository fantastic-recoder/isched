# SPDX-License-Identifier: MPL-2.0
# syntax=docker/dockerfile:1.7

FROM fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88 AS build
ARG ISCHED_SOURCE_REVISION
ARG ISCHED_PACKAGE_VERSION
ARG ISCHED_OS_PACKAGES
WORKDIR /src
COPY . /src
RUN dnf install -y --setopt=install_weak_deps=False ${ISCHED_OS_PACKAGES} \
    && dnf clean all
RUN python3 packaging/scripts/build_target.py \
    --target fedora-45 \
    --revision "${ISCHED_SOURCE_REVISION}" \
    --version "${ISCHED_PACKAGE_VERSION}" \
    --output-dir /out

# Runtime verification is based on the same immutable target image and does not
# receive the compiler, CMake, Ninja, Node.js, pnpm, Conan, or producer tree.
FROM fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88 AS runtime-verify
RUN dnf install -y --setopt=install_weak_deps=False python3 ca-certificates \
    && dnf clean all
WORKDIR /opt/isched
COPY packaging/targets.json packaging/targets.json
COPY packaging/scripts/package_identity.py packaging/scripts/verify_runtime.py packaging/scripts/
COPY --from=build /out/ /tmp/packages/
RUN set -- /tmp/packages/isched-runtime-*.rpm \
    && test "$#" -eq 1 \
    && python3 packaging/scripts/verify_runtime.py --target fedora-45 \
         --package "$1" --in-container --evidence /tmp/runtime-verification.json

# The SDK verifier receives a clean target package installation plus pinned OS
# compiler tooling; it copies only packaging helpers and the external fixture.
FROM fedora:45@sha256:53d138f25f6738257c78553d51369b662efc0e0eb1cd1556a2fe0ad5a115be88 AS sdk-verify
ARG ISCHED_OS_PACKAGES
RUN dnf install -y --setopt=install_weak_deps=False ${ISCHED_OS_PACKAGES} \
    && dnf clean all
WORKDIR /opt/isched
COPY packaging/targets.json packaging/tool-versions.json packaging/requirements-bootstrap.lock packaging/requirements-conan.lock packaging/
COPY packaging/scripts/package_identity.py packaging/scripts/bootstrap_tools.py packaging/scripts/verify_sdk.py packaging/scripts/
COPY tests/package/sdk-consumer/ consumer/
COPY --from=build /out/ /tmp/packages/
RUN set -- /tmp/packages/isched-devel-*.rpm \
    && test "$#" -eq 1 \
    && python3 packaging/scripts/verify_sdk.py --target fedora-45 \
         --package "$1" --consumer /opt/isched/consumer --in-container \
         --evidence /tmp/sdk-verification.json

# BuildKit's local exporter exposes only the prevalidated package set and its
# machine-readable evidence; neither verifier image's incidental files escape.
FROM scratch AS export
COPY --from=build /out/ /
COPY --from=runtime-verify /tmp/runtime-verification.json /runtime-verification.json
COPY --from=sdk-verify /tmp/sdk-verification.json /sdk-verification.json
