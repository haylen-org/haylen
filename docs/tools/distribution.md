# Distributing apps

A Haylen app is a package: `app.json`, the Lua modules under `source/` and the assets under `content/`. The engine is compiled once into prebuilt artifacts for every platform, and the project of each platform, which starts as a copy of the template of its platform, belongs to the developer. The script `haylen.py` writes the name, identifier, version, orientation and splash screen of `app.json`, the package and the plugins of the app into one generated folder of that project, builds the project where it is and launches it. A Lua app never compiles the engine, and the same package runs on every platform.

This guide covers the commands, the engine artifacts, the templates, the platform projects and their generated folder, the check of a built app, the privacy manifest, native libraries, splash screens, the web loader, the local web server and the platforms Haylen supports. The [plugin guide](../platform/plugins.md) covers plugins, which add native capabilities to apps, the [build guide](build.md) covers building the engine itself and the [embedding guide](embedding.md) covers C++ projects that compile the engine through CMake.

## Quick start

```sh
haylen new ~/apps/my-game --name "My Game" --identifier com.example.mygame
haylen run ~/apps/my-game
haylen run ~/apps/my-game --platform ios-simulator
haylen run ~/apps/my-game --platform android
haylen run ~/apps/my-game --platform web
```

The command `new` creates the app from the starter app and copies every platform template into its `platform/` folder, where the projects belong to the developer. The command `run` without `--platform` starts the desktop player of this machine in development mode, which reloads edited scripts and assets. With `--platform` it builds the engine artifacts when they are missing or stale, prepares the project of the platform, builds it, checks what the built app lacks and launches it on a simulator, an emulator, a device, this Mac or a local web server, and streams the output of the app.

A C++ app is a CMake project that compiles the engine. The option `--kind cpp` of `new` creates one, and `run-cpp` builds and runs it:

```sh
haylen new ~/apps/my-cpp-game --kind cpp
haylen run-cpp ~/apps/my-cpp-game
haylen run-cpp ~/apps/my-cpp-game --platform android
```

## Commands

The file `haylen` at the root of the repository, a shell script for macOS and Linux, and `haylen.cmd` next to it for Windows, run `haylen.py` with every argument they get, from any folder and through a link. With the root on `PATH`, every command below is `haylen <command>`, and `python3 haylen.py <command>` does the same. The option `haylen --version` prints `Haylen` followed by the version of the engine that `engine/VERSION` holds.

