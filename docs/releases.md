# Weekly releases

The [Weekly release workflow](../.github/workflows/release.yml) runs every Monday
at 06:17 UTC on `main`. It also supports **Actions → Weekly release → Run workflow**
with `main` selected. GitHub may delay scheduled runs.

Each run:

1. Runs the existing CI workflow against the source commit.
2. Runs `python scripts/update_version.py --bump-patch`. For example,
   `0.0.4-unstable` becomes `0.0.5-unstable`; major, minor, and release status stay
   unchanged. Both `VERSION.txt` and Doxygen's `PROJECT_NUMBER` are updated.
3. Builds Doxygen HTML with Graphviz from the versioned release source.
4. Syncs the generated HTML into `docs/` in the `nexilistudios.github.io`
   repository. The website build copies that directory to `dist/docs/`.
5. Pushes the version commit to `main` and its `vX.Y.Z-status` tag atomically.
6. Creates a GitHub release with generated release notes. Versions ending in
   `-unstable` are marked as prereleases. GitHub provides source archives;
   this workflow does not build binary packages.

A patch release is created each week even when there are no other source changes.
The website repository builds and deploys its Pages site on pushes to `main`.

## Repository setup

- Merge the workflow changes into the default branch, `main`.
- Give this repository's release workflow write access to
  `nexilistudios/nexilistudios.github.io` through `NEXILIS_IO_DEPLOY_KEY` or
  `NEXILIS_IO_TOKEN`. The default `GITHUB_TOKEN` does not grant access to a
  separate repository.
- Set the website repository's **Settings → Pages → Build and deployment →
  Source** to **GitHub Actions**. Its deployment workflow publishes the site
  with API documentation under `/docs/`.
- Allow GitHub Actions to write repository contents and create tags/releases.
  Branch and tag rules must allow the workflow's version commit and release tag;
  this workflow does not bypass protection rules. If required pull requests prevent
  bot pushes, adjust the repository's release policy before enabling the schedule.
- The website repository's `github-pages` environment must allow deployments
  from `main`. Required environment reviewers will make publication wait for
  approval.

The release workflow uses this repository's `GITHUB_TOKEN` for its version
commit, tag, and release. See GitHub's [custom Pages workflow
documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
for the website deployment setup.

## Failures and retries

CI, website repository access, or documentation build failures prevent the release push.
If `main` changes during the run, the push fails without overwriting those changes;
start a new manual run against the updated `main`.

If a run fails after the push, rerun its failed jobs to finish the same release.
The workflow recognizes its existing tag and does not increment the version again.
Starting a new manual run creates a new patch release.

For local version changes, install the script dependencies with
`python -m pip install ./scripts`, then run either:

```sh
python scripts/update_version.py --bump-patch
python scripts/update_version.py 0.1.0-unstable
```
