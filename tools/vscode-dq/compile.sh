#!/bin/bash

# npx @vscode/vsce package

set -euo pipefail

npm ci
npm run package
