# 🤖 Installers in CI

This guide is for developers who ship an app made with Haylen to stores and to players. It gives a complete GitHub Actions workflow that builds the app and produces what each platform ships, and it explains every secret that the workflow reads and how to make each one. The workflow belongs to the repository of the app, since the CI of the engine repository only tests and publishes nothing. It builds:

- A macOS app and disk image, signed with Developer ID and notarized.
- The installer package of the Mac App Store.
- The Windows installer.
- The Debian package and the AppImage of Linux.
- The iOS and tvOS archives of the App Store.
- An Android App Bundle signed with the upload key.
- The web site.

Every step that builds runs the command `haylen` of the engine checkout with `--verbose`. So the log of the job holds the whole output of every tool, which a terminal otherwise keeps in `build/logs`, as the [build guide](build.md#what-the-commands-print) explains. A step whose result the next command reads runs it with `--json` instead, which prints the result as one JSON document and the output of the tools on the error output, so the log of the job still holds it.

## What the workflow builds

| Job | Runner | Result |
| --- | --- | --- |
| `engine-apple` | macOS | The Apple SDK of macOS, iOS and tvOS and the desktop SDK of the Mac that builds them, kept in the cache for the Apple jobs. |
| `macos` | macOS | `<name>.dmg`, a disk image signed with Developer ID, notarized and stapled, and `<name>-macos.zip` with the stapled app. |
| `mac-app-store` | macOS | `<name>.pkg`, the installer package signed for the Mac App Store, uploaded to App Store Connect on a tag. |
| `ios-tvos` | macOS | The `.ipa` archives of iOS and tvOS, signed in the cloud for App Store Connect and uploaded on a tag. |
| `android` | Linux | The App Bundle `.aab`, signed with the upload key, for Google Play. |
| `windows` | Windows | `<executable>-<version>-setup.exe`, the installer signed with the code signing certificate. |
| `linux` | Linux | `<executable>_<version>_<arch>.deb` and `<executable>-<version>-<arch>.AppImage`. |
| `web` | Linux | The site with the protected release, ready for any static web server. |

Every job builds the Release configuration. So every result holds the [protected release](../content/content.md) of the app and a binary that compiles in its bootstrap, as the [distribution guide](distribution.md) describes. The results are the artifacts of the run, and the store uploads run only for tags.

## Before the first run

- The repository holds the app in a folder, `my-game` in the workflow, with `haylen.json`, `source/`, `content/` and the native code of its bridge in `native/`, which `haylen new` creates. The app holds no platform project, since every build generates it. The workflow checks the app out into a folder of the same name, because the name of the folder names the executable and the packages of desktop apps.
- The variable `HAYLEN_REF` names the tag or the commit of the [engine repository](https://github.com/haylen-org/haylen) that builds the app. A tag keeps every run on the same engine, and keeps the caches of the engine SDKs valid.
- The app has its content keys, which `haylen content keys <app>` shows, and creates on the machine of the developer, as [key folders](../content/content.md#key-folders) describe. Every release of an app needs the keys of its earlier releases, so the workflow receives a copy of them.
- The bundle identifier of `haylen.json` is registered as an App ID in the account of the Apple Developer Program, with an app record in App Store Connect for iOS, tvOS and macOS. The app exists in the Play Console.
- The Mac App Store accepts only sandboxed apps. So the section `platforms.macos.entitlements` of `haylen.json` holds `com.apple.security.app-sandbox` set to `true`, with the entitlements that the app needs, such as `com.apple.security.network.client`, and the Developer ID build carries them too. The generated project takes them, as the [distribution guide](distribution.md) describes.
- The `build` of `haylen.json` grows before every run that uploads to a store, since every store refuses a build number it already received, and the `version` changes with every release that people see. Every platform takes its build number from the `build`.

```json
{
    "platforms": {
        "macos": {
            "entitlements": {
                "com.apple.security.app-sandbox": true,
                "com.apple.security.network.client": true
            }
        }
    }
}
```

## Secrets

The secrets live in the settings of the repository, under Secrets and variables, Actions. A secret that holds a file holds it as Base64 text on one line. No secret value belongs in the workflow, the repository or a log.

| Secret | Value |
| --- | --- |
| `ASC_API_KEY` | The private key of an App Store Connect API key, the text of its file `AuthKey_<key ID>.p8`. |
| `ASC_ISSUER_ID` | The issuer ID of the API keys of the team. |
| `ASC_KEY_ID` | The key ID of the API key. |
| `KEYCHAIN_PWD` | The password of the temporary keychain that each macOS job creates for its certificates. |
| `MACOS_CERTIFICATE` | The Developer ID Application certificate with its private key, as a `.p12` file in Base64. |
| `MACOS_CERTIFICATE_PWD` | The password of that `.p12` file. |
| `MAS_APP_CERTIFICATE` | The certificate that signs apps for the Mac App Store, Apple Distribution or Mac App Distribution, with its private key, as a `.p12` file in Base64. |
| `MAS_APP_CERTIFICATE_PWD` | The password of that `.p12` file. |
| `MAS_APP_IDENTITY` | The name of that signing identity, such as `Apple Distribution: My Company (AB12CD34EF)`. |
| `MAS_INSTALLER_CERTIFICATE` | The Mac Installer Distribution certificate with its private key, as a `.p12` file in Base64. |
| `MAS_INSTALLER_CERTIFICATE_PWD` | The password of that `.p12` file. |
| `MAS_INSTALLER_IDENTITY` | The name of the installer identity, such as `3rd Party Mac Developer Installer: My Company (AB12CD34EF)`. |
| `MAS_PROVISION_PROFILE` | The Mac App Store provisioning profile of the app, a `.provisionprofile` file in Base64. |
| `NOTARIZATION_PASSWORD` | An app-specific password of the Apple Account that notarizes. |
| `NOTARIZATION_TEAM_ID` | The team ID of the Apple Developer Program, which also signs every Apple build. |
| `NOTARIZATION_USERNAME` | The email address of the Apple Account that notarizes. |
| `SIGNING_IDENTITY` | The name of the Developer ID identity, such as `Developer ID Application: My Company (AB12CD34EF)`. |
| `HAYLEN_CONTENT_KEYS` | The key folder of the app as a tar archive in Base64. |
| `ANDROID_KEYSTORE` | The upload keystore of the app in Base64. |
| `ANDROID_KEYSTORE_PWD` | The password of the upload keystore. |
| `ANDROID_KEY_ALIAS` | The alias of the upload key. |
| `ANDROID_KEY_PWD` | The password of the upload key. |
| `WINDOWS_CERTIFICATE` | The code signing certificate of Windows with its private key, as a `.pfx` file in Base64. |
| `WINDOWS_CERTIFICATE_PWD` | The password of that `.pfx` file. |
| `WINDOWS_TIMESTAMP_URL` | The time stamp server of the authority that issued the Windows certificate. |

### How to make each secret

**App Store Connect API key (`ASC_API_KEY`, `ASC_ISSUER_ID`, `ASC_KEY_ID`).** In App Store Connect, open Users and Access, then Integrations, then App Store Connect API. Generate a team key with the Admin role, which lets Xcode create the distribution certificates and profiles of iOS and tvOS in the cloud. The page shows the issuer ID above the list of keys, and the key ID in the row of the key. Download the `.p8` file, which App Store Connect offers only once. Store its whole text, from `-----BEGIN PRIVATE KEY-----` to `-----END PRIVATE KEY-----`, as `ASC_API_KEY`.

**Keychain password (`KEYCHAIN_PWD`).** Any long random text, such as the output of `openssl rand -base64 32`. Each macOS job creates a keychain of its own with it, imports the certificates there and loses it with the runner.

**Developer ID certificate (`MACOS_CERTIFICATE`, `MACOS_CERTIFICATE_PWD`, `SIGNING_IDENTITY`).** The Account Holder of the team creates a Developer ID Application certificate under Certificates, Identifiers and Profiles of the developer account. It starts from a certificate signing request that Keychain Access makes with Certificate Assistant, Request a Certificate From a Certificate Authority. Once the certificate is in the login keychain, Keychain Access exports it with its private key from My Certificates as a `.p12` file with a password, which is `MACOS_CERTIFICATE_PWD`. The command `base64 -i developer-id.p12 | pbcopy` copies the text of `MACOS_CERTIFICATE`. The command `security find-identity -v -p codesigning` lists the name of the identity for `SIGNING_IDENTITY`.

**Mac App Store certificates (`MAS_APP_CERTIFICATE`, `MAS_APP_CERTIFICATE_PWD`, `MAS_APP_IDENTITY`, `MAS_INSTALLER_CERTIFICATE`, `MAS_INSTALLER_CERTIFICATE_PWD`, `MAS_INSTALLER_IDENTITY`).** Create an Apple Distribution certificate, or a Mac App Distribution one, and a Mac Installer Distribution certificate in the developer account the same way. Export each with its private key as a `.p12` file with its own password, and store them in Base64 as above. The command `security find-identity -v -p codesigning` lists the name of the app identity, and `security find-identity -v -p basic` the name of the installer identity.

**Provisioning profile (`MAS_PROVISION_PROFILE`).** Under Profiles of the developer account, create a Mac App Store Connect distribution profile for the App ID of the app with the Mac App Store certificate. Download it and store the output of `base64 -i my-game.provisionprofile` as the secret. A profile expires with its certificate, and a new certificate needs a new profile.

**Notarization (`NOTARIZATION_USERNAME`, `NOTARIZATION_PASSWORD`, `NOTARIZATION_TEAM_ID`).** The username is the email address of an Apple Account of the team. Its app-specific password comes from Sign-In and Security, App-Specific Passwords, on the page of the Apple Account. The command `xcrun notarytool store-credentials` checks the three values on a Mac before they become secrets. The team ID is the ten characters that the Membership details of the developer account show.

**Content keys (`HAYLEN_CONTENT_KEYS`).** On macOS, the command `tar -c -C ~/Library/Application\ Support/Haylen/keys com.example.mygame | base64` prints the secret, with the identifier of the app. On Linux the folder is `~/.config/haylen/keys`, and the command `base64 -w 0` keeps the text on one line. On Windows it is `%APPDATA%\Haylen\keys`. The folder holds the keys of every release of the app, so a key added with `haylen content keys my-game --rotate` goes into the secret again.

**Android upload key (`ANDROID_KEYSTORE`, `ANDROID_KEYSTORE_PWD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PWD`).** The command `haylen app sign my-game --target android --option password="<password>"` creates the upload key in `platform/android/keystore/` of the app, which its `.gitignore` keeps out of the repository. The command `base64 < my-game/platform/android/keystore/release.jks` prints `ANDROID_KEYSTORE`. The password of the command is both `ANDROID_KEYSTORE_PWD` and `ANDROID_KEY_PWD`. The alias, `upload` unless `--option alias=<alias>` named another, is `ANDROID_KEY_ALIAS`. Google Play signs the app for the devices with its own app signing key, and receives the certificate of the upload key, `release.pem`, when the app is registered.

**Windows certificate (`WINDOWS_CERTIFICATE`, `WINDOWS_CERTIFICATE_PWD`, `WINDOWS_TIMESTAMP_URL`).** A code signing certificate from a certificate authority, exported with its private key as a `.pfx` file with a password. The PowerShell command `[Convert]::ToBase64String([IO.File]::ReadAllBytes("certificate.pfx"))` prints `WINDOWS_CERTIFICATE`, and the authority names its time stamp server. A certificate on a hardware token never leaves it. It signs on a runner of the team that holds the token, with `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` instead of the file.

## The workflow

Save it as `.github/workflows/release.yml` in the repository of the app:

```yaml
name: Release

# Builds the installers and store packages of the app on every tag that starts with "v", and on demand from the Actions tab.
on:
  push:
    tags: ["v*"]
  workflow_dispatch:

env:
  # The folder of the app in the repository, whose name also names the executable and the packages of desktop apps.
  APP: my-game
  # The tag or commit of the engine that builds the app.
  HAYLEN_REF: v0.0.1
  CPM_SOURCE_CACHE: ${{ github.workspace }}/.cache/cpm

jobs:
  engine-apple:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - run: brew install ninja
      - uses: actions/cache@v6
        id: engine
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-apple-${{ env.HAYLEN_REF }}
      # Apple builds of the engine precompile the shaders of the renderer with the Metal toolchain, a component of Xcode that a runner image may lack.
      - name: Install the Metal toolchain
        if: steps.engine.outputs.cache-hit != 'true'
        run: xcrun -sdk macosx metal --version > /dev/null 2>&1 || xcodebuild -downloadComponent MetalToolchain
      - name: Build the Apple SDK and the desktop SDK of this Mac, whose content tool builds the releases
        if: steps.engine.outputs.cache-hit != 'true'
        run: |
          haylen/haylen --verbose sdk build --target macos --target ios --target tvos
          haylen/haylen --verbose sdk build --target player

  macos:
    needs: engine-apple
    runs-on: macos-latest
    env:
      HAYLEN_APPLE_TEAM: ${{ secrets.NOTARIZATION_TEAM_ID }}
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - run: brew install ninja
      - uses: actions/cache@v6
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-apple-${{ env.HAYLEN_REF }}
      - name: Restore the content keys of the app
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
      - name: Import the Developer ID certificate into a keychain of this run
        env:
          KEYCHAIN_PWD: ${{ secrets.KEYCHAIN_PWD }}
          MACOS_CERTIFICATE: ${{ secrets.MACOS_CERTIFICATE }}
          MACOS_CERTIFICATE_PWD: ${{ secrets.MACOS_CERTIFICATE_PWD }}
        run: |
          KEYCHAIN="$RUNNER_TEMP/signing.keychain-db"
          echo "KEYCHAIN=$KEYCHAIN" >> "$GITHUB_ENV"
          security create-keychain -p "$KEYCHAIN_PWD" "$KEYCHAIN"
          security set-keychain-settings -lut 21600 "$KEYCHAIN"
          security unlock-keychain -p "$KEYCHAIN_PWD" "$KEYCHAIN"
          echo "$MACOS_CERTIFICATE" | base64 --decode > "$RUNNER_TEMP/developer-id.p12"
          security import "$RUNNER_TEMP/developer-id.p12" -k "$KEYCHAIN" -P "$MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign
          security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PWD" "$KEYCHAIN"
          security list-keychains -d user -s "$KEYCHAIN" login.keychain-db
      - name: Generate the Xcode project with the protected release and the bootstrap
        run: |
          project=$(haylen/haylen --json app prepare "$APP" --target macos --config Release | jq -r .result.project)
          echo "PROJECT=$project" >> "$GITHUB_ENV"
      - name: Archive the app signed with Developer ID
        env:
          SIGNING_IDENTITY: ${{ secrets.SIGNING_IDENTITY }}
          NOTARIZATION_TEAM_ID: ${{ secrets.NOTARIZATION_TEAM_ID }}
        run: |
          xcodebuild archive -project "$PROJECT" -scheme App -configuration Release \
            -destination "generic/platform=macOS" -archivePath "$RUNNER_TEMP/macos.xcarchive" \
            CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="$SIGNING_IDENTITY" DEVELOPMENT_TEAM="$NOTARIZATION_TEAM_ID" \
            OTHER_CODE_SIGN_FLAGS="--timestamp --keychain $KEYCHAIN"
          options="$RUNNER_TEMP/export.plist"
          plutil -create xml1 "$options"
          plutil -insert method -string developer-id "$options"
          plutil -insert signingStyle -string manual "$options"
          plutil -insert signingCertificate -string "$SIGNING_IDENTITY" "$options"
          plutil -insert teamID -string "$NOTARIZATION_TEAM_ID" "$options"
          xcodebuild -exportArchive -archivePath "$RUNNER_TEMP/macos.xcarchive" -exportPath "$RUNNER_TEMP/export" -exportOptionsPlist "$options"
      - name: Notarize the app, then pack it into a notarized disk image
        env:
          SIGNING_IDENTITY: ${{ secrets.SIGNING_IDENTITY }}
          NOTARIZATION_USERNAME: ${{ secrets.NOTARIZATION_USERNAME }}
          NOTARIZATION_PASSWORD: ${{ secrets.NOTARIZATION_PASSWORD }}
          NOTARIZATION_TEAM_ID: ${{ secrets.NOTARIZATION_TEAM_ID }}
        run: |
          bundle=$(find "$RUNNER_TEMP/export" -maxdepth 1 -name "*.app" | head -n 1)
          name=$(basename "$bundle" .app)
          notarize() {
            xcrun notarytool submit "$1" --apple-id "$NOTARIZATION_USERNAME" --password "$NOTARIZATION_PASSWORD" --team-id "$NOTARIZATION_TEAM_ID" --wait
          }
          mkdir -p dist "$RUNNER_TEMP/image"
          ditto -c -k --keepParent "$bundle" "$RUNNER_TEMP/notarize.zip"
          notarize "$RUNNER_TEMP/notarize.zip"
          xcrun stapler staple "$bundle"
          ditto -c -k --keepParent "$bundle" "dist/$name-macos.zip"
          cp -R "$bundle" "$RUNNER_TEMP/image/"
          ln -s /Applications "$RUNNER_TEMP/image/Applications"
          hdiutil create -volname "$name" -srcfolder "$RUNNER_TEMP/image" -ov -format UDZO "dist/$name.dmg"
          codesign --sign "$SIGNING_IDENTITY" --timestamp --keychain "$KEYCHAIN" "dist/$name.dmg"
          notarize "dist/$name.dmg"
          xcrun stapler staple "dist/$name.dmg"
      - uses: actions/upload-artifact@v7
        with:
          name: macos
          path: dist/

  mac-app-store:
    needs: engine-apple
    runs-on: macos-latest
    env:
      HAYLEN_APPLE_TEAM: ${{ secrets.NOTARIZATION_TEAM_ID }}
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - run: brew install ninja
      - uses: actions/cache@v6
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-apple-${{ env.HAYLEN_REF }}
      - name: Restore the content keys of the app
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
      - name: Import the Mac App Store certificates and the provisioning profile
        env:
          KEYCHAIN_PWD: ${{ secrets.KEYCHAIN_PWD }}
          MAS_APP_CERTIFICATE: ${{ secrets.MAS_APP_CERTIFICATE }}
          MAS_APP_CERTIFICATE_PWD: ${{ secrets.MAS_APP_CERTIFICATE_PWD }}
          MAS_INSTALLER_CERTIFICATE: ${{ secrets.MAS_INSTALLER_CERTIFICATE }}
          MAS_INSTALLER_CERTIFICATE_PWD: ${{ secrets.MAS_INSTALLER_CERTIFICATE_PWD }}
          MAS_PROVISION_PROFILE: ${{ secrets.MAS_PROVISION_PROFILE }}
        run: |
          KEYCHAIN="$RUNNER_TEMP/signing.keychain-db"
          echo "KEYCHAIN=$KEYCHAIN" >> "$GITHUB_ENV"
          security create-keychain -p "$KEYCHAIN_PWD" "$KEYCHAIN"
          security set-keychain-settings -lut 21600 "$KEYCHAIN"
          security unlock-keychain -p "$KEYCHAIN_PWD" "$KEYCHAIN"
          echo "$MAS_APP_CERTIFICATE" | base64 --decode > "$RUNNER_TEMP/mas-app.p12"
          echo "$MAS_INSTALLER_CERTIFICATE" | base64 --decode > "$RUNNER_TEMP/mas-installer.p12"
          security import "$RUNNER_TEMP/mas-app.p12" -k "$KEYCHAIN" -P "$MAS_APP_CERTIFICATE_PWD" -T /usr/bin/codesign
          security import "$RUNNER_TEMP/mas-installer.p12" -k "$KEYCHAIN" -P "$MAS_INSTALLER_CERTIFICATE_PWD" -T /usr/bin/productbuild
          security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PWD" "$KEYCHAIN"
          security list-keychains -d user -s "$KEYCHAIN" login.keychain-db
          profile="$RUNNER_TEMP/mas.provisionprofile"
          echo "$MAS_PROVISION_PROFILE" | base64 --decode > "$profile"
          uuid=$(security cms -D -i "$profile" | plutil -extract UUID raw -)
          for folder in "$HOME/Library/MobileDevice/Provisioning Profiles" "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"; do
            mkdir -p "$folder"
            cp "$profile" "$folder/$uuid.provisionprofile"
          done
          echo "PROFILE_UUID=$uuid" >> "$GITHUB_ENV"
      - name: Generate the Xcode project with the protected release and the bootstrap
        run: |
          project=$(haylen/haylen --json app prepare "$APP" --target macos --config Release | jq -r .result.project)
          echo "PROJECT=$project" >> "$GITHUB_ENV"
      - name: Archive the app and export the installer package for the Mac App Store
        env:
          MAS_APP_IDENTITY: ${{ secrets.MAS_APP_IDENTITY }}
          MAS_INSTALLER_IDENTITY: ${{ secrets.MAS_INSTALLER_IDENTITY }}
          NOTARIZATION_TEAM_ID: ${{ secrets.NOTARIZATION_TEAM_ID }}
        run: |
          archive="$RUNNER_TEMP/mac-app-store.xcarchive"
          xcodebuild archive -project "$PROJECT" -scheme App -configuration Release \
            -destination "generic/platform=macOS" -archivePath "$archive" \
            CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="$MAS_APP_IDENTITY" PROVISIONING_PROFILE_SPECIFIER="$PROFILE_UUID" \
            DEVELOPMENT_TEAM="$NOTARIZATION_TEAM_ID" OTHER_CODE_SIGN_FLAGS="--keychain $KEYCHAIN"
          identifier=$(/usr/libexec/PlistBuddy -c "Print :ApplicationProperties:CFBundleIdentifier" "$archive/Info.plist")
          options="$RUNNER_TEMP/export.plist"
          plutil -create xml1 "$options"
          /usr/libexec/PlistBuddy \
            -c "Add :method string app-store-connect" \
            -c "Add :destination string export" \
            -c "Add :signingStyle string manual" \
            -c "Add :teamID string $NOTARIZATION_TEAM_ID" \
            -c "Add :signingCertificate string $MAS_APP_IDENTITY" \
            -c "Add :installerSigningCertificate string $MAS_INSTALLER_IDENTITY" \
            -c "Add :provisioningProfiles dict" \
            -c "Add :provisioningProfiles:$identifier string $PROFILE_UUID" \
            "$options"
          xcodebuild -exportArchive -archivePath "$archive" -exportPath dist -exportOptionsPlist "$options"
      - name: Upload the package to App Store Connect
        if: startsWith(github.ref, 'refs/tags/')
        env:
          ASC_API_KEY: ${{ secrets.ASC_API_KEY }}
          ASC_ISSUER_ID: ${{ secrets.ASC_ISSUER_ID }}
          ASC_KEY_ID: ${{ secrets.ASC_KEY_ID }}
        run: |
          mkdir -p "$HOME/.appstoreconnect/private_keys"
          printf '%s' "$ASC_API_KEY" > "$HOME/.appstoreconnect/private_keys/AuthKey_$ASC_KEY_ID.p8"
          xcrun altool --upload-app --type macos --file dist/*.pkg --apiKey "$ASC_KEY_ID" --apiIssuer "$ASC_ISSUER_ID"
      - uses: actions/upload-artifact@v7
        with:
          name: mac-app-store
          path: dist/*.pkg

  ios-tvos:
    needs: engine-apple
    runs-on: macos-latest
    env:
      HAYLEN_APPLE_TEAM: ${{ secrets.NOTARIZATION_TEAM_ID }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - target: ios
            platform: iOS
            type: ios
          - target: tvos
            platform: tvOS
            type: appletvos
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - run: brew install ninja
      - uses: actions/cache@v6
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-apple-${{ env.HAYLEN_REF }}
      - name: Restore the content keys of the app and the App Store Connect API key
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
          ASC_API_KEY: ${{ secrets.ASC_API_KEY }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
          echo "ASC_KEY_PATH=$RUNNER_TEMP/AuthKey.p8" >> "$GITHUB_ENV"
          printf '%s' "$ASC_API_KEY" > "$RUNNER_TEMP/AuthKey.p8"
      - name: Generate the Xcode project with the protected release and the bootstrap
        run: |
          project=$(haylen/haylen --json app prepare "$APP" --target ${{ matrix.target }} --config Release | jq -r .result.project)
          echo "PROJECT=$project" >> "$GITHUB_ENV"
      - name: Archive the app and export it for App Store Connect, signed in the cloud
        env:
          ASC_ISSUER_ID: ${{ secrets.ASC_ISSUER_ID }}
          ASC_KEY_ID: ${{ secrets.ASC_KEY_ID }}
          NOTARIZATION_TEAM_ID: ${{ secrets.NOTARIZATION_TEAM_ID }}
          PLATFORM: ${{ matrix.platform }}
        run: |
          authentication=(-allowProvisioningUpdates -authenticationKeyPath "$ASC_KEY_PATH" -authenticationKeyID "$ASC_KEY_ID" -authenticationKeyIssuerID "$ASC_ISSUER_ID")
          archive="$RUNNER_TEMP/$PLATFORM.xcarchive"
          xcodebuild archive -project "$PROJECT" -scheme App -configuration Release \
            -destination "generic/platform=$PLATFORM" -archivePath "$archive" "${authentication[@]}" \
            CODE_SIGN_STYLE=Automatic DEVELOPMENT_TEAM="$NOTARIZATION_TEAM_ID"
          options="$RUNNER_TEMP/export.plist"
          plutil -create xml1 "$options"
          plutil -insert method -string app-store-connect "$options"
          plutil -insert destination -string export "$options"
          plutil -insert signingStyle -string automatic "$options"
          plutil -insert teamID -string "$NOTARIZATION_TEAM_ID" "$options"
          xcodebuild -exportArchive -archivePath "$archive" -exportPath dist -exportOptionsPlist "$options" "${authentication[@]}"
      - name: Upload the archive to App Store Connect
        if: startsWith(github.ref, 'refs/tags/')
        env:
          ASC_API_KEY: ${{ secrets.ASC_API_KEY }}
          ASC_ISSUER_ID: ${{ secrets.ASC_ISSUER_ID }}
          ASC_KEY_ID: ${{ secrets.ASC_KEY_ID }}
          TYPE: ${{ matrix.type }}
        run: |
          mkdir -p "$HOME/.appstoreconnect/private_keys"
          printf '%s' "$ASC_API_KEY" > "$HOME/.appstoreconnect/private_keys/AuthKey_$ASC_KEY_ID.p8"
          xcrun altool --upload-app --type "$TYPE" --file dist/*.ipa --apiKey "$ASC_KEY_ID" --apiIssuer "$ASC_ISSUER_ID"
      - uses: actions/upload-artifact@v7
        with:
          name: ${{ matrix.target }}
          path: dist/*.ipa

  android:
    runs-on: ubuntu-latest
    env:
      HAYLEN_RELEASE_STORE_PASSWORD: ${{ secrets.ANDROID_KEYSTORE_PWD }}
      HAYLEN_RELEASE_KEY_ALIAS: ${{ secrets.ANDROID_KEY_ALIAS }}
      HAYLEN_RELEASE_KEY_PASSWORD: ${{ secrets.ANDROID_KEY_PWD }}
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - uses: actions/setup-java@v6
        with:
          distribution: temurin
          java-version: "25"
      - uses: android-actions/setup-android@v4.0.4
      # The NDK and the platform that the build guide of the engine lists for Android.
      - run: sdkmanager "ndk;30.0.16248370" "platforms;android-37.2"
      # The desktop SDK of this system holds the content tool of the release.
      - run: sudo apt-get update && sudo apt-get install -y ninja-build libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev libegl-dev libwayland-dev libxkbcommon-dev libdbus-1-dev libibus-1.0-dev libudev-dev
      - uses: actions/cache@v6
        id: engine
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-android-${{ env.HAYLEN_REF }}
      - name: Build the Android SDK and the desktop SDK of this system
        if: steps.engine.outputs.cache-hit != 'true'
        run: |
          haylen/haylen --verbose sdk build --target android
          haylen/haylen --verbose sdk build --target linux
      - name: Restore the content keys of the app and the upload key
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
          ANDROID_KEYSTORE: ${{ secrets.ANDROID_KEYSTORE }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
          echo "HAYLEN_RELEASE_STORE_FILE=$RUNNER_TEMP/upload.jks" >> "$GITHUB_ENV"
          echo "$ANDROID_KEYSTORE" | base64 --decode > "$RUNNER_TEMP/upload.jks"
      - name: Build the App Bundle signed with the upload key
        run: haylen/haylen --verbose app package "$APP" --target android --format bundle -o dist
      - uses: actions/upload-artifact@v7
        with:
          name: android
          path: dist/*.aab

  windows:
    runs-on: windows-latest
    env:
      HAYLEN_WINDOWS_CERTIFICATE_PASSWORD: ${{ secrets.WINDOWS_CERTIFICATE_PWD }}
      HAYLEN_WINDOWS_TIMESTAMP_URL: ${{ secrets.WINDOWS_TIMESTAMP_URL }}
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - uses: ilammy/msvc-dev-cmd@v1.13.0
      - run: pip install ninja
      - uses: actions/cache@v6
        id: engine
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-windows-${{ env.HAYLEN_REF }}
      - name: Build the desktop SDK of Windows
        if: steps.engine.outputs.cache-hit != 'true'
        shell: pwsh
        run: haylen\haylen.cmd --verbose sdk build --target windows
      - name: Restore the content keys of the app and the code signing certificate
        shell: pwsh
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
          WINDOWS_CERTIFICATE: ${{ secrets.WINDOWS_CERTIFICATE }}
        run: |
          $keys = Join-Path $env:RUNNER_TEMP "haylen-keys"
          $certificate = Join-Path $env:RUNNER_TEMP "certificate.pfx"
          "HAYLEN_KEYS_DIR=$keys" >> $env:GITHUB_ENV
          "HAYLEN_WINDOWS_CERTIFICATE=$certificate" >> $env:GITHUB_ENV
          New-Item -ItemType Directory -Force -Path $keys | Out-Null
          [IO.File]::WriteAllBytes("$env:RUNNER_TEMP\keys.tar", [Convert]::FromBase64String($env:HAYLEN_CONTENT_KEYS))
          tar -x -f "$env:RUNNER_TEMP\keys.tar" -C $keys
          [IO.File]::WriteAllBytes($certificate, [Convert]::FromBase64String($env:WINDOWS_CERTIFICATE))
      - name: Build the signed installer
        shell: pwsh
        run: haylen\haylen.cmd --verbose app package $env:APP --target windows --format installer -o dist
      - uses: actions/upload-artifact@v7
        with:
          name: windows
          path: dist/

  linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      - run: sudo apt-get update && sudo apt-get install -y ninja-build libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev libegl-dev libwayland-dev libxkbcommon-dev libdbus-1-dev libibus-1.0-dev libudev-dev dpkg-dev
      # The AppImage generator of CPack needs a newer CMake than the one of the runner.
      - run: pip install cmake
      - uses: actions/cache@v6
        id: engine
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-linux-${{ env.HAYLEN_REF }}
      - name: Build the desktop SDK of Linux
        if: steps.engine.outputs.cache-hit != 'true'
        run: haylen/haylen --verbose sdk build --target linux
      - name: Restore the content keys of the app
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
      - name: Build the Debian package and the AppImage
        run: haylen/haylen --verbose app package "$APP" --target linux --format installer -o dist
      - uses: actions/upload-artifact@v7
        with:
          name: linux
          path: dist/

  web:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7
        with:
          path: ${{ env.APP }}
      - uses: actions/checkout@v7
        with:
          repository: haylen-org/haylen
          ref: ${{ env.HAYLEN_REF }}
          path: haylen
      - uses: astral-sh/setup-uv@v10.3.3
      # The desktop SDK of this system holds the content tool of the release.
      - run: sudo apt-get update && sudo apt-get install -y ninja-build libx11-dev libxi-dev libxcursor-dev libxext-dev libxrandr-dev libgl1-mesa-dev libegl-dev libwayland-dev libxkbcommon-dev libdbus-1-dev libibus-1.0-dev libudev-dev
      - uses: actions/cache@v6
        id: engine
        with:
          path: |
            .cache/cpm
            haylen/.tools
            haylen/build/sdk
          key: haylen-web-${{ env.HAYLEN_REF }}
      - name: Build the web SDK and the desktop SDK of this system
        if: steps.engine.outputs.cache-hit != 'true'
        run: |
          haylen/haylen --verbose sdk build --target web
          haylen/haylen --verbose sdk build --target linux
      - name: Restore the content keys of the app
        env:
          HAYLEN_CONTENT_KEYS: ${{ secrets.HAYLEN_CONTENT_KEYS }}
        run: |
          HAYLEN_KEYS_DIR="$RUNNER_TEMP/haylen-keys"
          echo "HAYLEN_KEYS_DIR=$HAYLEN_KEYS_DIR" >> "$GITHUB_ENV"
          mkdir -p "$HAYLEN_KEYS_DIR"
          echo "$HAYLEN_CONTENT_KEYS" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
      - name: Build the site with the protected release and verify it
        run: |
          site=$(haylen/haylen --json app build "$APP" --target web --config Release | jq -r .result.product.path)
          mkdir -p dist
          cp -R "$site"/. dist/
      - uses: actions/upload-artifact@v7
        with:
          name: web
          path: dist/
```

## How the jobs work

**The engine.** Each job checks out the app, and next to it, in `haylen/`, the engine at `HAYLEN_REF`. It runs the command `haylen` from there through uv, which `setup-uv` installs, so the build products land in `haylen/build/apps/<app>-<hash>/` and the engine SDKs in `haylen/build/sdk/`. The builds of apps never compile the engine itself, so each job first builds the SDKs it needs with `haylen sdk build`: the SDK of its target, and the desktop SDK of the runner, whose content tool builds the protected release. The command `sdk build` compiles the engine in a build tree under `haylen/build/engine`, which the caches do not keep, so the step runs only when the cache of the engine at `HAYLEN_REF` restored no SDKs. The caches keep the SDKs between runs of the same engine, with the sources of the dependencies and the tools that the command downloads or builds, such as the shader compiler. The job `engine-apple` builds the Apple SDK once for the three Apple jobs, and the desktop SDK that holds the content tool of this Mac, after it installs the Metal toolchain of Xcode, which the Apple builds of the engine need.

**Stable shards.** A runner starts without the release before it in its build folder. Stores compare files, so a job that keeps the shards of unchanged content byte for byte keeps the patches of stores small. Such a job first fetches the last build that shipped, such as the artifact of the last tagged run, the depots of the live Steam build or an unpacked APK of the store. Its build or its package then names that folder with `--previous-release`, such as `haylen app package <app> --target <target> --format steam --previous-release <folder>`, so the release of the build follows the release that shipped, as the [content guide](../content/content.md#building-content) describes. Without it a release still installs and updates, but the files that stores compare change more than the content did.

**Content keys.** Every job restores the key folder of the app into the temporary folder of the runner and names it with `HAYLEN_KEYS_DIR`. There the tooling never creates keys. So a missing or wrong secret stops the build, instead of producing a release that installed apps cannot read.

**The Apple projects.** The Apple backend has no command that archives or exports an app, so the Apple jobs hand the project it generates to Xcode. The command `app prepare --config Release` generates the Xcode project of the target with the protected release and its bootstrap, without building it, and prints the path of `App.xcodeproj` in its JSON result, which `jq` reads. Every Apple app has the one scheme `App`. The variable `HAYLEN_APPLE_TEAM` names the team that signs the apps of devices, unless `platforms.apple.team` of `haylen.json` names it.

**macOS with Developer ID.** The job imports the certificate into a keychain of its own. It archives the scheme `App` with the Developer ID identity and the hardened runtime of the generated project, and exports the app with the method `developer-id`. It notarizes a zip of the app with `notarytool` and staples the ticket to the app. It packs the stapled app into a disk image with a link to `Applications`, signs the image, and notarizes and staples the image too. So the app opens without a warning on a Mac without a connection.

**Mac App Store.** The job imports both Mac App Store certificates and installs the provisioning profile under its UUID. It archives with the distribution identity and the profile, and exports the installer package with the method `app-store-connect`, which signs the package with the installer identity. On a tag it uploads the package with `altool` and the API key.

**iOS and tvOS.** The jobs archive the scheme `App` of the targets `ios` and `tvos` with automatic signing. The API key lets Xcode sign in the cloud, with the distribution certificate and the profiles of the team. The jobs export the `.ipa` with the method `app-store-connect` and upload it on a tag.

**Android.** The job installs the NDK and the platform that the [build guide](build.md#requirements) lists for Android, and builds the Android SDK and the desktop SDK of the runner. The command `app package --format bundle` builds the app in the Release configuration, with the protected release and the library of the app for every ABI, verifies it and copies the App Bundle into `dist`. Gradle, which the tooling downloads, signs the bundle with the upload key that the variables `HAYLEN_RELEASE_STORE_FILE`, `HAYLEN_RELEASE_STORE_PASSWORD`, `HAYLEN_RELEASE_KEY_ALIAS` and `HAYLEN_RELEASE_KEY_PASSWORD` describe, which never reach a file of the generated project.

**Windows and Linux.** The command `app package --format installer` generates the project of the platform, builds it in the Release configuration, stops on anything a release never ships and runs CPack. The Windows project signs the executable, the installer and its uninstaller with the certificate of `HAYLEN_WINDOWS_CERTIFICATE`. The packages of Linux link the system libraries of the runner, so they run on that distribution and newer ones.

**Steam.** A job of Windows or Linux stages the depots of Steam with the format `steam` of the same command, such as `app package <app> --target windows --format steam --option appId=480 --option depots=windows=481 -o dist/steam`, with a depot for every named pack of the app, and uploads them with `steamcmd` and the app build script it writes. The [distribution guide](distribution.md) lists its options.

**Web.** The command `app build --config Release` makes the site with the protected release in `app/` and the runtime linked for the app, and its verification stops on anything a release never holds. Its JSON result names the folder of the site, which `jq` reads. The artifact is the whole site, which any static web server serves.

## Checking the results

| Result | Command |
| --- | --- |
| The macOS disk image and app | `spctl --assess --type open --context context:primary-signature -v my-game.dmg`, `xcrun stapler validate my-game.dmg` and `spctl --assess -v "My Game.app"` |
| The Mac App Store package | `pkgutil --check-signature "My Game.pkg"` |
| The iOS and tvOS archives | The build appears in TestFlight of App Store Connect after its processing. |
| The App Bundle | `jarsigner -verify -verbose app-release.aab` shows the certificate of the upload key. |
| The Windows installer | `signtool verify /pa /v my-game-1.0.0-setup.exe` |
| The Linux packages | `dpkg-deb --info my-game_1.0.0_amd64.deb`, and the AppImage runs with `--appimage-extract-and-run` on a machine without FUSE. |
| The web site | `haylen serve <site>` serves it locally with the headers that WebAssembly pages need. |
