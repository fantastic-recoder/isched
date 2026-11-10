# SPDX-License-Identifier: MPL-2.0
# syntax=docker/dockerfile:1.7

ARG ISCHED_BASE_IMAGE=ubuntu:26.04@sha256:157dc3d6e4a3e4f2c68d12df9df9ed6e808b59af6d4c3757b432b4f519916725
FROM ${ISCHED_BASE_IMAGE} AS build
ARG ISCHED_TARGET=ubuntu-26.04
ARG ISCHED_TARGETS_FILE=packaging/targets.json
ARG ISCHED_SOURCE_REVISION
ARG ISCHED_PACKAGE_VERSION
ARG ISCHED_OS_PACKAGES
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /src
COPY . /src
COPY ${ISCHED_TARGETS_FILE} /src/packaging/targets.json
RUN apt-get update \
    && apt-get install -y --no-install-recommends ${ISCHED_OS_PACKAGES} \
    && rm -rf /var/lib/apt/lists/*
RUN python3 packaging/scripts/build_target.py \
    --target "${ISCHED_TARGET}" \
    --targets /src/packaging/targets.json \
    --revision "${ISCHED_SOURCE_REVISION}" \
    --version "${ISCHED_PACKAGE_VERSION}" \
    --output-dir /out

# Runtime verification is based on the same immutable target image and does not
# receive the compiler, CMake, Ninja, Node.js, pnpm, Conan, or producer tree.
FROM ${ISCHED_BASE_IMAGE} AS runtime-verify
ARG ISCHED_TARGET=ubuntu-26.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/isched
COPY --from=build /src/packaging/targets.json /opt/isched/packaging/targets.json
COPY packaging/scripts/package_identity.py packaging/scripts/verify_runtime.py packaging/scripts/
COPY --from=build /out/ /tmp/packages/
RUN set -- /tmp/packages/isched-runtime-*.deb \
    && test "$#" -eq 1 \
    && python3 packaging/scripts/verify_runtime.py --target "${ISCHED_TARGET}" \
          --targets /opt/isched/packaging/targets.json \
          --package "$1" --in-container --evidence /tmp/runtime-verification.json

# The SDK verifier receives a clean target package installation plus pinned OS
# compiler tooling; it copies only packaging helpers and the external fixture.
FROM ${ISCHED_BASE_IMAGE} AS sdk-verify
ARG ISCHED_TARGET=ubuntu-26.04
ARG ISCHED_OS_PACKAGES
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends ${ISCHED_OS_PACKAGES} \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/isched
COPY --from=build /src/packaging/targets.json /opt/isched/packaging/targets.json
COPY packaging/tool-versions.json packaging/requirements-bootstrap.lock packaging/requirements-conan.lock packaging/
COPY packaging/scripts/package_identity.py packaging/scripts/bootstrap_tools.py packaging/scripts/verify_sdk.py packaging/scripts/
COPY tests/package/sdk-consumer/ consumer/
COPY --from=build /out/ /tmp/packages/
# Keep integration tests serial even when BuildKit schedules independent stages.
COPY --from=runtime-verify /tmp/runtime-verification.json /tmp/runtime-verification.json
RUN set -- /tmp/packages/isched-developer-*.deb \
    && test "$#" -eq 1 \
    && python3 packaging/scripts/verify_sdk.py --target "${ISCHED_TARGET}" \
          --targets /opt/isched/packaging/targets.json \
          --package "$1" --consumer /opt/isched/consumer --in-container \
         --evidence /tmp/sdk-verification.json

# BuildKit's local exporter exposes only the prevalidated package set and its
# machine-readable evidence; neither verifier image's incidental files escape.
FROM scratch AS export
COPY --from=build /out/ /
COPY --from=runtime-verify /tmp/runtime-verification.json /runtime-verification.json
COPY --from=sdk-verify /tmp/sdk-verification.json /sdk-verification.json
