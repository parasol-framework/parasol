#!/bin/sh

# Cloud sessions only
if [ "$CLAUDE_CODE_REMOTE" != "true" ]; then
  exit 0
fi

if command -v gh >/dev/null 2>&1; then
   exit 0
fi

export DEBIAN_FRONTEND="noninteractive"
apt-get update -y
if apt-get install -y gh; then
   exit 0
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

exit 0
