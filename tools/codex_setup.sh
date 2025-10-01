#!/usr/bin/env bash
# Download and install the Parasol CI artifact for local use.
#
# The script mirrors the behaviour of CI by fetching the latest successful
# workflow run artefact and extracting it into install/agents (by default).

set -euo pipefail

WORKFLOW_FILE="${WORKFLOW_FILE:-ci.yml}"
BRANCH_NAME="${BRANCH_NAME:-master}"
ARTIFACT_NAME="${ARTIFACT_NAME:-parasol-install-ubuntu-latest-FastBuild}"
DEST_DIR="${DEST_DIR:-install/agents}"
GITHUB_HOST="${GITHUB_HOST:-github.com}"
GH_TOKEN=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

install_gh() {
   if command -v gh >/dev/null 2>&1; then
      return
   fi

   export DEBIAN_FRONTEND="noninteractive"
   apt-get update -y
   if apt-get install -y gh; then
      return
   fi

   echo "Configuring GitHub CLI apt repository..."
   apt-get install -y curl ca-certificates gnupg
   install -d -m 0755 /etc/apt/keyrings
   curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
      | gpg --dearmor -o /etc/apt/keyrings/githubcli-archive-keyring.gpg
   chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg
   printf "deb [arch=%s signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main\n" \
      "$(dpkg --print-architecture)" \
      > /etc/apt/sources.list.d/github-cli.list
   apt-get update -y
   apt-get install -y gh
}

ensure_gh() {
   install_gh
   if ! command -v gh >/dev/null 2>&1; then
      echo "error: GitHub CLI (gh) is required but could not be installed." >&2
      exit 1
   fi
}

ensure_auth() {
   if gh auth status --hostname "${GITHUB_HOST}" >/dev/null 2>&1; then
      return
   fi

   if [[ -n "${GITHUB_TOKEN:-}" ]]; then
      printf '%s\n' "${GITHUB_TOKEN}" \
         | gh auth login --with-token --scopes "actions:read" --hostname "${GITHUB_HOST}" >/dev/null
      if gh auth status --hostname "${GITHUB_HOST}" >/dev/null 2>&1; then
         return
      fi
   fi

   echo "error: gh is not authenticated. Provide GITHUB_TOKEN with actions:read scope or run 'gh auth login'." >&2
   exit 1
}

ensure_gh
ensure_auth

run_id="$(gh run list \
   --workflow "${WORKFLOW_FILE}" \
   --branch "${BRANCH_NAME}" \
   --event push \
   --status success \
   --limit 1 \
   --json databaseId \
   --jq '.[0].databaseId' \
   --hostname "${GITHUB_HOST}")"

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
