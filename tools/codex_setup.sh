#!/usr/bin/env bash
# restore_latest_install.sh
set -euo pipefail

WORKFLOW_FILE="${WORKFLOW_FILE:-ci.yml}"
BRANCH_NAME="${BRANCH_NAME:-master}"
ARTIFACT_NAME="${ARTIFACT_NAME:-parasol-install-ubuntu-latest-Release}"
DEST_DIR="${DEST_DIR:-install/agents}"

if ! command -v gh >/dev/null 2>&1; then
   echo "error: GitHub CLI (gh) not found; install it and run 'gh auth login' first." >&2
   exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
   echo "error: gh is not authenticated. Run 'gh auth login' with a token that has actions:read scope." >&2
   exit 1
fi

run_id="$(gh run list \
   --workflow "${WORKFLOW_FILE}" \
   --branch "${BRANCH_NAME}" \
   --event push \
   --status success \
   --limit 1 \
   --json databaseId \
   --jq '.[0].databaseId')"

if [[ -z "${run_id}" ]]; then
   echo "error: no successful runs found for ${WORKFLOW_FILE} on ${BRANCH_NAME}." >&2
   exit 1
fi

echo "Using workflow run ${run_id}"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

# download extracts the artifact straight into tmp_dir/ARTIFACT_NAME
gh run download "${run_id}" \
   --name "${ARTIFACT_NAME}" \
   --dir "${tmp_dir}"

tarball="$(find "${tmp_dir}" -type f -name '*.tar.gz' -print -quit)"
if [[ -z "${tarball}" ]]; then
   echo "error: no .tar.gz found in downloaded artifact; verify the CI packaging step." >&2
   exit 1
fi

mkdir -p "${DEST_DIR}"
rm -rf "${DEST_DIR:?}/"*

tar -xzf "${tarball}" -C "${DEST_DIR}"

echo "Restored ${ARTIFACT_NAME} into ${DEST_DIR}"
