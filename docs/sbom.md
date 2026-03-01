# SBOM

Generate SBOM locally:

```bash
bash ops/gen_sbom.sh
```

Output artifacts:

- `artifacts/sbom/sbom.cdx.json` (CycloneDX)
- `artifacts/sbom/sbom.spdx.json` (SPDX)

If `syft` is not installed locally, the script falls back to dockerized syft when Docker is available.

## CI Integration

`quality-gate.yml` runs SBOM generation and uploads:

- `artifacts/sbom/sbom.cdx.json`
- `artifacts/sbom/sbom.spdx.json`

## Publication Note

Treat SBOM files as release artifacts, not source-controlled files. Review component/license policy before public sharing.
