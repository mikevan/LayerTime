# Vixen download page acceptance

The public fox-hunt download page is served at `/LayerTime/vixen/`. Its Android APK comes from Vixen. Both ESP32-C5 factory images come from Hunter, which owns the Hunter and Vixen beacon firmware targets.

## Check-in and publication

1. Merge the accepted Hunter and Vixen changes to their respective `main` branches. The product workflows must both support the `download_bundle` input before enabling the LayerTime publishing workflow.
2. Merge the publishing workflow into LayerTime `main`. It runs on changes to the Pages source and publishing code, and checks hourly for product updates. For an immediate check after the product merges, run `gh workflow run vixen-downloads.yml -R mikevan/LayerTime --ref main`.
3. Confirm both product checks and the Pages deployment passed. On the public page, open **Build details and checksums** and verify that the listed Hunter and Vixen commits match the accepted commits. Check the APK link and both installer manifests. Their files should match the SHA-256 digests in `downloads.json`.

Each run resolves exact product commits before building. It runs Hunter's host checks and Vixen's Android tests, builds both factory images and signs the full sideload APK. The publication refuses unexpected Android package or certificate identities, downgrades, incomplete firmware images and superseded revisions. The APK remains at the existing `layerscope.apk` URL for saved links. The visible app name is Vixen.

## One-time GitHub setup

In `mikevan/LayerTime`, change Settings > Pages > Source to **GitHub Actions** when both product workflow updates are on `main` and signing is ready. The workflow builds the existing `docs/` site with Jekyll and deploys the assembled site. Only the deploy job receives Pages write permission. No APK or generated firmware needs to be checked into Git.

Configure these Actions secrets in LayerTime using the release key that signed the existing public Android APK.

- `VIXEN_DOWNLOAD_KEYSTORE_BASE64`
- `VIXEN_DOWNLOAD_STORE_PASSWORD`
- `VIXEN_DOWNLOAD_KEY_ALIAS`
- `VIXEN_DOWNLOAD_KEY_PASSWORD`

The expected signing certificate SHA-256 is `907dfd514ed86f34f3caba164a6df00d78fd70bca3479b895c0e9798370747f0`. Keep the signing material out of Git and build artifacts.

For first publication or any signing change, install the signed APK over the previous public version on an Android test device and confirm its data is accessible. Flash the Hunter and beacon images only to identified spare boards, then follow the existing hardware acceptance procedure. GitHub Actions verifies the packaged files and product checks. Those results do not establish handset or board acceptance.
