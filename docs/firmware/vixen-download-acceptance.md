# Vixen download page acceptance

The public fox-hunt page at `/LayerTime/vixen/` serves the Vixen Android APK and two ESP32-C5 factory images. Vixen owns the Android build. Hunter owns both firmware targets.

## Build and sign the APK locally

Use the existing Vixen checkout at `C:\workspace_LayerTime\Vixen`. Pull the accepted `main` with `git pull --ff-only` and confirm the working tree is clean. Gradle reads the ignored `android/keystore.properties` file. Configure that file locally with the existing release keystore. Use an absolute `storeFile` path. Keep the file and signing key on your machine.

From the Vixen `android` folder, build `:app:assembleFullRelease`. The signed output is `app/build/outputs/apk/full/release/app-full-release.apk`. Verify it with Android SDK `apksigner verify --print-certs`. The certificate must match the previous public download:

`907dfd514ed86f34f3caba164a6df00d78fd70bca3479b895c0e9798370747f0`

## Submit the signed APK

Copy the signed output to `vixen.apk` outside the repository. Get the full Vixen commit SHA with `git rev-parse HEAD`. On `mikevan/Vixen`, create a GitHub prerelease tagged `website-apk-<full SHA>` targeting that exact commit and attach `vixen.apk`. The prerelease is a public staging location. Upload only the signed APK.

```powershell
gh release create "website-apk-$vixenCommit" $signedApkPath --repo mikevan/Vixen --target $vixenCommit --prerelease --title "Website APK $vixenCommit" --notes 'Locally signed APK for download-page acceptance.'
```

Set `$vixenCommit` to the full commit SHA and `$signedApkPath` to the full path ending in `vixen.apk` before running the command.

## Publish and verify

1. After merging the publisher update into LayerTime `main`, switch [LayerTime Pages](https://github.com/mikevan/LayerTime/settings/pages) to **GitHub Actions** once the signed APK is available.
2. Run `gh workflow run vixen-downloads.yml -R mikevan/LayerTime --ref main` for immediate publication. The hourly run also detects the APK. If Vixen main moves, build and sign an APK for the new commit before it can publish.
3. Confirm both product checks and the Pages deployment passed. On the public page, open **Build details and checksums**. Compare the listed Hunter and Vixen commits and the three file digests with the accepted build and downloads.

The workflow verifies the APK signature, certificate and alignment. It checks that the release tag points to the tested source revision and that the signed APK package ID and version match the CI build. The release tag records the source used for the local build. The CI and local APK binaries can differ. The site builds both factory images from the accepted Hunter commit and refuses stale revisions or Android downgrades.

For first publication, check an in-place upgrade on an Android test device and flash the firmware to identified spare boards using the [hardware acceptance procedure](hardware-acceptance.md).
