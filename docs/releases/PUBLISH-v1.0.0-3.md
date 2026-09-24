# Publishing checklist — Ubuntu Gaming Mode v1.0.0-3 GOLD

This checklist is intentionally separate from the release body.

## GitHub Release

- Tag: `v1.0.0-3`
- Target: `main`
- Title: `Ubuntu Gaming Mode v1.0.0-3 — GOLD`
- Body: copy from [v1.0.0-3.md](v1.0.0-3.md)
- Mark as latest release: yes
- Pre-release: no
- Discussion: optional

## Upload only the validated artifacts

- `ubuntu-gaming-mode_1.0.0-3_amd64.deb`
- `SHA256SUMS`

Do **not** rebuild the 1.0.0-3 package.

Expected SHA256:

```text
959afa06fd87eeeba8ffc1d4a3a6c965f9fa902d5678d43597b2ce7b20dca797  ubuntu-gaming-mode_1.0.0-3_amd64.deb
```

## After publishing

1. Download both assets from the public GitHub Release.
2. Run `sha256sum -c SHA256SUMS`.
3. Confirm the downloaded `.deb` still has the GOLD SHA256.
4. Perform the final install test from the downloaded release artifact.
5. Record the GitHub-download acceptance test in the project checklist.
