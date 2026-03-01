# Tool Installation Notes

The security/quality scripts can run in partial mode when some tools are missing.

## Required for Full Local Gate

- `semgrep`
- `gitleaks`
- `trivy`
- `osv-scanner`
- `jq`
- `syft` (or `docker` fallback)
- `openapi-spec-validator` (or Python module)
- `schemathesis` (for contract tests)

## Quick Install Examples

### macOS (Homebrew)

```bash
brew install jq trivy osv-scanner gitleaks syft
pipx install semgrep
pipx install schemathesis
pipx install openapi-spec-validator
```

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y jq python3-pip pipx
pipx install semgrep
pipx install schemathesis
pipx install openapi-spec-validator
```

Install `gitleaks`, `trivy`, `osv-scanner`, and `syft` from their official release packages if they are not in your distro repo.

### Windows (PowerShell)

```powershell
winget install --id GitHub.cli -e
winget install --id Docker.DockerDesktop -e
winget install --id Python.Python.3.12 -e
```

Then install Python-based tools:

```powershell
python -m pip install --user pipx
pipx install semgrep
pipx install schemathesis
pipx install openapi-spec-validator
```

Use direct binaries or package managers for `gitleaks`, `trivy`, `osv-scanner`, `jq`, and `syft`.
