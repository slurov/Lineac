# Builds

Every released build is kept here, so older versions stay available after a
newer one lands. Each folder holds the exact executable that was published
under that tag.

| Version | Date | File | Size | Notes |
|---|---|---|---|---|
| **2.0.0** *(current)* | 2026-09-01 | [`v2.0.0/LineAC-v2.0.0.exe`](v2.0.0/LineAC-v2.0.0.exe) | 175 KB | Reworked click engine, live console |
| 1.0.0 | 2026-07-23 | [`v1.0.0/LineAC-v1.0.0.exe`](v1.0.0/LineAC-v1.0.0.exe) | 163 KB | First public build |

Nothing to install — download the `.exe` and run it.

## Checksums (SHA-256)

```
0e562b544b6c7744522dca4b5c8156f0de04da384dcc0ed27ab6a70b5cb8d824  LineAC-v2.0.0.exe
6777cd1e3a7c05a6e2ab87f542ed4294b5b7f987b77c68996f348ffc25b01a6d  LineAC-v1.0.0.exe
```

Verify on Windows with:

```bat
certutil -hashfile LineAC-v2.0.0.exe SHA256
```

See [../CHANGELOG.md](../CHANGELOG.md) for what changed between the two.
