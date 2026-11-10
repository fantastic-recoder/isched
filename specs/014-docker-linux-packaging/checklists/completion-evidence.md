# Completion evidence

Status: implementation remains active. Successful producer/runtime/SDK jobs
alone do not complete the original specification.

| Requirement | Evidence / remaining gate |
|---|---|
| FR-001, FR-003, FR-004, FR-005, SC-001 | Both initial target pipelines passed run 42707740390; see us1-validation.md |
| FR-002, FR-008 | Canonical identities contain role, distro, release, arch, format and full SHA; hosted downloadable metadata still requires US2 evidence |
| FR-006, SC-002 | Named independent jobs and fail-fast false; controlled single-target failure remains pending |
| FR-007, SC-003 | Artifact staging/upload implementation and real downloads must pass before closure |
| FR-009, SC-004 | Additional-target implementation and full isolated experiment must pass before closure |
| FR-010 | Native checks precede export; both-role staging and hosted failure isolation must be evidenced |
| SC-005 | Four real contributor selections, each correct within 60 seconds, remain pending |

T030 local complete target runs are distinct from the hosted pipeline evidence.
T038 requires passing and controlled-failing hosted artifact runs.
T043 requires a real independently pinned additional-target build, not only
identity fixtures. T048/T049 cannot be closed until these gates are resolved.

User authorized completing the remaining requirements, then archival/merge,
on 2026-11-10. No requirement was waived or silently marked complete.
