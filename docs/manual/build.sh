#!/usr/bin/env bash
# Пересобирает docs/Eido-manual.pdf: свежие скриншоты из теста → нарезка → печать HTML в PDF.
# Нужны: Python 3 с Pillow, Node.js с Playwright. На Linux тест запускается через xvfb-run.
set -euo pipefail
cd "$(dirname "$0")/../.."

TEST=./build/EidoTest_artefacts/Release/EidoTest
SHOTS=$(mktemp -d)
trap 'rm -rf "$SHOTS"' EXIT

[ -d build ] || cmake -B build -DEIDO_BUILD_TESTS=ON
cmake --build build --target EidoTest
RUN=()
if [[ "$(uname)" == "Linux" ]]; then RUN=(xvfb-run -a); fi
"${RUN[@]}" "$TEST" "$SHOTS" | tail -1

python3 docs/manual/crop.py "$SHOTS" docs/manual/img
NODE_PATH="${NODE_PATH:-$(npm root -g)}" node docs/manual/print.js docs/manual/Eido-manual.html docs/Eido-manual.pdf
echo "docs/Eido-manual.pdf: $(python3 -c 'import os; print(os.path.getsize("docs/Eido-manual.pdf") // 1024)') KB"
