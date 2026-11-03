# Redivis's fork of ReadStat

Redivis's app image uses this fork's `readstat` and `extract_metadata` binaries to convert SAS, Stata and SPSS uploads
and exports. It differs from upstream ([WizardMac/ReadStat](https://github.com/WizardMac/ReadStat)) by its changes
marked "Redivis:" in the source; see [CHANGELOG.md](CHANGELOG.md).

## Releasing

Releases are automatic, like Redivis's Python and R libraries: to release, bump [VERSION](VERSION) and add its section
to [CHANGELOG.md](CHANGELOG.md), then merge to `main`. If that version isn't tagged yet,
`.github/workflows/release.yml` builds the binaries for linux/amd64 and linux/arm64 with [build.sh](build.sh) (on
Debian trixie, as in the app image, running the tests first), then creates the tag and a GitHub release with them
attached as `readstat-<version>-linux-<arch>.tar.gz`, each with a `.sha256`. Versions already tagged are skipped, so a
failed run can be re-run from the Actions tab.

Then update `READSTAT_VERSION` and the checksums in the app's `containers/app/Dockerfile`.

## Merging upstream

Merge upstream's `dev` branch, and keep each "Redivis:" change unless upstream has made it.
