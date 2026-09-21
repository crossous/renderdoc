#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${RENDERDOC_METAL_BUILD_DIR:-${REPO_ROOT}/build-macos-debug}"

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required to locate the Qt 5 development tools." >&2
  exit 1
fi

QT5_PREFIX="$(brew --prefix qt@5)"
BISON_PREFIX="$(brew --prefix bison)"
QMAKE="${QMAKE_QT5_COMMAND:-${QT5_PREFIX}/bin/qmake}"

if [ ! -x "${QMAKE}" ]; then
  echo "Qt 5 qmake was not found at ${QMAKE}." >&2
  echo "Install dependencies with: brew install qt@5 autoconf automake pcre bison" >&2
  exit 1
fi

export PATH="${QT5_PREFIX}/bin:${BISON_PREFIX}/bin:${PATH}"

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DQMAKE_QT5_COMMAND="${QMAKE}" \
  -DENABLE_METAL=ON \
  -DENABLE_GL=OFF \
  -DENABLE_GLES=OFF \
  -DENABLE_EGL=OFF \
  -DENABLE_VULKAN=OFF \
  -DENABLE_PYRENDERDOC=OFF \
  -DENABLE_RENDERDOCCMD=ON

cmake --build "${BUILD_DIR}" --target build-qrenderdoc renderdoccmd -j "$(sysctl -n hw.ncpu)"

APP_PATH="${BUILD_DIR}/bin/qrenderdoc.app"
echo "Built ${APP_PATH}"
echo "Built ${BUILD_DIR}/bin/renderdoccmd"

if [ "${1:-}" = "--run" ]; then
  open "${APP_PATH}"
fi