| Command | Purpose |
| --- | --- |
| `engine [--platform apple\|android\|web\|desktop\|all] [--config]` | Builds the prebuilt engine artifacts into `build/artifacts/`. |
| `new <folder> [--kind lua\|cpp] [--name] [--identifier] [--orientation]` | Creates a Lua or C++ app from the layers of its [template](#templates), with a project of every platform template under `platform/`. |
| `platform add <app> <template>` | Creates `platform/<template>` of an app from the template of a platform for the kind of the app. |
| `platform diff <app> --template` | Shows how `platform/<template>` of an app differs from the current template for the kind of the app, without changing anything. |
| `run <app> [--platform] [--device] [--config] [--engine-config]` | Runs an app, in the desktop player by default or built for a platform. |
| `prepare <app> --platform [--config] [--engine-config]` | Writes the folder `haylen/` of the project of a platform and nothing else, or the site of the web, with the package as it is or, with `--config Release`, with the protected release of the app. |
| `xcodegen [app] [--platform] [--config] [--template]` | Writes `haylen/` of the Apple project of an app and generates its `App.xcodeproj` again from `project.yml`, or generates the project of the Apple template. |
| `check <app> --platform [--config] [--coop]` | Checks the last build of an app against what the engine and its plugins need, and prints every missing requirement with the snippet that adds it. |
| `dist <app> --platform windows\|linux [-o] [--engine-config]` | Builds the packages that ship a Windows or Linux app in the Release configuration: the installer of Windows, or the `.deb` and the AppImage of Linux. |
| `android-key <app> [--release\|--debug] [--alias] [--password] [--dname] [--force]` | Creates the upload key of release builds, or a debug key, in `platform/android/keystore/` of an app, with its certificate and the properties that sign its builds. |
| `steam <app> --platform macos\|windows\|linux --app-id <id> --depot <name>=<id> [--branch] [--output]` | Builds the release of an app for a desktop platform and stages its Steam depots, one for the app and one for each pack, with the build scripts of `steamcmd`. |
| `run-cpp <project> [--platform] [--target] [--device] [--config] [--engine-config] [--build-only]` | Builds a C++ project that compiles the engine through CMake and runs it on this machine, in the browser, on Mac Catalyst, iOS, tvOS, their simulators or Android. |
| `package <app> [-o app.zip]` | Zips `app.json`, `source/` and `content/` of an app. |
| `content build\|verify\|inspect\|diff\|publish\|compact\|keys <app> ...` | Builds, verifies, inspects, compares, publishes and compacts the protected releases of an app, and shows or rotates its keys, as the [content guide](../content/content.md#the-content-tool) describes. |
| `shaders <app> [--force]` | Compiles the shaders under `content/shaders/` of an app into `.shader` files. |
| `serve <folder> [--host] [--port] [--coep] [--coop] [--open]` | Serves a folder with the headers WebAssembly pages need. |
| `plugin add <folder\|repository> [--ref] [--app]` | Copies a plugin folder, or a plugin repository at a branch, tag or commit, into `plugins/` of an app and lists it in its `app.json`. |
| `plugin remove <id> [--app]` | Deletes a plugin from an app and from its `app.json`. |
| `plugin list [--app]` | Lists the plugins of an app with their status. |
| `plugin new <folder> [--id]` | Creates a plugin from `templates/plugin/`. |

The argument `app` is the folder of an app, relative to the current folder or absolute. A command without an app, or with a path that holds no `app.json`, stops with an error that says what an app folder is, and never runs another app in its place.

### engine

```sh
haylen engine
haylen engine --platform apple --config Release
```

Builds the artifacts of one platform, or of every platform with `--platform all`, which is the default. The option `--config` is `Release` by default. Each build records the configuration and a SHA-256 hash of the engine sources in `build/artifacts/manifest.json`, and `run` rebuilds the artifacts of a platform before it uses them whenever the manifest does not match the current sources or the configuration that `--engine-config` asks for. The hash covers every file under `engine/` except the tests, the benchmark and build outputs, so any change to the engine code, its CMake files, its shaders or its platform projects makes the artifacts stale.

### new

```sh
haylen new ~/apps/space-race --orientation portrait
haylen new ~/apps/space-race-cpp --kind cpp
```

Copies the [template](#templates) of the kind of app that `--kind` names, `lua` by default or `cpp`, into the folder: first `templates/common`, then `templates/lua` or `templates/cpp`, whose files replace the common files of the same path. The app gets its package, its `.gitignore`, `.editorconfig` and `.clang-format`, and the project of every platform template of its kind under `platform/<platform>`: `apple`, `android`, `linux`, `web` and `windows` for a Lua app, and `android` and `web` for a C++ app, whose CMake project builds the other platforms itself. Then it writes the name, identifier and orientation into `app.json` and turns the window and design resolution upright for `portrait`. The projects belong to the developer, who edits them as the company wants, and haylen.py builds them where they are and writes only their `haylen/` folder, as [platform projects](#platform-projects) describes. The name defaults to the folder name in title case, the identifier to `com.example.<folder>` and the orientation to `landscape`. The folder must not exist or must be empty.

### run

```sh
haylen run ~/apps/my-game
haylen run ~/apps/my-game --platform macos
haylen run ~/apps/my-game --platform catalyst
haylen run ~/apps/my-game --platform ios-simulator --device "iPad Pro 13-inch (M5)"
haylen run ~/apps/my-game --platform tvos-simulator
haylen run ~/apps/my-game --platform android --device emulator-5554
haylen run ~/apps/my-game --platform web --coep off --open
```

Before anything else, `run` compiles the shaders of the app whose sources changed, as [shaders](#shaders) describes. Without `--platform`, `run` builds the `haylen` player in the configuration of `--engine-config`, `Release` by default, in the build tree of this machine (`build/<host>-<config>`), builds or copies the [native libraries](#native-libraries) of the app for this machine into `native/development` of the [build folder of the app](#platform-projects), and starts the player with `--dev`, `--native` and that folder when there are libraries, and the app folder. While the player runs it compiles every shader source that changes again, which the player then reloads. With `--platform` it prepares the project of the platform as [platform projects](#platform-projects) describes and then:

| Platform | Build | Launch |
| --- | --- | --- |
| `macos` | Generates `App.xcodeproj` again when it may, then `xcodebuild` of the `macOS` scheme into the build folder of the app. | Starts the executable of the app bundle, whose log the engine writes to the standard output, so it stays in the terminal. |
| `catalyst` | `xcodebuild` of the `iOS` scheme for the Mac Catalyst destination. | Starts the executable of the Mac Catalyst bundle and streams its log with `log stream`. |
| `ios-simulator`, `tvos-simulator` | `xcodebuild` of the `iOS` or `tvOS` scheme for the simulator. | `xcrun simctl boot`, `install` and `launch --console-pty`, on the simulator named by `--device` (a name or an id), on a booted one, or on the first iPhone or Apple TV available, and streams the log of the app with `log stream` inside the simulator. |
| `ios`, `tvos` | `xcodebuild` for the device, signed with the team in the `HAYLEN_APPLE_TEAM` environment variable. | `xcrun devicectl device install app` and `device process launch --console` on the device id given with `--device`. |
| `android` | Gradle `:app:assembleDebug`, or `assembleRelease` with `--config Release`, into the build folder of the app. | `adb install -r` of the newest APK of the configuration, `adb shell am start` of its package and the log of the app process, on the device or emulator serial given with `--device`, which may be left out when only one is connected. |
| `web` | Copies the files of the web project into the site with the prebuilt runtime and `app.zip`, or with `--config Release` the protected release in `app/` and runtimes linked for the app, and writes `config.json` next to them. | Serves the site on `--port` (8000 by default) with the `--coep` and `--coop` policies, and opens it with `--open`. |
| `windows`, `linux` | CMake builds the project with Ninja, or the default generator on Windows without Ninja, into `cmake/<platform>-<config>` of the build folder of the app, and installs the app into `build/apps/<app>-<hash>/installed/<platform>/`, as [Windows and Linux](#windows-and-linux) describes. | Runs the installed executable, `<executable>.exe` on Windows and `bin/<executable>` on Linux, which plays the `app` folder next to it. |

The option `--config` is the configuration of the platform project (`Debug` by default), and `--engine-config` the configuration of the engine, of the artifacts the project links and of the desktop player (`Release` by default). A `Debug` engine is only for debugging the engine itself, because it runs many times slower. The command `xcodebuild` and Gradle build with the jobs of `--jobs`. The command `run` streams the output of the app until it exits or Ctrl+C stops it. The engine logs to the standard output on desktops, to the log of the process on Android, which `adb logcat --pid` streams, and to the unified log on iOS, tvOS and Mac Catalyst, with the bundle identifier of the app as the subsystem and `haylen` as the category, which logcat on Android also uses as the tag. On Mac Catalyst and the simulators `run` streams those lines next to the standard output and error of the process, with warnings and errors on stderr, and keeps streaming for a second after the app ends so its last lines arrive. The log of an app on a device shows in Console.app, while `run` shows the standard output and error of its process.

The command `run` builds the artifacts of the engine it needs when they are missing or older than the engine sources, and on Apple platforms only the slice of `Haylen.xcframework` it runs on, such as the slice of the iOS simulator, so a change to the engine rebuilds one slice before a run. The commands `prepare` and `xcodegen` and the command `engine` build every slice, since a project of the developer may build for any Apple platform.

### run-cpp

```sh
haylen run-cpp ~/apps/my-cpp-game
haylen run-cpp ~/apps/my-cpp-game --platform web
haylen run-cpp ~/apps/my-cpp-game --platform ios-simulator --device "iPhone 17"
haylen run-cpp ~/apps/my-cpp-game --platform catalyst
haylen run-cpp ~/apps/my-cpp-game --platform android --device emulator-5554
```

Configures the CMake project in the folder it names, relative to the current folder or absolute, which adds the engine and calls `haylen_add_app`, as the [embedding guide](embedding.md) shows, in `build/cpp/<project>-<hash>/<platform>-<config>`, builds its app target and runs it. The part `<hash>` is the start of the SHA-256 hash of the absolute project folder, so projects in folders with the same name never share a build tree. The target is `main`, the name that the [C++ template](#cpp) gives it, unless `--target` names another one. The command hands the project the engine of this checkout in `HAYLEN_ENGINE_SOURCE`, the shader compiler in `HAYLEN_SOKOL_SHDC` and the dependency cache of the engine builds in `CPM_SOURCE_CACHE`, so a project anywhere on the machine compiles the engine without downloading its dependencies again. With `--build-only` it builds and checks the app without launching it, as continuous integration does on Windows and Linux. C++ projects compile the engine from source, so only Android uses an engine artifact, for its Java classes. The option `--config Release` ships the protected release of the app, built with the content tool of the desktop artifacts, and inspects the built app before it launches, as the [embedding guide](embedding.md#protected-releases) describes.

| Platform | Build | Launch |
| --- | --- | --- |
| This machine | Ninja, or the default generator on Windows without Ninja, with the macOS minimum of the engine as `CMAKE_OSX_DEPLOYMENT_TARGET` on a Mac. | Runs the executable. |
| `web` | Emscripten for WebGPU and for WebGL2 into `build/cpp/<project>-<hash>/web`, with an `index.html` that picks the backend the browser supports. | Serves that folder like `serve`. |
| `ios-simulator`, `tvos-simulator` | The Xcode generator with the iOS or tvOS simulator SDK for the architecture of this Mac, which compiles the launch screen and signs the bundle. | Like `run`, on the simulator named by `--device`, a booted one or the first one available. |
| `ios`, `tvos` | The Xcode generator with the device SDK, signed with the team in `HAYLEN_APPLE_TEAM`. | Like `run`, on the device named by `--device`. |
| `catalyst` | Ninja with `engine/cmake/haylen-catalyst.toolchain.cmake` for the architecture of this Mac. `haylen_add_app` signs the bundle ad hoc after linking, as Xcode does, because macOS opens the window of an unsigned Mac Catalyst app at the top left of the screen instead of where the app asks. | Like `run`: starts the executable and streams its log. |
| `android` | Ninja with the NDK for the ABI of the device named by `--device`, or of the only one connected, into `bin/<target>/lib<target>.so`. Then it prepares the Android project of the package that `bin/<target>/package.txt` names, its `platform/android` or the copy of the template, with the library in `haylen/jniLibs/<abi>` and `library` of `haylen/haylen.properties` set to the target, which makes `HaylenActivity` load the library of the app and leaves the Lua player of the haylen library out of the APK. The haylen library comes from the artifacts, built with `--engine-config` when they are missing or stale. | Like `run`. |

### package

```sh
haylen package ~/apps/my-game -o dist/my-game.zip
```

Compiles the shaders of the app whose sources changed, then zips `app.json`, `source/` and `content/` of the app, with `plugin.json` and `source/` of every plugin that `app.json` lists, and nothing else in its folder, into `app.zip` or the file `-o` names. The desktop player runs the zip with `haylen app.zip`, and a web page hands it to the runtime as described in [the web loader](#web-loader).

### shaders

```sh
haylen shaders ~/apps/my-game
haylen shaders ~/apps/my-game --force
```

Compiles every source under `content/shaders/` that declares an `@program` into a `.shader` file next to it, for every backend the engine runs on, with `sokol-shdc` from `.tools/`. Sources without a program are files the others include. A shader is compiled again when its `.shader` file is older than any source of the app or the shader library, and `--force` compiles them all. A compile error names the file, the line and the program, and fails the command. The [shader guide](../graphics/shaders.md) explains the sources and the files. The web editor cannot compile shaders, so apps ship their `.shader` files in their package.

### serve

```sh
haylen serve dist/my-game --port 8000
haylen serve dist/my-game --coep off --open
haylen serve dist/my-game --host 192.168.1.20
```

Serves a folder at `http://127.0.0.1:<port>/` with a threading Python server. The option `--host` listens on another address of this machine instead, such as its LAN address, so phones and other computers open the page. Browsers offer `AudioWorklet` only to pages served over https or from localhost, so those pages run without sound. The commands `run --platform web` and `run-cpp --platform web` take the same options. The server sends:

| Header | Value |
| --- | --- |
| `Cross-Origin-Opener-Policy` | None by default, `same-origin-allow-popups` with `--coop same-origin-allow-popups`, and `same-origin` with `--coop same-origin` |
| `Cross-Origin-Embedder-Policy` | `require-corp` by default, `credentialless` with `--coep credentialless`, and none with `--coep off` |
| `Cross-Origin-Resource-Policy` | `cross-origin` |
| `Access-Control-Allow-Origin` | `*` |
| `Cache-Control` | `no-cache` |
| `ETag`, `Accept-Ranges` | A strong validator of each file, and `bytes`, since the server answers one byte range of a file with `206`, `Content-Range` and the bytes asked for, and a range whose `If-Range` names another validator with the whole file, as a content delivery network serves the shards of a protected release. |

The web runtime is single-threaded, so its pages need no cross-origin isolation, and the server sends no opener policy by default: `same-origin` would put the sign-in and payment popups of plugins into a browsing context group of their own, which cuts them off from the page, as the [plugin guide](../platform/plugins.md#web-screens) explains. The option `--coop same-origin` together with a `--coep` policy makes a page cross-origin isolated, for pages that use `SharedArrayBuffer` or threads of their own, and `--coop same-origin-allow-popups` keeps the popups of the page linked to it. Pages that load third-party scripts, such as sign-in libraries, need `--coep credentialless` or `--coep off`, because those scripts do not send the resource policy that `require-corp` asks for. The server names the MIME type of `.wasm` (`application/wasm`), `.js` and `.mjs` (`text/javascript`), `.json`, `.zip`, `.html`, `.css`, `.svg` and `.png` explicitly. A request for a file that also exists as `<file>.br` or `<file>.gz` receives the compressed copy with its `Content-Encoding` when the browser accepts that encoding.

### plugin

```sh
haylen plugin list
haylen plugin add ~/plugins/camera-scanner --app ~/apps/my-game
haylen plugin list --app ~/apps/my-game
haylen plugin remove camera-scanner --app ~/apps/my-game
haylen plugin new ~/plugins/my-plugin
```

The command `plugin add` copies a plugin folder, or the root of a plugin repository at the branch, tag or commit that `--ref` names, into `plugins/<id>/` of the app and lists it in the `plugins` section of `app.json` with the defaults of its parameters and an empty text for each required one. The command `plugin remove` deletes both, `plugin list` lists the plugins of an app with the problems that keep them from building, and `plugin new` creates a plugin from `templates/plugin/`. The option `--app` defaults to the current folder. The [plugin guide](../platform/plugins.md) describes the commands, the `plugins` section of `app.json`, the checks that `run` makes before it builds and the plugin format.

### platform

```sh
haylen platform add ~/apps/my-game android
haylen platform diff ~/apps/my-game --template apple
```

The command `platform add` creates `platform/<template>/` of an app from the template of the platform, the common layer and the layer of the kind of the app, for an app whose folder has none, which then builds that project instead of the copy that haylen.py keeps. A folder that holds `CMakeLists.txt` is a C++ app, and any other a Lua app, and a template that the kind lacks, such as `apple` for a C++ app, stops the command with the templates the kind has. The command `platform diff` compares `platform/<template>/` of an app with the same layered template and prints the files that only one of them has and a unified diff of every text file that differs, leaving out `haylen/`, build outputs and the state of Xcode for each person, so the developer adopts what they want of a newer template. It changes nothing.

### prepare

```sh
haylen prepare ~/apps/my-game --platform ios-simulator
haylen prepare ~/apps/my-game --platform android
```

Builds the engine artifacts of the platform when they are missing or stale and writes the folder `haylen/` of the project of the platform, and nothing else, which is enough to open the project in Xcode, Android Studio or an editor that opens CMake projects and build, run or archive it there. On the web it makes the site. The native libraries of an Apple project are the ones of the platform it names. The option `--config Release` writes the protected release and the bootstrap of the app, as [release builds](#release-builds) describe, which is what an archive for a store needs.

### xcodegen

```sh
haylen xcodegen ~/apps/my-game
haylen xcodegen --template
```

Prepares the Apple project of an app, with the native libraries of `--platform`, `macos` by default, and the package of `--config`, `Debug` by default, and generates its `App.xcodeproj` again from `project.yml` with the XcodeGen that haylen.py pins, after the developer changed `project.yml`, as [generating App.xcodeproj](#generating-appxcodeproj) describes. The option `--template` generates the `App.xcodeproj` of the Apple template of the engine again, in a copy with the `haylen/` folder of the starter app, after a change to its `project.yml`.

### android-key

```sh
haylen android-key ~/apps/my-game
haylen android-key ~/apps/my-game --debug
haylen android-key ~/apps/my-game --alias store --password "a long secret" --dname "CN=My Company, O=My Company, C=US"
```

Creates the upload key that signs the release builds of an app, or with `--debug` a debug key of the project, which signs its debug builds in place of the debug key of the Android SDK, so every machine of a team builds with the same signature. It runs `keytool -genkeypair` for an RSA key of 2048 bits that stays valid for 10000 days in a PKCS12 keystore, with the alias of `--alias` and the password of `--password` for the keystore and the key, both `upload` by default, and the distinguished name of `--dname`, `CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR` by default. Then it exports the certificate as PEM with `keytool -exportcert -rfc`, which is the file a store asks for when it registers or resets an upload key. The password reaches `keytool` through the environment, so the printed commands never show it, and `keytool` asks for at least 6 characters. The command finds `keytool` in the JDK of `JAVA_HOME`, or else on `PATH`.

The key goes into `platform/android/keystore/` of the app as `<type>.jks`, `<type>.pem` and `<type>.properties`, where the type is `release` or `debug`. An app without `platform/android/` gets the project from the template first. The command refuses to replace a key that the project has unless `--force` is given, since an app signed with another key cannot update the installed one, and the `.gitignore` of the app keeps the folder out of the repository.

The app module of the Android template signs each build type with the key that `keystore/<type>.properties` describes, which names the keystore next to it:

```properties
storeFile=release.jks
storePassword=upload
keyAlias=upload
keyPassword=upload
```

Continuous integration, where the folder does not exist, gives a key through Gradle properties or environment variables instead:

| Gradle property | Environment variable | Value |
| --- | --- | --- |
| `haylen.release.storeFile` | `HAYLEN_RELEASE_STORE_FILE` | The keystore, as an absolute path or a path relative to the Android project. |
| `haylen.release.storePassword` | `HAYLEN_RELEASE_STORE_PASSWORD` | The password of the keystore. |
| `haylen.release.keyAlias` | `HAYLEN_RELEASE_KEY_ALIAS` | The alias of the key. |
| `haylen.release.keyPassword` | `HAYLEN_RELEASE_KEY_PASSWORD` | The password of the key. |

The debug key takes the same names with `debug`, such as `haylen.debug.storeFile` and `HAYLEN_DEBUG_STORE_FILE`. A key needs its four values, the properties file wins over the Gradle properties, and a Gradle property wins over its environment variable. A release build without an upload key stops before it compiles with a message that names both ways to add one, and a debug build without a key of the project signs with the debug key of the Android SDK. A workflow keeps the keystore as Base64 in a secret and writes it to a file before the build:

```yaml
env:
  HAYLEN_RELEASE_STORE_FILE: ${{ runner.temp }}/release.jks
  HAYLEN_RELEASE_STORE_PASSWORD: ${{ secrets.ANDROID_KEYSTORE_PWD }}
  HAYLEN_RELEASE_KEY_ALIAS: ${{ secrets.ANDROID_KEY_ALIAS }}
  HAYLEN_RELEASE_KEY_PASSWORD: ${{ secrets.ANDROID_KEY_PWD }}
steps:
  - run: echo "${{ secrets.ANDROID_KEYSTORE }}" | base64 --decode > "$HAYLEN_RELEASE_STORE_FILE"
```

The command `base64 < platform/android/keystore/release.jks` prints the text of the `ANDROID_KEYSTORE` secret, the [installers in CI](../ci-installers.md) guide builds the App Bundle with these secrets, and `apksigner verify --print-certs` of the build tools of the Android SDK shows the certificate that signed an APK.

### dist

```sh
haylen dist ~/apps/my-game --platform windows
haylen dist ~/apps/my-game --platform linux -o ~/releases/my-game
```

Builds what ships a Windows or Linux app on this machine: it prepares the project of the platform in the Release configuration, builds it, installs it, stops on what a release never ships as [check](#check) does, and runs CPack with the configuration that `CMakeLists.txt` of the project describes. The packages land in `dist/` of the app folder, which its `.gitignore` keeps out of the repository, or in the folder of `-o`.

| Platform | Packages | Tools |
| --- | --- | --- |
| Windows | `<executable>-<version>-setup.exe`, an installer that installs the app for the person or, with an administrator, for every user, with a shortcut in the Start menu, an optional one on the desktop, the option to run the app at the end and an uninstaller in the settings of Windows, both signed as [signing](#signing) describes. | Inno Setup 7.1.0, which haylen.py installs into `.tools` in portable mode, without an uninstaller or entries in the registry. |
| Linux | `<executable>_<version>_<arch>.deb`, which installs into `/usr` with the dependencies that `dpkg-shlibdeps` finds in the executable and GTK 3 and the sound servers as recommendations, and `<executable>-<version>-<arch>.AppImage`, one file that runs on any distribution without installing. | The DEB generator of CPack, with `dpkg-shlibdeps` of `dpkg-dev`, and the AppImage generator of CPack, which needs CMake 4.2 or later, with appimagetool 1.9.1, the AppImage runtime 20251108 and patchelf 0.19.2, which haylen.py downloads into `.tools` and pins by their SHA-256 hashes. |

Inno Setup makes the Windows installer because it is free, maintained with regular releases for more than two decades, compiles a script from the command line, which CPack writes, and signs the installer and its uninstaller through the signing command it receives. MSI packages suit software that companies deploy to many machines, while an app for people needs a plain installer. The `.deb` reaches Debian, Ubuntu and the distributions based on them through their package manager, which resolves the libraries of the system that the app links, and the AppImage serves every other distribution with one file that needs no installation, no permission of the administrator and no runtime. The packages link the C library and the graphics libraries of the system that builds them, so they run on distributions as new as that one or newer, and a release that reaches older ones builds on the oldest distribution it supports.

#### Signing

A Windows app without a signature makes SmartScreen warn the person who installs it, so the release builds of the Windows project sign the executable, the installer and the uninstaller with `signtool` of the Windows SDK, with a SHA-256 digest and an RFC 3161 time stamp, which keeps the signature valid after the certificate expires. The certificate never belongs in the project or the repository: `signing.cmake` reads it from the environment.

| Environment variable | Value |
| --- | --- |
| `HAYLEN_WINDOWS_CERTIFICATE` | A PFX file with the code signing certificate and its private key. |
| `HAYLEN_WINDOWS_CERTIFICATE_PASSWORD` | The password of the PFX file. |
| `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` | Instead of a file, the SHA-1 thumbprint of a certificate in the certificate store of the user, such as one on a hardware token, whose private key never leaves it. |
| `HAYLEN_WINDOWS_TIMESTAMP_URL` | The time stamp server of the authority that issued the certificate. |
| `HAYLEN_WINDOWS_SIGNTOOL` | Another `signtool` than the one on `PATH` or in the newest Windows SDK. |

The project decides when CMake configures it: with `HAYLEN_WINDOWS_CERTIFICATE` or `HAYLEN_WINDOWS_CERTIFICATE_THUMBPRINT` it signs, and without either the app stays unsigned and the install of every release build says so. A workflow of GitHub Actions keeps the PFX file as Base64 in a secret and writes it to a file before the build:

```yaml
env:
  HAYLEN_WINDOWS_CERTIFICATE: ${{ runner.temp }}\certificate.pfx
  HAYLEN_WINDOWS_CERTIFICATE_PASSWORD: ${{ secrets.WINDOWS_CERTIFICATE_PWD }}
  HAYLEN_WINDOWS_TIMESTAMP_URL: ${{ secrets.WINDOWS_TIMESTAMP_URL }}
  HAYLEN_KEYS_DIR: ${{ runner.temp }}\keys
steps:
  - shell: pwsh
    run: |
      [IO.File]::WriteAllBytes($env:HAYLEN_WINDOWS_CERTIFICATE, [Convert]::FromBase64String("${{ secrets.WINDOWS_CERTIFICATE }}"))
      python haylen.py dist my-game --platform windows
```

| Secret | Value |
| --- | --- |
| `WINDOWS_CERTIFICATE` | The PFX file as Base64, which `[Convert]::ToBase64String([IO.File]::ReadAllBytes("certificate.pfx"))` prints in PowerShell. |
| `WINDOWS_CERTIFICATE_PWD` | The password of the PFX file. |
| `WINDOWS_TIMESTAMP_URL` | The time stamp server of the authority that issued the certificate. |

The [installers in CI](../ci-installers.md) guide gives the whole workflow of every platform. The content keys of the app reach the workflow through `HAYLEN_KEYS_DIR` as [the content guide](../content/content.md) describes, since every release of an app needs the keys of its earlier releases. The command `signtool verify /pa /v my-game-1.0.0-setup.exe` shows the signature of an installer.

Linux packages carry no signature of their own: the repositories that publish `.deb` packages sign their indexes, and the AppImage takes a GnuPG signature from `CPACK_APPIMAGE_SIGN` and `CPACK_APPIMAGE_SIGN_KEY` in the project.

### steam

```sh
haylen steam ~/apps/my-game --platform windows --app-id 480 --depot windows=481 --depot forest=482
```

Builds the release of an app for macOS, Windows or Linux, inspects it as `check --config Release` does, and lays it out for SteamPipe in `build/apps/<app>-<hash>/steam/`, or in the folder of `--output`. Steam installs the app with its release, so the app needs no content server: `content/<platform>/` holds the depot of the app, the installed Windows or Linux app with its release, or the macOS bundle, with every shard of the release except those of the [packs](../content/content.md#delivery) of the app, or only the shards its required content needs when `app.json` names a content server, and `content/<pack>/` holds the depot of each pack, whose shards install into `packs/` next to the executable, in `lib/<executable>/` on Linux, or next to the macOS bundle, where the runtime finds them. A player who does not own a pack, such as an expansion sold apart, never receives its depot, and its assets stay unavailable: `assets.loadAsync` of one fails with `storeAssetUnavailable`, which an app shows as content to buy. The folder `scripts/` holds `app_build.vdf` with the app ID, a description of the app and its version, and the branch of `--branch` that the build goes live on, and one `depot_build_<id>.vdf` per depot that maps its whole folder. Every depot needs its ID from Steamworks with `--depot <name>=<id>`, the platform as one name and each pack as another, and the command stops before it builds when one is missing.

Steam compares each depot with the build before it file by file, and a release keeps every unchanged shard byte for byte under the same name, so an update of a few assets uploads and downloads only the shards that changed and the two manifests. The command `steamcmd +login <account> +run_app_build <steam>/scripts/app_build.vdf +quit` uploads the build. The Steamworks API of the app itself, such as achievements and the ownership of packs, is a native library that the app calls through `haylen.native`, as the [native libraries](#native-libraries) describe.

## Engine artifacts

The command `haylen.py engine` writes this layout:

```text
build/artifacts/
  manifest.json                 Engine version and, per platform, the configuration and the source hash of the last build.
  apple/Haylen.xcframework      Static library and headers for macOS, iOS, the iOS simulator, Mac Catalyst, tvOS and the tvOS simulator.
  apple/haylen-frameworks.json  The system frameworks that the engine needs on iOS, Mac Catalyst, tvOS and macOS, which haylen.py links into Apple projects.
  apple/PrivacyInfo.xcprivacy   The APIs with required reasons that the engine calls, which haylen.py merges into the privacy manifest of Apple apps.
  apple/HaylenBridgeAsync.swift The Swift helpers of the bridge and of plugin contexts, which haylen.py writes into the haylen/ folder of Apple projects.
  android/maven/                Maven repository with dev.haylen:haylen, the Android library with the player, and dev.haylen:haylen-plugins, haylen-links and haylen-coroutines.
  android/sdk/<abi>/            The SDK of the engine for each Android ABI, which links the release libraries of apps.
  web/webgpu/                   haylen.js and haylen.wasm of the player for WebGPU, haylen-audio-worklet.js, the processor of its audio output, and haylen-content-store.js, the worker of its content store.
  web/webgl2/                   The same files for WebGL2.
  web/sdk-webgpu/, sdk-webgl2/  The SDK of the engine for each backend, which links the release runtime of each app.
  desktop/<os>-<arch>/haylen    The player of this machine.
  desktop/<os>-<arch>/haylen-content  The content tool of this machine, which builds, verifies, inspects and publishes protected releases.
  desktop/<os>-<arch>/sdk/      The SDK of the engine for this machine, which the Windows and Linux projects of apps link.
```

The intermediate build trees live in `build/engine/` for Apple, Android and the desktop, and in the regular build trees of `haylen.py build` (`build/web-<config>` and `build/web-webgl2-<config>`) for the web.

### Haylen.xcframework

The framework holds one static library per slice, `libhaylen.a`, with the engine, every dependency (Varn, Lua, Poco, OpenSSL, libuv, Box2D, ImGui, miniaudio and the rest), the runtime and the Lua player merged by `libtool`, and the public headers. Every architecture is a CMake build of `engine/` with `HAYLEN_BUILD_SDK` and `HAYLEN_BUILD_FRAMEWORK` in `build/engine/apple-<slice>-<arch>-<config>`: `haylen-install.cmake` merges the engine and the runtime as it does for the [SDK](embedding.md), and `haylen-framework.cmake` merges both with the player and writes `haylen-frameworks.json` from the lists of system frameworks in `haylen-app.cmake`, the same lists that `haylen_link_runtime_platform` links into C++ apps. The tool `lipo` joins the architectures of a slice, `xcodebuild -create-xcframework` joins the slices, and haylen.py copies `haylen-frameworks.json` and the privacy manifest of the engine next to the framework.

| Slice | Architectures | Built with |
| --- | --- | --- |
| `macos` | arm64, x86_64 | The macOS SDK. |
| `ios` | arm64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphoneos` SDK. |
| `ios-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=iOS` and the `iphonesimulator` SDK. |
| `ios-maccatalyst` | arm64, x86_64 | `engine/cmake/haylen-catalyst.toolchain.cmake`, which compiles for the `macabi` flavor of iOS against the macOS SDK and its `iOSSupport` folder, because CMake has no system name for Mac Catalyst. The engine treats the build as iOS. |
| `tvos` | arm64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvos` SDK. |
| `tvos-simulator` | arm64, x86_64 | `CMAKE_SYSTEM_NAME=tvOS` and the `appletvsimulator` SDK. |

The slices target the oldest systems the engine code allows: iOS and tvOS 16.3 and Mac Catalyst 16.4, which is macOS 13.3 on the Mac, the releases whose C++ library formats floating point numbers with `std::format`, which the engine uses for text and logs, and macOS 14.0, because `sokol_app` drives the frames of macOS apps with `-[NSView displayLinkWithTarget:selector:]`, which macOS 14.0 introduced, without checking the version of the system, so an app built for an earlier macOS stops as it opens there. Mac Catalyst draws through the UIKit path of `sokol_app`, which never calls it. The Apple template sets `IPHONEOS_DEPLOYMENT_TARGET[sdk=macosx*]` to 16.4 for Mac Catalyst, because Xcode would otherwise map iOS 16.3 to macOS 13.2. The toolchain itself reaches back to iOS and tvOS 15 and macOS 12, and a build for iOS 15 fails only on `std::format` with floating point. The engine parses floating point numbers with fast_float, because `std::from_chars` for floating point needs iOS, tvOS and macOS 26. The script `haylen.py` passes the versions as `CMAKE_OSX_DEPLOYMENT_TARGET`, to the C++ apps it builds for macOS too, the Apple template sets them in `project.yml`, and C++ apps built with `haylen_add_app` use the same ones.

On Apple platforms `sokol_app` is compiled with `SOKOL_NO_ENTRY`, so the runtime never defines `main`. The app calls `haylen_main` instead, declared in `haylen/platform/apple/HaylenMain.h`:

```objc
#import "haylen/platform/apple/HaylenMain.h"

int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
```

That `main` is also the place to register native platform bridge handlers with `[HaylenBridge registerHandler:handler:]` from `haylen/platform/apple/HaylenBridge.h`, before the runtime starts, while native plugins register theirs through their context, as the [plugin guide](../platform/plugins.md#the-apple-part) describes. Apple apps built with CMake through `haylen_add_app`, and the macOS desktop player, get the same `main` from `engine/src/platform/apple/AppleMain.cpp`.

### The Android libraries

The folder `engine/platform/android` is a Gradle project with four modules. The module `haylen` is the Android library (AAR) with the namespace `dev.haylen`. It holds `HaylenActivity`, a `GameActivity` of the AndroidX games libraries and so an `AppCompatActivity`, which draws the app in a `SurfaceView` of an ordinary view hierarchy, installs the splash screen, hides the system bars besides the status bar that the app shows, reports the safe area, the on-screen keyboard and the screen orientation, forwards low-memory warnings, takes the back button through an `OnBackPressedCallback` while the app captures it and hands its events to the plugins, `HaylenLinkActivity`, which hands the links and notifications of the app to it, `HaylenEditText`, the hidden field that edits the focused text field of the UI, `HaylenAudioFocus`, which holds the audio focus in the foreground, `HaylenNetwork`, which follows the network while the app holds the permission `ACCESS_NETWORK_STATE`, `HaylenBattery`, which follows the battery, `HaylenAccessibility`, which follows the accessibility settings, `HaylenAccessibilityTree`, which serves the elements of the UI to TalkBack and the other accessibility services, `HaylenDialogs` with the native dialogs, `HaylenBridge` for the platform bridge, the host of the Android parts of [plugins](../platform/plugins.md#the-android-part) with `HaylenPluginProvider`, which loads them when the process starts, `HaylenPlugin`, `HaylenPluginContext`, `HaylenRequirements`, `HaylenOverlay` with its `HaylenPlacement`, the screens of plugins with `HaylenScreens` and `HaylenScreen`, and the streams `HaylenVideoStream` and `HaylenAudioStream`, the splash themes and the Kotlin HTTP transport of Varn, which Gradle copies from the Varn sources. Its manifest declares only the OpenGL ES 3 requirement, so the permissions, the back policy and the components that only some apps want never reach an app through it: the modules `haylen-plugins` and `haylen-links` declare `HaylenPluginProvider` and the exported `HaylenLinkActivity` in manifests of their own, which reach the apps whose plugins depend on them, and `haylen-coroutines` holds `HaylenCoroutines` and `registerSuspend` with `kotlinx-coroutines-android`, as the [plugin guide](../platform/plugins.md#engine-libraries) describes. It packages the player, which the engine CMake project builds with `HAYLEN_BUILD_PLAYER` as the shared library `libhaylen.so`, for arm64-v8a, armeabi-v7a (the 32-bit Android TV devices still in use) and x86_64 (emulators), from Android 8.1 (API 27) on. The NDK r30 links the arm64-v8a and x86_64 libraries with 16 KB page alignment, which Android 15 devices with 16 KB pages need, and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` makes it explicit. 32-bit ARM has no 16 KB pages and keeps 4 KB alignment.

The command `haylen.py engine --platform android` builds the player and the SDK of the engine of each ABI in `build/engine/android-<abi>-<config>`, one ABI after the other with the jobs of `--jobs`, so the compilers stay within them and the configures never write into the shared CPM sources at the same time, gathers the libraries in `build/engine/android-<config>/jniLibs` and installs the SDK of each ABI into `build/artifacts/android/sdk/<abi>`, which links the release libraries of apps. It passes them to Gradle with the folder where CPM placed Varn, then runs the `publishReleasePublicationToArtifactsRepository` task of every module, which publishes `dev.haylen:haylen`, `dev.haylen:haylen-plugins`, `dev.haylen:haylen-links` and `dev.haylen:haylen-coroutines` at the engine version with their POMs to `build/artifacts/android/maven`. The POM of `haylen` lists the dependencies of the library, which Gradle brings into every app: the Kotlin standard library, `androidx.games:games-activity` 4.4.2, which the Gradle module metadata pins strictly because the player links the native side of the same version, `androidx.appcompat:appcompat`, `androidx.activity:activity` and `androidx.core:core`, which `HaylenActivity` and its plugins build on, `androidx.core:core-splashscreen` and `androidx.window:window-java`, the Jetpack WindowManager that reports the fold of foldable devices. The POMs of the other libraries list `haylen`, and the one of `haylen-coroutines` also `kotlinx-coroutines-android`, which apps and plugins with suspending handlers get as an API.

### Web and desktop

The web artifacts are the `haylen` player built for WebGPU and for WebGL2, each next to `haylen-audio-worklet.js`, the AudioWorklet processor that the runtime loads from the folder of its script, and `haylen-content-store.js`, the worker of its content store, and the SDK of the engine for each backend in `web/sdk-webgpu/` and `web/sdk-webgl2/`, which link the runtime of the release of each app. The player embeds no app: in development the page hands it the package at runtime, so the same `haylen.wasm` runs every app. The desktop artifacts are the `haylen` player of this machine, which runs any package that its command line names, the content tool `haylen-content`, and the SDK of the engine for this machine in `sdk/`, which the Windows and Linux projects of apps link in their Debug and Release configurations. One build tree of the engine, `build/engine/desktop-<config>/`, builds all three.

## Templates

```text
templates/
  common/             What every kind of app shares: app.json, the logo, the .gitignore, .editorconfig and .clang-format of an app, and the platform projects that both kinds build.
    platform/
      android/        Gradle project that depends on the haylen library, with the Kotlin handler of the bridge example.
      web/app.js      Page code of the app, with the JavaScript handler of the bridge example.
  lua/                The Lua app: its starter code and the platform projects that only Lua apps build.
    source/main.lua   The starter app, which calls the bridge example.
    platform/
      apple/          XcodeGen project with targets for iOS and iPadOS with Mac Catalyst, tvOS and macOS, with the Swift handler of the bridge example.
      web/            Loading page with the logo, the progress bar, the backend choice and the error screen.
      windows/        CMake project of the Windows executable with its icon, version, manifest, signing and Inno Setup installer.
      linux/          CMake project of the Linux executable with its desktop entry, icons, AppStream metainfo, `.deb` and AppImage.
  cpp/                The C++ app, a CMake project that compiles the engine.
    CMakeLists.txt    Adds the engine and builds the app with haylen_add_app.
    src/App.cpp       The starter app, which calls the bridge example and answers it on Windows and Linux.
    src/AppBridge.mm  The Objective-C handler of the bridge example on Apple platforms.
  plugin/             The starter plugin of haylen.py plugin new, with the .gitignore, .editorconfig and .clang-format of a plugin repository.
```

The templates of apps are layers. The folder `common` holds what every kind of app shares, and `lua` and `cpp` what only their kind has. An app or a platform project takes the files of `common` first and then those of its kind, and a file of the kind replaces the common file of the same path, while a common file that the kind lacks stays, so no file repeats between the layers. The command `new` copies both layers whole, and `platform add`, `platform diff` and the copies that haylen.py keeps for apps without a project of a platform take `platform/<template>/` of both layers. The platform templates of a kind are the folders under `platform/` of its layers.

The tool `haylen.py` finds the platform templates by folder. A new platform is a folder under `platform/` of a layer and a run target in `haylen.py`, the `RUN_TARGETS` table that names, for each `--platform`, the template of its project, the engine artifacts it needs and the functions that prepare, build and launch it and check what the built app lacks. Apps mirror the same shape with their `platform/<platform>/` folders.

### common

The file `app.json` and `content/logo.png`, the Haylen symbol on a transparent background, which is also the splash logo of the app, are the package that both kinds share, and the Android project and the page code of the web are the platform files that both kinds build the same way.

Next to them are the files that the repository of an app keeps at its root. The file `.gitignore` keeps out what an app never commits: the build outputs in `build/` and `dist/`, the CPM cache in `.cache/`, the folder `haylen/` of every platform project, the files of Finder and of the editors, the local settings and caches of Gradle, Kotlin and the Android build of C++ code, and the state of Xcode for each person. The file `.editorconfig` sets the style of text files, and `.clang-format` holds the C, C++ and Objective-C style of the engine for the code of the app, of its platform projects and of its plugins. None of them is part of the package.

### lua

The file `source/main.lua` is a scene with the logo, the name of the app, a line with the engine version and platform, a button, a label that counts the presses and a line with the answer of the [bridge example](#the-bridge-example). The button starts with the focus, so gamepads and TV remotes can press it. It runs on every platform.

### cpp

The folder of a C++ app is its CMake project and its package at once, since `haylen_add_app` deploys only `app.json`, `source/`, `content/` and the manifests and Lua of the plugins that `app.json` lists, whose native parts `run-cpp` brings as the [plugin guide](../platform/plugins.md#c-apps) describes. Its `CMakeLists.txt` adds the engine folder that `HAYLEN_ENGINE_SOURCE` names, which `run-cpp` passes, or an installed SDK through `find_package` when the variable is empty, as the [embedding guide](embedding.md) describes, and builds the target `main` with the `CPP` option from `src/App.cpp`, with `src/AppBridge.mm` on Apple platforms and with `platform/web/app.js` linked into the runtime script with `--pre-js` on the web. The target is not named `app`, because iOS and tvOS bundles keep the package in a folder of that name next to the executable. Its scene draws the logo, the name of the app and the answer of the [bridge example](#the-bridge-example) with the renderer of the engine, and the app runs no Lua. Android builds through `platform/android` of the common layer, as [run-cpp](#run-cpp) describes, and `haylen_add_app` makes the apps of the other platforms itself. The commands `run`, `prepare`, `xcodegen`, `dist` and `steam`, which build the platform projects of Lua apps, stop for the folder of a C++ app and name `run-cpp` instead.

### The bridge example

Both templates ship a working call of the [platform bridge](../platform/platform-bridge.md). The app calls the method `app.greet` with its kind, `Lua` or `C++`, as the name, the native code of the platform answers with a greeting that names its language, and it sends the event `app.greeted` with the count of greetings so far, which the app shows next to the answer.

| Platform | Lua app | C++ app |
| --- | --- | --- |
| macOS, iOS, iPadOS, Mac Catalyst, tvOS | Swift, in `platform/apple/source/AppBridge.swift`, which the function `main` of `main.m` registers. | Objective-C, in `src/AppBridge.mm`, which registers in `+load`. |
| Android | Kotlin, in `onCreate` of `App` in `platform/android/app/src/main/kotlin/dev/haylen/app/App.kt`. | The same Kotlin. |
| Web | JavaScript, in `Module.preRun` of `platform/web/app.js`. | The same JavaScript, linked with `--pre-js`. |
| Windows, Linux | No native handler, so the call fails with the code `noHandler` and the app shows its message. | C++, in `registerDesktopHandlers` of `src/App.cpp`, whose engine handler answers there. |

The Lua side is `Home:greet` of `source/main.lua`, which listens with `platform.on` and calls with `platform.call` inside `scene.spawn`, both owned by the scene. The C++ side is `HomeScene::enter` of `src/App.cpp`, which listens with `Bridge::on` and calls with `Bridge::call`. The desktop player of `haylen run` holds no native code of the app, so there the call fails with `noHandler` too. To replace the example with real code, rename the method and the event in the app and in each handler and change what they do, keep the files where they are, since the projects already build and register them, and add more handlers next to them with the APIs that the [platform bridge guide](../platform/platform-bridge.md) describes for each language. An app that needs no native code deletes the handlers and the call.

### apple

```text
templates/lua/platform/apple/
  project.yml                     XcodeGen spec, which includes haylen/project.yml and gives each target its template from it.
  App.xcodeproj                   The project generated from project.yml with the haylen/ folder of an app without plugins or native libraries, always committed next to it.
  App.xcconfig                    Includes haylen/Haylen.xcconfig, and the settings after the include win.
  source/main.m                   Calls haylen_main, after the native bridge handlers of the app register.
  source/AppBridge.swift          The Swift handler of the bridge example.
  source/HaylenBridging.h         Makes HaylenBridge.h, HaylenPlugin.h and HaylenNotificationPlugin.h visible to the Swift files of the app and its plugins.
  ios/                            Info.plist, LaunchScreen.storyboard and Assets.xcassets with the app icon.
  tvos/                           Info.plist, LaunchScreen.storyboard and Assets.xcassets with the layered app icons and the top shelf images.
  macos/                          Info.plist and Assets.xcassets with the app icon.
```

The targets are `iOS` (iPhone and iPad, with `SUPPORTS_MACCATALYST` for the Mac Catalyst destination), `tvOS` and `macOS`, each with a shared scheme of the same name and the target template of its platform, `HaylenIOS`, `HaylenTVOS` or `HaylenMacOS`, which brings the engine, the package, the splash assets, the completed `Info.plist`, entitlements and privacy manifest, the native libraries and the native parts of the plugins, as [the Apple project](#the-apple-project) describes. What stays in the template belongs to the developer: the deployment targets, C++20 and the Swift bridging header, the device families, Mac Catalyst and its deployment target of 16.4, the hardened runtime of macOS, the ad hoc signing of builds for this Mac and the app icons. The folder `source/` is one that Xcode keeps in sync, so every Objective-C, C++ and Swift file in it builds into every target, and the module of the targets is `HaylenApp`, so Objective-C++ reaches Swift classes through `HaylenApp-Swift.h`. The targets read their product name and bundle identifier from `HAYLEN_PRODUCT_NAME` and `HAYLEN_BUNDLE_IDENTIFIER` and their version from `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION`, which `haylen/Haylen.xcconfig` sets from `app.json`. Builds for this Mac, native or Mac Catalyst, sign with the ad hoc identity, while device builds use the team of `HAYLEN_APPLE_TEAM`.

The `Info.plist` of each platform is complete and holds `$(HAYLEN_DISPLAY_NAME)`, `$(MARKETING_VERSION)` and the other build settings for the values of `app.json`. Its other keys are visible defaults that the developer changes or deletes: the scene manifest, which declares several scenes on iOS, so screens of plugins open windows of their own on Mac Catalyst and iPad while the app still draws in one window, and one scene on tvOS, `ITSAppUsesNonExemptEncryption`, the statement on export compliance that App Store Connect asks about, `GCSupportsControllerUserInteraction`, the hidden status bar of iOS and the principal class and minimum system of macOS. The orientations of iOS come from `app.json`, so the template leaves them out.

The asset catalogs carry the Haylen icons, the gradient symbol on the navy `#07112F` of the brand. The iOS icon is an opaque full square that the system masks. The macOS icons put the symbol on a navy rounded plate with a soft drop shadow, on the grid of macOS icons, with every size drawn on its own. The layered tvOS icons have an opaque navy back layer and the symbol on a transparent front layer, and the top shelf images show the horizontal logo with its white wordmark on navy.

### android

```text
templates/common/platform/android/
  settings.gradle.kts         Reads haylen/haylen.properties: the engine repository, the plugin modules in haylen/plugins, the engine version of the modules and their build folder.
  build.gradle.kts            Android Gradle Plugin 9.4.1 for apps and libraries, Kotlin 2.4.21 for the Kotlin sources, and the Gradle plugins of the plugins on the build classpath.
  gradle.properties           Gradle settings.
  gradle/wrapper/             Gradle 9.8.1 for Android Studio.
  app/build.gradle.kts        The app module, which depends on dev.haylen:haylen, which brings GameActivity, AppCompat, the activity library and core, and on the plugin modules, applies the Gradle plugins of the plugins and has no C++.
  app/src/main/AndroidManifest.xml
  app/src/main/kotlin/dev/haylen/app/App.kt  The application class, where the native bridge handlers of the app register before the first activity starts, with the handler of the bridge example.
  app/src/main/res/           Adaptive launcher icon with its monochrome layer and Android TV banner.
```

The app module reads the application id, version name and code, label, screen orientation, status bar, native library and manifest placeholders from `haylen/haylen.properties` and adds `haylen/assets`, `haylen/res` and `haylen/jniLibs` to its source folders, as [the Android project](#the-android-project) describes. The version code comes from the version, with 1.2.3 becoming 1002003. The SDK levels and R8 are choices of the developer, the signing reads the keys of `keystore/` as [android-key](#android-key) describes, and the `minSdk` of 27 is what the engine library needs. The manifest declares the permissions `INTERNET`, `ACCESS_NETWORK_STATE` and `VIBRATE` as visible defaults, which network access, the network events and `system.vibrate` need and which an app deletes when it does not use them, since the engine library declares no permission: a feature whose permission is missing logs once what is missing and does nothing or fails, as the [plugin guide](../platform/plugins.md#android-requirements) describes. The `<application>` sets `android:enableOnBackInvokedCallback`, which lets the back callback of `HaylenActivity` play the predictive back animation of the system. The manifest declares `HaylenActivity` single top, so the launcher icon brings back every screen that shows over the app, such as a purchase, lists every change of the configuration in `android:configChanges`, so a new language, font size or display keeps the app running instead of starting it again, with the `android.app.lib_name` meta-data set to the native library, which GameActivity loads, `haylen` for Lua apps, the Lua player of the haylen library, and the library of the app for C++ apps, whose APK leaves the Lua player out, the `dev.haylen.statusBar` meta-data, which launches the activity with the status bar of `app.json` until the engine applies it, and without which the status bar starts hidden, the `LAUNCHER` and `LEANBACK_LAUNCHER` categories, the TV banner, and a touchscreen, the screen orientations, Android TV and a gamepad as optional features, so the same APK serves phones, tablets and Android TV. The links and notifications of the app go to `HaylenLinkActivity`, which the `haylen-links` library of the plugins that receive them declares with their intent filters, so an app without such plugins has no exported component besides its launcher activity, and `haylen.py run` starts the app with the launcher intent, whose task the launcher icon then brings back as it is.

The resources are vector drawables of the Haylen brand. The adaptive launcher icon has the gradient symbol inside the safe zone of its foreground, over `ic_launcher_background`, the navy `#07112F` of the brand, and a monochrome layer with the silhouette of the symbol, which launchers tint for themed icons. The Android TV banner shows the horizontal logo with its white wordmark on the same navy.

### web

```text
templates/lua/platform/web/
  index.html        The canvas and the splash with the logo, the progress bar and the status line.
  loader.css        Styles of the page and the splash.
  loader.js         Checks the browser, picks the backend, downloads with progress, loads the plugins and starts the runtime.
templates/common/platform/web/
  app.js            Page code of the app, where its bridge handlers register, with the handler of the bridge example.
```

### windows

```text
templates/lua/platform/windows/
  CMakeLists.txt          Includes haylen/haylen.cmake, builds the executable in the Windows subsystem, installs it with the package and the native libraries, signs release builds and describes the installer.
  signing.cmake           Signs a file with signtool and a time stamp, with the certificate that the environment names.
  resources/app.rc        The icon, the version resource and the application manifest of the executable.
  resources/app.manifest  Windows 10 and 11, the Common Controls of version 6, UTF-8 as the code page, per-monitor DPI awareness and no elevation.
  resources/app.ico       The Haylen icon in the sizes Windows shows, from 16 to 256 pixels.
```

`CMakeLists.txt` adds the target `app`, which `haylen_app(app)` of `haylen/haylen.cmake` makes the executable of the app, names it after `HAYLEN_APP_EXECUTABLE` and builds in the Windows subsystem, without a console. The resource script `resources/app.rc` reads the values of `app.json` from `haylen/HaylenApp.h` for the version resource, which the properties of the file and the Task Manager show: `CompanyName` from `company`, `FileDescription` and `ProductName` from `name`, `FileVersion` and `ProductVersion` from `version`, `LegalCopyright` from `copyright`, `OriginalFilename` from the executable and `Comments` from `description`. It embeds the application manifest itself, so the linkers of MSVC and clang-cl take `/MANIFEST:NO` and add none of their own. The engine needs the Common Controls of version 6, where the task dialogs of [haylen.dialogs](../lua-api/dialogs.md) live, and UTF-8 as the code page of the process, since it keeps every text and path in UTF-8, while the per-monitor DPI awareness matches the one that the D3D11 runtime asks for. The window of the app and its taskbar button take the first icon of the executable, the one that Explorer shows. The icon is the gradient symbol on a navy plate with rounded corners, every size drawn from the full-size art.

The project installs the executable with the package in `app/` and the native libraries next to it, signs the executable of a release build when the environment names a certificate, and describes the installer that [dist](#dist) makes with Inno Setup.

### linux

```text
templates/lua/platform/linux/
  CMakeLists.txt              Includes haylen/haylen.cmake, builds the executable, installs it with the desktop entry, the AppStream metainfo and the icons, and describes the .deb and the AppImage.
  resources/app.desktop       The keys of the desktop entry that belong to the developer, such as its categories.
  resources/app.metainfo.xml  The elements of the AppStream metainfo that belong to the developer, such as the licenses and the content rating.
  resources/icons/            The Haylen icon in the hicolor sizes from 16 to 512 pixels, and as SVG for every other size.
```

`CMakeLists.txt` adds the target `app`, which `haylen_app(app)` makes the executable of the app, with the C++ library of the compiler linked in statically, so the app runs on distributions older than the one that built it, as long as their C library is as new. It installs the executable into `lib/<executable>/` with the package in `app/` and the native libraries in `lib/`, which the `RUNPATH` `$ORIGIN:$ORIGIN/lib` of the executable names, and a link `bin/<executable>` to it, so desktops start the app by name. The executable cannot share the name of those folders, so `haylen.py` stops a Linux build of an app whose folder gives the executable `app` or `lib` and asks for another folder name. The desktop entry goes into `share/applications/`, the metainfo into `share/metainfo/` and the icons into `share/icons/hicolor/`, all named after the identifier of `app.json`, which is also the class of the window of the app, so desktops show the name and the icon of the entry for the window.

The tool `haylen.py` completes `resources/app.desktop` and `resources/app.metainfo.xml` into `haylen/`, the way it completes the `Info.plist` of Apple projects: what the developer gives wins, with a warning when it differs from `app.json`, and the values of `app.json` fill what is missing.

| File | Values of `app.json` |
| --- | --- |
| `app.desktop` | `Name` from `name`, `Comment` from `description`, `Exec` from the executable, and `Icon` and `StartupWMClass` from `identifier`. |
| `app.metainfo.xml` | `id` from `identifier`, `name`, `summary` and the first paragraph of `description` from `description`, `developer` with the name of `company` and the identifier without its last part as its id, the `launchable` desktop entry, and the `homepage` url from `website`. |

The metainfo of the template declares the `CC0-1.0` license for the metadata itself, a proprietary license for the app and an empty content rating, which the developer changes, and the desktop entry puts the app among games. The command `appstreamcli validate` checks the metainfo, which needs a `website` in `app.json` to pass, and `desktop-file-validate` checks the desktop entry. A metainfo that lists the releases of the app keeps them in `<releases>`, each with its version and date.

## Platform projects

The project of a platform belongs to the developer. It is `platform/<template>/` of the app when that folder exists, and haylen.py builds it where it is. An app without that folder uses a copy of the layers of the template for the kind of the app, which haylen.py keeps in the build folder of the app, `build/apps/<app>-<hash>/<template>/`, named after the app folder and the start of the SHA-256 hash of its absolute path, so apps in folders with the same name never share projects, and makes again whenever the template changes. A file that the developer deletes stays deleted, and a newer template reaches a project only when the developer adopts it, which [`platform diff`](#platform) helps with.

The tool `haylen.py` writes one folder inside a project, `haylen/`, which the `.gitignore` of the app ignores, and never edits `project.yml`, `project.pbxproj`, the Gradle scripts, the manifests, the `Info.plist` files or the entitlements of the developer. The files of the project reach what it writes through a few visible lines, which the developer may delete to take over. Build products go to the build folder of the app, outside the project: Xcode builds into `xcode/`, Gradle into `gradle/`, with its project cache in `gradle-cache/`, and CMake into `cmake/<platform>-<config>/`.

The command `prepare` writes the folder, and `run` prepares the project, generates `App.xcodeproj` again when it may, builds, [checks](#check) the built app and launches it. Before the first step, `run` checks the plugins of the app and their values for the platform, and stops with every problem it finds.

### The Apple project

The file `project.yml` includes `haylen/project.yml`, and every target names its template in `templates`. XcodeGen appends the lists of a template to the lists of the target, and a setting of the target wins over the setting of the template, so the developer adds frameworks, sources and settings to a target, or points `INFOPLIST_FILE` at a file of their own and leaves the completed `Info.plist` behind. `App.xcconfig` starts with `#include "haylen/Haylen.xcconfig"`, and its settings after the include win, such as a build number from continuous integration in `CURRENT_PROJECT_VERSION`. The sources of each target leave its `Info.plist` out of the resources with `excludes` and list it with `buildPhase: none`, since the target builds the completed copy.

| File in `haylen/` | Contents |
| --- | --- |
| `project.yml` | The target templates `HaylenIOS`, `HaylenTVOS` and `HaylenMacOS`, and the Swift packages of the plugins. Each template links `Haylen.xcframework` and the system frameworks of the engine, from the list that the artifacts publish, with IOKit for Mac Catalyst alone, leaving out a framework that a target of the template links itself, since XcodeGen refuses a dependency listed twice. It copies the package and the privacy manifest, and on iOS and tvOS the splash assets, sets `INFOPLIST_FILE` to the completed `Info.plist` and `CODE_SIGN_ENTITLEMENTS` to the completed entitlements when there are any, and adds the native parts of the plugins of its platforms, as the [plugin guide](../platform/plugins.md#apple-platforms) describes. The phase `Embed native libraries`, which copies the libraries of the file list `haylen/native/<platform>-<sdk>.xcfilelist` into the `Frameworks` folder of the bundle and signs them with the identity of the app, joins the templates of the platforms that the app has native libraries for, and scripts run without the script sandbox in a template with any build phase, which would need every file of a bundle and the temporary files of `codesign` listed one by one. |
| `Haylen.xcconfig` | `HAYLEN_PRODUCT_NAME`, `HAYLEN_DISPLAY_NAME`, `HAYLEN_BUNDLE_IDENTIFIER`, `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` from `app.json`, and `OTHER_LDFLAGS` with `HAYLEN_NATIVE_LDFLAGS`, which links static native libraries. |
| `ios/Info.plist`, `tvos/Info.plist`, `macos/Info.plist` | The `Info.plist` of the developer, completed. |
| `ios/App.entitlements`, `ios/Catalyst.entitlements`, `tvos/App.entitlements`, `macos/App.entitlements` | The entitlements of the developer at the same path, completed with the `entitlements` of the plugins of iOS, Mac Catalyst, tvOS and macOS, written when either has any. |
| `PrivacyInfo.xcprivacy` | The [privacy manifest](#privacy-manifest) of the app. |
| `Splash.xcassets` | The `splash-logo` image and the `splash-background` color of the [launch screens](#splash-screens). |
| `app/` | The package, which the bundle carries as `Resources/app`, or in the Release configuration the protected release of the app. |
| `HaylenBootstrap.cpp` | The bootstrap of the app, which every target template compiles. In the Release configuration the content tool writes it from the key folder of the app, with its sealed content keys, and in the Debug configuration it holds only a check that stops a release build of the development package with the way to prepare the release. |
| `HaylenBridgeAsync.swift` | The [Swift helpers](../platform/plugins.md#swift-helpers) of the bridge and of plugin contexts, which every target template compiles, from the Apple artifacts. |
| `native/`, `HaylenNativeSymbols.mm` | The [native libraries](#native-libraries) of the run platform with the file lists of the embed phases, and the table of the symbols of static libraries. |
| `plugins/<id>/` | The Apple sources and resources of the plugins. |
| `Haylen.xcframework` | A link to the framework of the artifacts. |
| `state.json` | The hashes of the last generation of `App.xcodeproj`, and for a copy of the template, the hash of the template. |

The completed `Info.plist` and entitlements follow one rule: the value of the developer wins, and the keys of `app.json` and of the plugins only fill what is missing. Objects merge key by key and arrays gain the items they lack, so two ad plugins share `SKAdNetworkItems`, and a value of the developer that differs from the one of a plugin stays, with a warning, such as a usage description in the words of the company. Two plugins, or a plugin and `app.json`, that give one key different values stop the build with both named. The file `app.json` fills the orientations of iPhone and iPad, the status bar that the app launches with, `UIStatusBarHidden` and `UIStatusBarStyle` from `window.statusBar`, and, for an app whose `window.showInTaskbar` is `false`, `LSUIElement` on macOS, so macOS never shows its Dock icon, not even while it starts. No key that haylen.py adds is a decision of the company, such as the statement on export compliance, which stays a default of the template that the developer keeps or deletes.

### Generating App.xcodeproj

The developer changes the project in `project.yml` and generates `App.xcodeproj` again with `haylen xcodegen <app>`, which prepares `haylen/` and runs the XcodeGen that haylen.py pins. The command `run` generates it on its own when `project.yml` includes `haylen/project.yml` and the inputs changed since the last generation, which are `project.yml`, `haylen/project.yml`, the XcodeGen version and the files of the plugin sources, such as after the app gained a plugin, as long as `project.pbxproj` still has the hash of the last generation or is an untouched copy of the template in a project without a record. The file `haylen/state.json` keeps both hashes. A project whose inputs did not change builds as it is, edits made in Xcode included. When the inputs changed and `project.pbxproj` differs from the last generation, or haylen.py never generated it, such as in a fresh clone of the repository of an app, haylen.py generates the project once more in a scratch folder that links the entries of the project: when the result is the same file, haylen.py records it and goes on, and otherwise it stops without touching the project and says what to do:

```text
Error: The project platform/apple/App.xcodeproj changed since its last generation from "project.yml", or was never generated from it, so it stays as it is. Move the changes made in Xcode into "project.yml" and run "haylen xcodegen .", which generates the project again.
```

A `project.yml` without the include is never generated by `run`, and [check](#check) reports what its targets lack.

### The Android project

The Gradle scripts of the developer read `haylen/haylen.properties`:

| Key | Value |
| --- | --- |
| `repository`, `engineVersion` | The local Maven repository of the artifacts and the engine version. `settings.gradle.kts` gives every module the version as the extra property `haylenEngineVersion`, which plugin modules depend on the engine libraries with. |
| `name`, `identifier`, `versionName`, `versionCode`, `orientation`, `statusBar` | The identity, version, orientation and status bar of `app.json`. |
| `library` | The native library that `HaylenActivity` loads: `haylen`, the Lua player, for the debug builds of Lua apps, `haylen_app` for their release builds, or the library of a C++ app. The template stops a release build of a project whose library is `haylen`, since that project holds the development package. |
| `plugins`, `gradlePlugins`, `placeholder.<name>` | The plugin modules as `id=folder` entries, their Gradle plugins as `id=version` entries and their manifest placeholders, as the [plugin guide](../platform/plugins.md#android) describes. |
| `buildDirectory` | The build folder of the app, where `settings.gradle.kts` sends the build outputs of every module. |

Next to it, `assets/app/` holds the package with `haylen-package-index.json`, the list of its files, because Android cannot list asset folders recursively, or in the Release configuration the flat folder of the protected release, `assets/haylen-plugins.json` the plugins with an Android part in [load order](../platform/plugins.md#load-order) with the values of their parameters, `res/` the splash resources, `jniLibs/<abi>/` the native libraries, and `plugins/<id>/` the library modules of the plugins. The tool `haylen.py` writes every value as ASCII with `\uXXXX` escapes, so any text survives. It builds with Gradle, whose outputs and project cache go to the build folder of the app, and installs the newest APK of the configuration, whatever product flavors the project defines.

### The web site

The files of `platform/web/` of the app, or of the layers of the template, go as they are into the site in the build folder of the app, `build/apps/<app>-<hash>/web/`, and haylen.py writes next to them `config.json`, the package, `app.zip` in development or the protected release in `app/` with `--config Release`, the splash logo, the web modules of the plugins in `plugins/` and the WebGPU and WebGL2 runtimes. Nothing is written into `platform/web/`, and a file that the developer deletes stays out of the site.

### Windows and Linux

`CMakeLists.txt` of `platform/windows` or `platform/linux` includes `haylen/haylen.cmake` before its targets and gives its executable target `haylen_app(<target>)`.

| File in `haylen/` | Contents |
| --- | --- |
| `haylen.cmake` | The values of `app.json` as `HAYLEN_APP_NAME`, `HAYLEN_APP_IDENTIFIER`, `HAYLEN_APP_VERSION`, `HAYLEN_APP_DESCRIPTION`, `HAYLEN_APP_COMPANY`, `HAYLEN_APP_COPYRIGHT`, `HAYLEN_APP_WEBSITE` and `HAYLEN_APP_EXECUTABLE`, the folders `HAYLEN_PACKAGE_DIR` and `HAYLEN_NATIVE_DIR`, on Linux `HAYLEN_DESKTOP_ENTRY` and `HAYLEN_METAINFO`, the SDK of the desktop artifacts through `find_package(haylen)`, and the function `haylen_app(<target>)`. The function gives the target the Lua player of the engine, the bootstrap and `haylen::runtime` with hidden symbols, the C runtime of the SDK in every configuration on Windows, and on Linux the `RUNPATH` `$ORIGIN:$ORIGIN/lib` with its symbols stripped in Release. It also links the package and the native libraries next to the executable of the build tree, so the build tree runs from an IDE as the installed app does. |
| `app/` | The package, or in the Release configuration the protected release of the app. |
| `HaylenBootstrap.cpp` | The bootstrap of the app, which `haylen_app` compiles. In the Release configuration the content tool writes it from the key folder of the app, and in the Debug configuration it holds only a check that stops a build where `NDEBUG` is defined, since the project then holds the development package. |
| `native/` | The [native libraries](#native-libraries) of the app for this machine. |
| `HaylenApp.h` | On Windows, the values of `app.json` as strings of the resource script and the version as four numbers, which `resources/app.rc` includes. |
| `app.desktop`, `app.metainfo.xml` | On Linux, the desktop entry and the AppStream metainfo of the project completed with the values of `app.json`, as [linux](#linux) describes. |

The executable is named after the app folder in lowercase dash-case, such as `my-game` for `My Game`, which every package format accepts. The command `run` configures the project with the configuration of `--config` in `cmake/<platform>-<config>/` of the build folder of the app, builds it and installs it again into `installed/<platform>/` of the build folder, which is the app as it ships:

```text
build/apps/my-game-1a2b3c4d/installed/windows/
  my-game.exe             The executable of the app, with its icon, version resource and manifest.
  app/                    The package, or the manifests and shards of the protected release.
  my_glue.dll             The native libraries of the app.

build/apps/my-game-1a2b3c4d/installed/linux/
  bin/my-game             A link to the executable.
  lib/my-game/my-game     The executable of the app.
  lib/my-game/app/        The package, or the manifests and shards of the protected release.
  lib/my-game/lib/        The native libraries of the app.
  share/applications/     The desktop entry, named after the identifier.
  share/metainfo/         The AppStream metainfo.
  share/icons/hicolor/    The icons.
```

The project also builds without haylen.py once `prepare` wrote its folder `haylen/`, from an IDE or from a terminal:

```sh
haylen prepare ~/apps/my-game --platform linux --config Release
cmake -S ~/apps/my-game/platform/linux -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
cmake --install build/linux --prefix dist/linux
```

## Release builds

The Release configuration of `run` and `prepare` builds what an app ships: its package only as the protected release that the content tool builds into `build/apps/<app>-<hash>/release/<profile>/`, as the [content guide](../content/content.md) describes, with the Lua modules as bytecode, and a binary that compiles in the bootstrap of the app, `HaylenBootstrap.cpp`, which the content tool writes from the key folder of the app and which seals its content keys, so no key sits in a file of the app. A release reuses the shards of the release before it and the build cache of the app, so a rebuild after a small change writes only the shards of what changed. The Debug configuration ships the package as it is, for development.

```sh
haylen run ~/apps/my-game --platform linux --config Release
haylen prepare ~/apps/my-game --platform windows --config Release
haylen check ~/apps/my-game --platform linux --config Release
```

| Platform | Release build |
| --- | --- |
| Windows, Linux | Writes the release of the `windows` or `linux` profile into `haylen/app/` of the project and the bootstrap into `haylen/HaylenBootstrap.cpp`, which `haylen_app` compiles into the executable that links the SDK of the desktop artifacts, with hidden symbols, stripped on Linux and without a PDB on Windows. The project installs the release in `app/` next to the executable. |
| Android | Writes the release of the `android` profile into `haylen/assets/app/` of the Android project, a flat folder that Android lists without an index and whose shards the template keeps uncompressed in the APK, and links the library of the app, `libhaylen_app.so`, for every ABI from the SDK of the Android artifacts with the bootstrap, which exports only the entry points of Java and GameActivity and ships stripped, while its symbols stay in `libhaylen_app.so.debug` of `build/apps/<app>-<hash>/release-library/build-<abi>/` for crash reports. `library` of `haylen/haylen.properties` names it, so the activity loads it and the Lua player of the `haylen` library stays out of the APK. The release build signs with the upload key of the app, which `android-key` creates. |
| Web | Places the whole release of the `web` profile in `app/` of the site, where the shards of the content stay and stream by range, and links the runtime of the app for WebGPU and for WebGL2 with Emscripten from the web SDK of the artifacts with the bootstrap, in `build/apps/<app>-<hash>/release-web/`. The site holds no `app.zip`, and `config.json` names the manifests and the shards of the app domain, which the page downloads before the app starts. The symbol map of each runtime, `haylen.js.symbols`, stays in its build folder for crash reports and never reaches the site. |
| macOS, Mac Catalyst, iOS, tvOS | Writes the release of the `apple` profile into `haylen/app/` of the Apple project, which the bundle carries as `Resources/app` and on macOS as `Contents/Resources/app`, and the bootstrap into `haylen/HaylenBootstrap.cpp`, which the targets compile into the app that links `Haylen.xcframework`. Every target of the project shares the release. Xcode keeps the debug information of a release build in a `.dSYM` next to the app, and an archive strips the app. |

After a release build, `run` inspects the built app and stops before it launches when the app holds what a release never may, and `check --config Release` reports the same as missing requirements, so continuous integration fails on it: a `.lua` file, the `haylen-package-index.json` of a development package, a file of the package as it is under any name, other files in the release folder than its manifests and shards, symbol files such as `.pdb`, `.dSYM` or `.debug`, and any content key or the signing key of the app as bytes, as hexadecimal text in either case or as Base64 text. The splash logo is the one file of the package that an app shows outside its release, since the system shows it before the engine starts.

Stores limit the size of an app, so a release build warns when its release is larger than the store of its platform accepts inside an app: the App Store accepts iOS and tvOS apps of at most 4 GB uncompressed, and Google Play a base module of at most 200 MB to download, which the compressed shards of a release fill almost byte for byte. Those limits belong to the stores and never to the format, which serves releases of any size.

### check

The command `check` compares the last build of an app for a platform with what the engine and every plugin of the platform need, and prints each missing requirement with who needs it, why, and the snippet with the file to put it in. The command `run` runs the same check after each build and prints what it finds as warnings, since the features that need a missing requirement log once and answer `unsupported` instead of stopping the app, as the [plugin guide](../platform/plugins.md#requirements) describes. The command fails when anything is missing, so continuous integration can run it.

| Platform | What it reads | What it checks |
| --- | --- | --- |
| Apple | The `Info.plist` of the bundle, the frameworks that `otool -L` lists for the executable, the entitlements that `codesign` reports, the privacy manifest of the bundle and the classes of the executable, and with `--config Release` every file of the bundle. | The system frameworks of the engine and the scene manifest on iOS and tvOS, and for every plugin its class, its `frameworks`, its `infoPlist` keys, its `entitlements`, its `privacy` declarations and its resources. A value only has to exist, so a usage description may have other words, while items of arrays and booleans must match. With `--config Release`, what a release never ships, as [release builds](#release-builds) describe. |
| Android | The merged manifest of the APK through `aapt2 dump xmltree`, and its native libraries, and with `--config Release` every entry of the APK. | `HaylenActivity`, the provider that loads plugins in an app with plugins, the native libraries of the app, and for every plugin the permissions, components and meta-data of the manifest of its module, and in a project of the developer the `files` it places there. A permission that the manifest of the app removes with `tools:node="remove"` is named with the file that removes it. With `--config Release`, what a release never ships, as [release builds](#release-builds) describe, and every shard that the APK compresses. |
| Web | `config.json` of the site and the modules of the plugins. | Every plugin with a web part in `config.json`, and with `--coop same-origin`, every plugin whose web module registers screens, since that opener policy cuts popups off from the page. With `--config Release`, anything a release never holds, as [release builds](#release-builds) list, and any zipped package. |
| Windows, Linux | The folder of the installed app and, on Windows, the application manifest in the resources of the executable. | The executable, the package and the native libraries where the engine finds them, on Windows the Common Controls of version 6 and UTF-8 as the code page in the manifest, on Linux the desktop entry named after the identifier, and with `--config Release`, what a release never ships, as [release builds](#release-builds) describe. |

```text
Warning: The plugin "native-demo" needs "NSCameraUsageDescription" in the "Info.plist" of the app, which the built app lacks.
  Add to platform/apple/ios/Info.plist:
    <key>NSCameraUsageDescription</key>
    <string>The Native Demo plugin asks for the camera to show how a plugin requests a permission.</string>
Warning: The plugin "native-demo" needs the permission "android.permission.POST_NOTIFICATIONS", which the merged manifest of the built app lacks.
  The file platform/android/app/src/main/AndroidManifest.xml removes it with the attribute "tools:node" set to "remove". Delete that entry, or keep it, and the calls that need the permission answer "unsupported".
```

## Privacy manifest

Apps for Apple platforms declare the APIs with required reasons that their code calls, and the engine is part of that code, since `Haylen.xcframework` is a static library. The file `engine/platform/apple/PrivacyInfo.xcprivacy` holds the declarations of the engine, which the Apple artifacts carry next to the framework and C++ apps built with `haylen_add_app` copy into their bundles. The tool `haylen.py` writes `haylen/PrivacyInfo.xcprivacy` from the one of the engine, the `privacy` keys of the Apple plugins and `PrivacyInfo.xcprivacy` at the root of the Apple project of the developer, when it exists: the reasons of every API type join the reasons of that type, the tracking domains and the collected data types gain the items they lack, and the app tracks when any of them says so.

The undefined symbols of `libhaylen.a` show which of those APIs the engine and its dependencies reach:

| Category | Reason | Callers |
| --- | --- | --- |
| File timestamp, `NSPrivacyAccessedAPICategoryFileTimestamp` | `C617.1`: the metadata of files inside the container of the app. | `stat`, `fstat` and `lstat` from the file layer of libuv, whose modification time Varn's `fs.stat` reports, and from Poco, OpenSSL, libsodium, libzip, pugixml, miniaudio and the log backend. |
| System boot time, `NSPrivacyAccessedAPICategorySystemBootTime` | `35F9.1`: time between events of the app and timers. | `mach_absolute_time` from the clock of `sokol_time`, the timers of libuv and the audio of miniaudio. |
| Disk space, `NSPrivacyAccessedAPICategoryDiskSpace` | `E174.1`: whether there is room to write files. | `statfs` from the file systems of libuv and Poco, which report the free space of a disk. |

The engine reads no user defaults, no active keyboards and no system boot time through `systemUptime`. An app that uses these APIs for other reasons, such as the file timestamps of documents the person picks with a dialog, `3B52.1`, adds them in its own `PrivacyInfo.xcprivacy`.

## Native libraries

The `native` section of `app.json` lists the native libraries an app ships, by the name `native.load` takes, as prebuilt files for each platform or as a CMake project that haylen.py builds for each platform it lists. The [native code guide](../platform/native.md#packaging-libraries-with-an-app) describes the section. The `native` section of a plugin adds one more library, named after the plugin id with underscores for its dashes, which haylen.py builds and places in the same way.

```json
{
    "native": {
        "steam_api": {"files": {"macos": "platform/apple/native/libsteam_api.dylib", "windows": "platform/windows/steam_api64.dll", "linux": "platform/linux/libsteam_api.so"}},
        "my_glue": {"cmake": "native", "platforms": ["macos", "ios", "android", "windows", "linux"]}
    }
}
```

The tool `haylen.py` builds a CMake library in `native/<library>/` of the build folder of the app, once per architecture with the settings of the engine artifacts, and joins the architectures of Apple platforms with `lipo`. Then it places every library of the run platform:

| Platform | Place |
| --- | --- |
| macOS and Mac Catalyst | `haylen/native/<platform>-<sdk>/` of the Apple project, which the `Embed native libraries` phase copies into `Contents/Frameworks` and signs. An `.xcframework` gives the slice of the platform. |
| iOS and tvOS | The same, into `Frameworks`, where a dynamic CMake library becomes a framework with its own `Info.plist`. A static library is linked through `HAYLEN_NATIVE_LDFLAGS` in `haylen/Haylen.xcconfig`, and haylen.py writes `haylen/HaylenNativeSymbols.mm`, which keeps and registers the symbols the section lists. |
| Android | `haylen/jniLibs/<abi>/` of the Android project for arm64-v8a, armeabi-v7a and x86_64, built one ABI after the other with the NDK and `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES`. |
| Windows | `haylen/native/` of the Windows project, which the project installs next to the executable. |
| Linux | `haylen/native/` of the Linux project, which the project installs into `lib/` next to the executable, whose `RUNPATH` is `$ORIGIN:$ORIGIN/lib`. |
| Web | Nothing, because the browser loads no native libraries. |

An Apple project gets the libraries of the platform it was prepared for, so a build for another platform in Xcode runs `prepare` for that platform first.

## Splash screens

The `splash` object of `app.json` sets the launch screen of every platform:

```json
{
    "splash": {"logo": "ui/splash.png", "background": "#FF101418", "darkBackground": "#FF000000", "duration": 1.5, "fadeOut": 0.4}
}
```

| Key | Default | Meaning |
| --- | --- | --- |
| `logo` | The Haylen symbol | An image relative to `content/`. |
| `background` | `clearColor` | A color as `#RRGGBB` or `#AARRGGBB`, so by default the launch screen blends into the first frame of the app. |
| `darkBackground` | `background` | The background while the system uses dark colors, which the asset catalog of Apple platforms, the night resources of Android and the `prefers-color-scheme` media query of the web page pick. |
| `duration` | `0` | The least time in seconds that the launch screen stays from the launch of the app. It always stays until the app drew its first frame. |
| `fadeOut` | `0.25` | The seconds the launch screen takes to fade out into the app. |

The runtime ends the launch screen after the first frame that the app drew once the `duration` passed since the runtime started, and the platform fades it out over `fadeOut` seconds. The project changes the rest: the `LaunchScreen.storyboard` of the iOS and tvOS targets of the Apple project, whose layout belongs to the developer, the theme `Theme.Haylen.Splash` that the manifest of the Android project names, which a theme of the project may replace, and the page and `loader.css` of the web site. The app icons live in the asset catalogs of the Apple project and the launcher resources of the Android project, which the developer replaces.

| Platform | Launch screen |
| --- | --- |
| iOS, iPadOS, Mac Catalyst | `ios/LaunchScreen.storyboard`: the `splash-logo` image centered in the safe area over the `splash-background` color, both from the asset catalog. Auto Layout keeps the logo square, at most 200 points and at most half of the safe area in each direction, so it fits every device, orientation and iPad window size. The system takes the launch screen away as soon as the window of the app shows, so `AppleSplash` covers the window with the same storyboard, which the `UILaunchStoryboardName` of the `Info.plist` names, until the runtime ends the splash and fades it out. |
| tvOS | `tvos/LaunchScreen.storyboard` with the same layout and a logo of at most 360 points, kept over the window the same way. |
| Android | The SplashScreen API of `androidx.core:core-splashscreen`, with the `Theme.Haylen.Splash` theme of the manifest showing `haylen_splash_icon` over `haylen_splash_background`, then `Theme.Haylen`, whose window background has the same color. The system splash screen ends with the first frame of the activity window, while `sokol_app` draws only once the activity resumed and its surface exists, so `HaylenSplash` covers the surface of the app from that first frame with a view of the same background and icon, which stays until the runtime ends the splash through `nativeFramePresented` and then fades out over the seconds of `nativeFadeOut`. No black frame shows in between. The icon insets the logo so it fits the circle that Android masks it with, in every orientation, on phones, tablets and TVs. |
| Web | The page shows the logo over the background with the progress bar until the runtime ends the splash through `Module.haylen.onSplashEnded`, which fades it out. |

The tool `haylen.py` writes the logo into the `splash-logo` image set of `haylen/Splash.xcassets` of the Apple project, as `engine/platform/web/haylen-logo.svg`, the SVG of the Haylen symbol, with its vector data preserved when the app names none, and the background into its `splash-background` color set, with the dark background as its dark appearance, which the launch screens of iOS and tvOS find in the catalogs of their target. On Android it writes the background into `haylen/res/values/haylen_splash.xml`, the dark background into `haylen/res/values-night/haylen_splash.xml`, and copies the logo to `haylen/res/drawable/haylen_splash_logo.<ext>`, which replace the defaults of the library, the Haylen symbol as a vector drawable over `#FF101418`. Android splash logos are PNG, WebP or JPEG images. On the web it copies the logo, or the Haylen symbol, next to the files of the page as `splash.<ext>`, and writes both backgrounds into `config.json`.

## Development mode

The runtime turns on development behavior, which is [hot reload](../core/lua.md#hot-reload), only when its command line has `--dev`, or `--dev-server <address>`, which also connects to the development server at the WebSocket address. The option `--native <folder>`, which may repeat, adds a folder that `native.load` searches before the folders of the platform. The first command-line argument that is not an option names the package to play, a folder or a zip, and without one the runtime plays the package bundled with the app. Other options are ignored, because systems add their own, such as the ones Xcode passes to the macOS apps it launches. Android starts apps without a command line, so a debuggable Android app takes the address of the development server from the extra `dev.haylen.developmentServer` of the intent that launched it, and an app that is not debuggable never reads it. A release build, which carries the bootstrap of its app, ignores `--dev` and `--dev-server` with a warning, since a protected release never runs in development. The command `haylen.py run` without `--platform` passes `--dev`, `haylen.py run --platform web` with the Debug configuration serves a page that turns on development through `Module.haylen.development`, and `haylen.py run` with any other platform in the Debug configuration starts a development server and launches the app connected to it, as the next paragraphs describe, so apps built from the templates and launched by hand or by Xcode and Android Studio, shipped apps, sites that `prepare` makes and the Release configuration never run in development mode.

The page of `run --platform web` connects to the development server that `haylen.py` runs on the same port as the site, at `/haylen/development` with a random token of the run that `config.json` hands to the page, and the server refuses a connection without it. The server scans the package of the app twice a second with the rules of the player, leaving hidden, backup and non-Lua source files out and files that are still being written for the next scan, compiles the shaders whose sources changed, and sends every batch of saved files to every open page as one JSON message followed by the bytes of each file. A page reloads them in place, and the terminal prints what each page did, such as `The app reloaded "source/scenes/level.lua" in 4 ms.` A page that opens or reconnects later gets the files that changed since the package was made, and a page of an earlier run of the server gets every file, so it always plays the files on disk. A file larger than 16 MiB stays out with an error that names it, and a shader that fails to compile prints its error in the terminal and on the pages, which keep the last good shader.

The Debug configuration of `run` with any other platform starts the same development server, alone on the port of `--port`, 8000 by default, once the app is built, and launches the app with `--dev-server` and its address, so every file saved in the app reloads in it the way it does in the player and in the pages. The manifest of the server holds the files of the package that the build placed in the project, in `haylen/app` of the Apple project and `haylen/assets/app` of the Android project, so the app catches up on what changed while it was built. The server stops when the launch ends, such as when the app quits or Ctrl+C stops the streamed log.

While the development server of `run` runs in an interactive terminal, with the web platform or any other, the line `r` sends the files saved since the last batch at once, for a save that the watcher has not seen yet, and `R` restarts every connected app from its package with a fresh Lua state, which the app reports back. Every other line typed in the terminal goes to the connected apps as a line of their [developer console](../lua-api/debug.md#console), and the terminal prints the result of each app, so commands run on a phone or a TV from the computer. An app that stops or restarts leaves the server, which says so and keeps serving the apps that connect next.

| Platform | How the app reaches the server |
| --- | --- |
| `android` | `adb reverse` forwards the port of the device, emulator or phone, to this machine, so the app connects to `127.0.0.1`. The address travels as the extra `dev.haylen.developmentServer` of the launch intent, and the forward goes when the launch ends. |
| `ios-simulator`, `tvos-simulator`, `macos`, `catalyst`, `windows`, `linux` | The simulator and this machine share the loopback address of this machine, and the address follows the arguments of `xcrun simctl launch` or of the executable. |
| `ios`, `tvos` | The device reaches this machine over the network, so the server listens on every address of this machine, or on the one `--host` names, and the app connects to the address of this machine on the network, which `xcrun devicectl device process launch` passes. The device and this machine must share a network, and the first connection asks the person on the device for access to the local network. |

## Web loader

The file `loader.js` runs when the page loads:

1. It creates `Module` with the canvas, so `app.js` can add page handlers to `Module.preRun`.
2. It reads `config.json`, which haylen.py writes with the app name and identifier, whether `window.transparent` is set, the splash logo and background, the files of the protected release that download before the app starts, or `app.zip` in development, the size of each of them and of the two `haylen.wasm` files, and the plugins with a web part. It sets the title and the background, and downloads `splash.<ext>`, the logo of the app or the Haylen symbol, once for both the logo of the splash and the icon of the page, which has no icon of its own before. For a transparent app the page itself has no background, so whatever holds the page, such as the page of an editor that embeds it in a frame, shows through the transparent pixels of the canvas, and only the splash keeps the splash background.
3. It checks for WebAssembly and picks the backend: WebGPU when `navigator.gpu` returns an adapter, and WebGL2 otherwise. `?backend=webgpu` or `?backend=webgl2` forces one when the browser supports it. A browser with neither sees a message instead of a blank page.
4. It downloads `<backend>/haylen.wasm` with the manifests and the app domain of the protected release, or with `app.zip` in development, together with one progress bar. Each download counts the bytes it streams against its `Content-Length`, or against the size in `config.json` when the length is missing or describes compressed bytes. Meanwhile it imports the web module of every plugin from `plugins/<id>/`.
5. It hands the WebAssembly bytes to the runtime through `Module.instantiateWasm`, which compiles them instead of downloading `haylen.wasm` again, and the release as `Module.haylen.release`, or the package as `Module.haylen.packageData` in development, then loads `<backend>/haylen.js`. The runtime opens the [content store](../content/content.md#the-web) of the app before it starts, and the content of the release streams from `app/` as the app needs it. Every file of the page downloads once.
6. Before the app starts, a `Module.preRun` callback holds the start with a run dependency while it calls `load(context)` of every plugin in load order, with the context of `Module.haylen.createPluginContext`, as the [plugin guide](../platform/plugins.md#web) describes. A plugin that fails to import or to load replaces the progress bar with its error, and the app does not start.
7. `Module.haylen.onSplashEnded` fades the splash out once the runtime ends it, and `Module.haylen.onStarted` tells the page that the app runs. `Module.haylen.onError` writes every error to the console, and one that happens before the app starts replaces the progress bar with the message and the Lua stack trace. Later errors show on the error screen of the runtime.

`Module.haylen.packageData` takes the bytes of a zipped package, as an `ArrayBuffer` or a `Uint8Array`, which the runtime writes to its file system and plays instead of the bundled package. `Module.haylen.packageUrl` remains for pages that let the runtime download the package itself, and `Module.haylen.loadZip` replaces the running app later. None of them turns on development mode. When `config.json` holds the `development` entry that only `run --platform web` writes, the loader sets `Module.haylen.development` and the address of the development server with its token in `Module.haylen.developmentServer`. The [build guide](build.md#runtime-api) lists the whole runtime API.

## Platform support

| Platform | Status | How it runs |
| --- | --- | --- |
| macOS | Supported | macOS 14.0 and later, because `sokol_app` drives the frames with `-[NSView displayLinkWithTarget:selector:]`, which macOS 14.0 introduced. The desktop player, or the `macOS` target of the Apple template with `--platform macos`. |
| Windows, Linux | Supported | Windows 10 version 1903 and later, the first that applies the UTF-8 code page of the application manifest, through which the engine reads its command line, and Linux with X11 and the glibc of the machine that built the app or later. The desktop player, or the project of the [Windows or Linux template](#windows-and-linux) with `--platform windows` or `--platform linux`. Built and tested on those hosts by CI. The application manifest of Windows apps declares Windows 10 and 11, and Linux apps load GIO and GTK 3 at run time for the theme and the dialogs, and run without them as [haylen.system](../lua-api/system.md#platforms) and [haylen.dialogs](../lua-api/dialogs.md#platforms) describe. |
| iOS, iPadOS | Supported | iOS and iPadOS 16.3 and later. The `iOS` target on iPhone and iPad, with every orientation of `app.json` and every iPad window size. |
| Mac Catalyst | Supported | macOS 13.3 and later. The `iOS` target on the Mac, whose window opens at the size of `app.json` in the points of the Mac, where macOS places it. Mac Catalyst passes only touches and the presses of UIKit to `sokol_app`, and the left mouse button arrives as a touch, so the runtime reads the keyboard through `GCKeyboard`, as on iOS and tvOS, the right and middle buttons and the wheel through `GCMouse`, and the pointer position through a hover gesture on the view of the app. Typed text comes from the native field of the [text input](../ui/text-input.md) of the runtime. |
| tvOS | Supported | tvOS 16.3 and later. The `tvOS` target on Apple TV and its simulator, with the Siri Remote and game controllers. |
| visionOS | Runs the iPad app | The iOS target runs on Apple Vision Pro as a compatible iPad app. A native visionOS slice is not possible yet: `sokol_app` reads `UIScreen` through `windowScene.screen` in eight places, which the visionOS SDK marks unavailable, so its implementation does not compile for visionOS. |
| watchOS | Not possible | The watchOS 27 SDK has no Metal, MetalKit, GameController or AudioToolbox, which the renderer, the input and the audio of the engine need. |
| Android | Supported | Phones, tablets and Android TV from one APK, on arm64-v8a, armeabi-v7a and x86_64, Android 8.1 (API 27) and later. |
| Web | Supported | Desktop and mobile browsers with WebGPU or WebGL2 and the exception handling of WebAssembly, which Chrome 95, Firefox 100 and Safari 15.2 introduced, since Lua unwinds its errors and yields through it. Sound needs `AudioWorklet`, which browsers offer only to pages served over https or from localhost, and apps on other pages run without sound. |

## Notes on dependencies

- The library miniaudio manages the audio session on iOS and tvOS with Objective-C, so its implementation file compiles as Objective-C there.
- The module `sokol_app` asks Android for an OpenGL ES 3.1 context, which some devices and some configurations of the Android emulator lack, while the shaders of the engine are GLSL ES 3.00, which every version runs. The runtime asks instead for the newest version that the device declares in `ro.opengles.version`, because the emulator creates a context of exactly the version an app asks for while every context reports the newest version of the device, so `sokol_gfx` would query the features of that version, such as the limits of OpenGL ES 3.1, in a context that rejects them.
- The headers of Poco ask MSVC to link every Poco library by its file name, such as `PocoFoundationmd.lib`, while the SDK merges those libraries into `haylen.lib`, so apps that link the SDK would look for files that do not exist. The engine defines `POCO_NO_AUTOMATIC_LIBS` for Poco and every target that uses it, since CMake links the Poco libraries by their targets.
- The engine builds miniaudio with AAudio as its only Android backend, and miniaudio uses AAudio from Android 8.1 (API 27) on, because the first AAudio release of Android 8.0 has known faults. The Android library and template therefore set `minSdk` to 27 and the native code builds for API 27, above what GameActivity needs, API 21, what AppCompat and the activity library need, API 23, and what Varn's HTTP transport needs, API 24. `HaylenActivity` reaches the system bars through `WindowCompat` and `WindowInsetsControllerCompat` of `androidx.core`, and covers display cutouts from Android 9 on, where cutouts exist. Back reaches `HaylenActivity` only through the `OnBackPressedCallback` it enables while the app takes back or edits a text field, which Android 13 and later call through the back callbacks of the window that the manifest of the template turns on with `android:enableOnBackInvokedCallback`, so the system plays its predictive back animation when back leaves the app.
- The module `sokol_app` sizes the framebuffer of iOS apps by the screen, which crops every app whose window is smaller than the screen, as Mac Catalyst windows and iPad windows are. The patch `engine/cmake/patches/sokol-ios-view-size.patch`, which CPM applies to the pinned commit, sizes it by the view of the app instead.
- The module `sokol_app` creates its own application delegate on Apple platforms, which on iOS, tvOS and Mac Catalyst also delegates the scene, and it is the only object that receives the launch, the links and user activities that open the app, the shortcut items, the scene life cycle and the registration and delivery of remote notifications. Native plugins need all of them, and swizzling the class of `sokol_app` at run time would hide behavior and fight the SDKs that swizzle the delegate themselves. The patch `engine/cmake/patches/sokol-apple-classes.patch` adds `sapp_desc.apple.delegate_class`, which names the class that `UIApplicationMain` or AppKit creates instead, and lets the scene configuration name the class of the application delegate, so one object delegates both. The runtime names `HaylenSceneDelegate` on iOS, tvOS and Mac Catalyst and `HaylenAppDelegate` on macOS, which derive from the delegates of `sokol_app`, call them for the events they handle and hand every event to the plugins, as the [plugin guide](../platform/plugins.md#events-of-the-app) describes. Since the scene configuration names the class of the application delegate for every scene, `HaylenSceneDelegate` passes only the scene that holds the window of the app to `sokol_app`, gives the windows of screens a delegate of their own and removes other scenes of the app, so `sokol_app` never builds a second window on its global state. UIKit asks the view controller of the window whether the status bar shows and which content color it has, and the plain `UIViewController` of `sokol_app` answers neither, so the same patch adds `sapp_desc.apple.view_controller_class`, which names the subclass that holds the Metal view, and the runtime names `HaylenViewController`, which answers with the status bar of the app.
- The module `sokol_app` hosts Android apps in `NativeActivity`, which extends the plain `Activity`, so it can never be the `ComponentActivity` that the Activity Result API and current SDKs, such as paywalls, payment sheets and biometric prompts, need, and it takes the window surface, so views of the activity never draw over the app. The patch `engine/cmake/patches/sokol-android-gameactivity.patch`, applied after the other patches, hosts them in GameActivity of the AndroidX games libraries 4.4.2, an `AppCompatActivity` whose `SurfaceView` lives in an ordinary view hierarchy. It keeps the render thread of `sokol_app` and its handshakes with the UI thread, copies the input that GameActivity delivers on the UI thread into a queue that the render thread drains, which grows when a slow frame lets it fill up, so no release is ever dropped, translates keys through a key table with the characters of hardware keyboards, decides on the UI thread which keys the app takes, so back and the system keys go on to the views and the system and a release never reaches the app without its press, while `HaylenActivity` hands every release to the app before the views, so the release of a press that the app took reaches it even when a view took the focus in between, such as the hidden field that the press started editing, and follows the Choreographer from Android 10 on, where `AChoreographer_postFrameCallback64` exists, which it looks up at run time, while older devices wait for the display refresh in `eglSwapBuffers`. The module `sokol_app` runs Android frames only while the window of the activity has the focus, which would stop timers, cancels and timeouts under every dialog and leave the surface black when the app comes back under one, so the patch runs them while the activity is resumed and has a surface, whatever window has the focus, and reports the changes of that focus as the focus events that make the app `'inactive'`. A frame swaps its buffers only when it acquired the swapchain, since a frame that drew nothing, such as those of a covered app after its one covered frame, would show a buffer of undefined content, so the surface keeps the last picture, and older devices wait about a display refresh after such a frame. The module `sokol_app` also runs no frames at all while the activity is paused, which would stop the event loop of the app under every other activity, such as a document picker, a permission prompt or the screen of a plugin, and in the background, so the patch ticks a paused activity every 100 milliseconds with a frame that presents nothing, in which the app, in the background from the pause on, runs its event loop and draws nothing. Once the surface is gone the GL context stays current without one (`EGL_KHR_surfaceless_context`), so those ticks still create and release GPU resources. The engine links the static library of GameActivity from the prefab folder of its AAR, which CPM pins by its hash, with `-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode`, since nothing in the library references the JNI entry that GameActivity looks up.
- The module `sokol_app` ends a destroyed Android activity with `exit()`, which runs the static destructors of the process while the rendering threads of Android still use them and aborts the process with a crash report every time the app closes, and it ignores `sapp_quit()` on Android. The patch `engine/cmake/patches/sokol-android-quit.patch` calls the cleanup callback of the runtime when the activity is destroyed, even after its window is gone, by binding the GL context without a surface, so the engine stops and releases its resources, then lets the activity finish normally and the process stay cached like any Android app. It also makes `haylen.quit()` finish the activity. A later activity in the same process starts a new runtime.
- The module `sokol_app` scales the points that trackpads and other precise devices scroll down to a tenth on macOS and to a hundredth on the web, without telling them from the steps of a wheel, and counts each of the three lines that browsers report for a step of a wheel as 1.33 steps, so the same gesture scrolls by different amounts on every platform. The patch `engine/cmake/patches/sokol-scroll-units.patch` reports precise scrolls in the points they move with `scroll_precise` set, and the scroll of wheels in steps of the wheel, one for each step of a mouse wheel on macOS, Windows and Linux, and on the web where the browser reports the wheel in lines, as Firefox does. Chromium and Safari report most wheels in pixels, about 100 for a step, which the page cannot tell from a trackpad, so there a step of a wheel scrolls like a trackpad that moves as far. On macOS the patch also reports the phase of the gesture in `scroll_phase`, from the phase and the momentum phase of the event: the fingers touch the trackpad, move, leave it, or the system adds momentum after them. It also sends the scrolls that only change the phase, when the fingers touch or leave the trackpad, which `sokol_app` dropped for having no movement, so the UI knows when to release the content and run its own inertia, as iOS does. The engine passes the flag on as `Event::preciseScroll` and the phase as `Event::scrollPhase`, and the UI scrolls by the points of a trackpad and by the theme metric `scrollLine` for each step of a wheel, as the [UI guide](../ui/ui.md#scrolling-with-wheels-and-trackpads) describes. Android reports the wheels of mice and the scrolling of touchpads as scroll motion events, which the engine takes from GameActivity before `sokol_app`, in steps of a wheel at the place of the pointer.
- The module `sokol_app` listens for the keys of the whole page on the window and takes the focus moving between the elements of the page, such as from the canvas to a button, for the app losing the focus, so a page that hosts the app next to elements of its own, such as the editor of a website, could keep their keys from the app only by stopping them before they reach those elements. The patch `engine/cmake/patches/sokol-web-page-events.patch` skips a key whose time stamp the runtime names in `Module.sappIgnoredKey`, which it does for the keys of elements that are not the canvas, the page or the fields of the runtime, so those elements still hear them, and reports only the focus of the window.
- The module `sokol_app` turns only the arrows, Select, Menu and Play/Pause presses of UIKit into keys, so a hardware keyboard of an iPad or an Apple TV would reach the app through those keys alone. The runtime reads every key of a hardware keyboard through `GCKeyboard` on iOS, iPadOS, tvOS and Mac Catalyst, and `engine/cmake/patches/sokol-ios-keyboard-presses.patch`, applied last, leaves the presses that carry a key of a keyboard to it, so no key arrives twice, and keeps the presses of remotes and game controllers. On tvOS it keeps the Menu press that the Escape key of a keyboard makes, so Escape goes back like the remote.
- The module `sokol_gfx` declares the `copy_src` usage of images but does not use it yet, so it creates the textures of WebGPU without the `CopySrc` usage, which `graphics::GpuReadback` needs to copy a render target into a buffer the CPU maps. The engine creates render targets with `copy_src`, and `engine/cmake/patches/sokol-webgpu-copy-source.patch` gives the textures with that usage `CopySrc`.
- The module `sokol_gfx` measures no time on the GPU, which the GPU lane of [profiler traces](../lua-api/debug.md#traces) needs, so `engine/cmake/patches/sokol-metal-gpu-time.patch`, applied last, reads the start and end times of the GPU from the completed command buffer of every Metal frame and adds `sg_mtl_gpu_frame_nanoseconds()`, which returns the time of the last completed frame.
