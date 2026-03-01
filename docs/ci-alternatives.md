# CI Alternatives

Primary pipeline is GitHub Actions (`.github/workflows/quality-gate.yml`).

## GitLab CI (minimal)

```yaml
quality_gate:
  image: ubuntu:24.04
  stage: test
  script:
    - apt-get update && apt-get install -y bash jq curl python3 python3-pip
    - pip3 install semgrep schemathesis openapi-spec-validator
    - bash ops/quality_gate.sh
    - bash ops/gen_sbom.sh
  artifacts:
    when: always
    paths:
      - artifacts/security/
      - artifacts/sbom/
```

## Azure DevOps (minimal)

```yaml
trigger:
- main

pool:
  vmImage: ubuntu-latest

steps:
- checkout: self
- bash: |
    sudo apt-get update
    sudo apt-get install -y jq curl python3 python3-pip
    pip3 install semgrep schemathesis openapi-spec-validator
    bash ops/quality_gate.sh
    bash ops/gen_sbom.sh
  displayName: Run security quality gate
- task: PublishBuildArtifacts@1
  inputs:
    PathtoPublish: artifacts
    ArtifactName: security-and-sbom
```
