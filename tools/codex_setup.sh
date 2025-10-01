#!/usr/bin/env bash
# Download and install the Parasol CI artifact for local use.
#
# The script mirrors the behaviour of CI by fetching the latest successful
# workflow run artefact and extracting it into install/agents (by default).
#
# NB: The GITHUB_TOKEN must be defined in the Codex Cloud settings as a secret variable

set -euo pipefail

WORKFLOW_FILE="${WORKFLOW_FILE:-ci.yml}"
BRANCH_NAME="${BRANCH_NAME:-master}"
ARTIFACT_NAME="${ARTIFACT_NAME:-parasol-install-ubuntu-latest-FastBuild}"
DEST_DIR="${DEST_DIR:-install/agents}"
GITHUB_HOST="${GITHUB_HOST:-github.com}"
DEFAULT_REPOSITORY="${DEFAULT_REPOSITORY:-team-parasol/parasol}"

# The GitHub CLI expects authentication tokens to be supplied via the
# GITHUB_TOKEN environment variable.  Older versions of this script
# populated GH_TOKEN instead, which meant the non-interactive login step
# failed and the artefact download never happened.  Normalise the token so
# that GITHUB_TOKEN is always set when either variable is provided.
if [[ -z "${GITHUB_TOKEN:-}" && -n "${GH_TOKEN:-}" ]]; then
   export GITHUB_TOKEN="${GH_TOKEN}"
fi

# Older versions of gh (including the one available in this environment)
# do not support the --hostname flag.  Setting GH_HOST ensures the CLI
# operates against the requested GitHub instance without requiring the
# newer flag.
export GH_HOST="${GITHUB_HOST}"

gh_authenticated=false

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
   if gh auth status >/dev/null 2>&1; then
      gh_authenticated=true
      return
   fi

   if [[ -n "${GITHUB_TOKEN:-}" ]]; then
      if printf '%s\n' "${GITHUB_TOKEN}" \
         | gh auth login --with-token --scopes "actions:read" >/dev/null; then
         if gh auth status >/dev/null 2>&1; then
            gh_authenticated=true
            return
         fi
      fi
   fi

   echo "error: GitHub CLI authentication failed; provide GITHUB_TOKEN with actions:read scope or run 'gh auth login'." >&2
   gh_authenticated=false
   return 1
}

discover_repo() {
   if [[ -n "${REPOSITORY:-}" ]]; then
      printf '%s\n' "${REPOSITORY}"
      return
   fi

   if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      local remote_url
      if remote_url="$(git remote get-url origin 2>/dev/null)"; then
         :
      elif remote_url="$(git remote get-url upstream 2>/dev/null)"; then
         :
      else
         remote_url=""
      fi

      if [[ -n "${remote_url}" ]]; then
         remote_url="${remote_url%.git}"
         remote_url="${remote_url%/}"
         printf '%s\n' "${remote_url}" | sed -E 's#.*[:/]([^/:]+/[^/:]+)$#\1#'
         return
      fi
   fi

   printf '%s\n' "${DEFAULT_REPOSITORY}"
}

ensure_gh
if ! ensure_auth; then
   exit 1
fi

repository="$(discover_repo)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

artifact_tarball=""
download_source=""
tar_strip=""

download_ci_artifact() {
   if [[ "${gh_authenticated}" != true ]]; then
      return 1
   fi

   local run_id
   if ! run_id="$(gh run list \
      --workflow "${WORKFLOW_FILE}" \
      --branch "${BRANCH_NAME}" \
      --event push \
      --status success \
      --limit 1 \
      --json databaseId \
      --jq '.[0].databaseId' \
      --repo "${repository}" 2>/dev/null)"; then
      echo "error: failed to query workflow runs via gh." >&2
      return 1
   fi

   if [[ -z "${run_id}" ]]; then
      echo "error: no successful runs found for ${WORKFLOW_FILE} on ${BRANCH_NAME}." >&2
      return 1
   fi

   echo "Using workflow run ${run_id}"

   if ! gh run download "${run_id}" \
      --name "${ARTIFACT_NAME}" \
      --dir "${tmp_dir}" \
      --repo "${repository}" >/dev/null 2>&1; then
      echo "error: failed to download CI artefact." >&2
      return 1
   fi

   local tarball
   tarball="$(find "${tmp_dir}" -type f -name '*.tar.gz' -print -quit)"
   if [[ -z "${tarball}" ]]; then
      echo "error: no .tar.gz found in downloaded CI artefact." >&2
      return 1
   fi

   artifact_tarball="${tarball}"
   download_source="workflow ${run_id}"
   tar_strip=""
   return 0
}

if ! download_ci_artifact; then
   exit 1
fi

if [[ -z "${artifact_tarball}" ]]; then
   echo "error: no artefact available; aborting." >&2
   exit 1
fi

mkdir -p "${DEST_DIR}"
rm -rf "${DEST_DIR:?}/"*

tar -xzf "${artifact_tarball}" -C "${DEST_DIR}" ${tar_strip}

if [[ -n "${download_source}" ]]; then
   echo "Restored ${ARTIFACT_NAME} into ${DEST_DIR} from ${download_source}"
else
   echo "Restored ${ARTIFACT_NAME} into ${DEST_DIR}"
fi
