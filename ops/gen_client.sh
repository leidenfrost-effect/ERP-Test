#!/usr/bin/env bash
set -euo pipefail

SPEC_PATH="${OPENAPI_SPEC_PATH:-openapi/openapi.yaml}"
OUT_DIR="${OPENAPI_CLIENT_OUT_DIR:-clients/mobile_api_client}"
GENERATOR="${OPENAPI_CLIENT_GENERATOR:-dart}"

if [ ! -f "${SPEC_PATH}" ]; then
  echo "client generation failed: spec not found (${SPEC_PATH})" >&2
  exit 1
fi

mkdir -p "$(dirname "${OUT_DIR}")"

if command -v openapi-generator-cli >/dev/null 2>&1; then
  openapi-generator-cli generate \
    -i "${SPEC_PATH}" \
    -g "${GENERATOR}" \
    -o "${OUT_DIR}" \
    --skip-validate-spec
  echo "client generated via local openapi-generator-cli at ${OUT_DIR}"
  exit 0
fi

if command -v docker >/dev/null 2>&1; then
  docker run --rm \
    -v "$(pwd):/local" \
    openapitools/openapi-generator-cli:v7.15.0 generate \
    -i "/local/${SPEC_PATH}" \
    -g "${GENERATOR}" \
    -o "/local/${OUT_DIR}" \
    --skip-validate-spec
  echo "client generated via dockerized openapi-generator-cli at ${OUT_DIR}"
  exit 0
fi

echo "client generation failed: install openapi-generator-cli or docker." >&2
exit 1
