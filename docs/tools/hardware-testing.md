# 🧪 Hardware testing

This page explains how to test Haylen on every platform and GPU it supports, from the command line, without continuous integration, in three places: locally, on machines, simulators, emulators and devices you have, in the browser, and remotely in the cloud, on GPUs and phones rented from Amazon Web Services and Microsoft Azure, with the approximate cost of each.

The tests themselves are described in [Testing](testing.md). This page is about where to run them.

## What to run

Every place runs the same suites:

- The engine tests: `haylen test`.
- The GPU tests, the conformance suite and the reference scenes of the 2D renderer on each graphics API: `haylen test --gpu`, or one API with `--graphics-api <name>`.
- The test project in its run-all mode, which runs every test of `samples/tests` for two seconds each and ends with the line "Ran N tests, and none failed.".
- The benchmarks on each API: `haylen engine bench --gpu`, described in [Benchmarks](benchmarks.md).
- The stress samples in `samples/stress`, whose readout shows the API and the GPU time.

Two variables change which GPU a device takes:

| Variable | Effect |
| --- | --- |
| `HAYLEN_TEST_GPU_SOFTWARE=1` | The devices take the software rasterizer of their API, such as WARP on Direct3D. |
| `HAYLEN_TEST_GPU_LOW_POWER=1` | The devices take the GPU that uses the least power, such as the integrated GPU of a laptop with two GPUs. |

## Local

### Machines you have

| Machine | What it tests | Commands |
| --- | --- | --- |
| A Mac | Metal on macOS and Mac Catalyst, and the Apple simulators. | `haylen test --gpu`, `haylen test --gpu --target catalyst` |
| A Windows PC | Direct3D 12, Direct3D 11, Vulkan and OpenGL, on each GPU of the machine. | `haylen test --gpu`, then again with `HAYLEN_TEST_GPU_LOW_POWER=1` and `HAYLEN_TEST_GPU_SOFTWARE=1` |
| A Linux PC | Vulkan and OpenGL on the GPU of the machine, on X11 and Wayland. | `haylen test --gpu` |
| Any machine with Docker | Linux with Vulkan on Mesa lavapipe and OpenGL on Mesa llvmpipe, which are software rasterizers. | Run `haylen test --gpu` inside an Ubuntu container with the Mesa packages that the CI workflow installs. On an arm64 machine, such as a Mac with Apple silicon, those packages also bring Vulkan drivers of hardware that a container lacks, whose loader reports errors that the tests count, so set `VK_DRIVER_FILES` to `/usr/share/vulkan/icd.d/lvp_icd.json` to load lavapipe alone. |

Drivers keep the shaders they compiled in a cache on the disk, so a problem that comes with compiling shows up only while the cache is empty, as in the first run on a new machine. Clear the cache of the driver before each run of a batch that checks such a problem, such as `~/.cache/AMD` on Linux and `%LOCALAPPDATA%\AMD` on Windows for the drivers of AMD.

The windowed runs of the test project need a desktop session that nobody is using, since the app opens a window. GPU tests draw offscreen and need none, except OpenGL on Windows, whose hidden window needs an interactive session.

### Simulators and emulators

| Target | Command |
| --- | --- |
| iOS and iPadOS simulators | `haylen test --gpu --target ios-simulator --device <name>` |
| tvOS simulator | `haylen test --gpu --target tvos-simulator --device <name>` |
| visionOS simulator | `haylen test --gpu --target visionos-simulator --device <name>`. Install the visionOS platform first with `xcodebuild -downloadPlatform visionOS`. |
| Android emulator | `haylen test --gpu --target android` builds the GPU test app, installs it, runs it on Vulkan and OpenGL ES, reads the results back and uninstalls it. It starts an emulator without a window when none is running. |

The UI tests of an app run on the iOS, iPadOS, tvOS and visionOS simulators with `haylen test --ui <app> --target ios-simulator|tvos-simulator|visionos-simulator --device <name>`, which taps, drags, presses the remote, answers the prompts and taps the notifications there without a window on the Mac, and on the Android emulator with `haylen test --ui <app> --target android`, which starts the emulator without a window when none is running, as [the UI tests](testing.md#the-ui-tests) describe.

Run the test project with `haylen app run samples/tests --target <target>`. Start its run-all mode from the developer console of the run with `lua require('harness.runner').start()`, so no input reaches the device.

### Real devices

| Device | Command |
| --- | --- |
| iPhone and iPad | `haylen test --gpu --target ios --device <name or UDID>`. Unlock the device and keep it unlocked during the run. Sign with your own team through `HAYLEN_APPLE_TEAM`. |
| Apple TV | `haylen test --gpu --target tvos --device <name or UDID>`. Pair it with Xcode first. It must be awake with no screen saver showing. |
| Apple Vision Pro | `haylen test --gpu --target visionos --device <name or UDID>` |
| Android phone or tablet | `haylen test --gpu --target android --device <serial>` |

Each run installs a signed test app, runs it, copies the results back and uninstalls it.

#### Plugins on real devices

The plugin tests of the test project and the samples of the plugins run on a device for what a simulator or an emulator cannot show: the camera, the microphone, the haptics, the motion sensors and Bluetooth, and the prompts, the notifications, the back button of Android and the remote of a TV on real hardware. They are [UI tests](testing.md#the-ui-tests), which answer the prompts, tap the notifications, go home, close the app and open it again from a notification without a person:

```sh
haylen test --ui samples/tests --target ios --device <name or UDID>
haylen test --ui samples/tests --target tvos --device <name or UDID>
haylen test --ui samples/tests --target android --device <serial>
```

Each run installs the app and its runner, runs the tests, copies the screenshots back and uninstalls both. An iPhone or an iPad stays unlocked during the run, and an Android device has the confirmation of installs over USB turned off, as the settings of its maker name it. tvOS launches no app while the Apple TV sleeps, so each test of the Apple TV presses the remote before it launches the app, which wakes it as the press of a person does. The checks of each test are the tables of [the UI tests](testing.md#the-tests-of-the-test-project) and of the samples of the plugins.

Each kind of device runs these suites:

| Device | Suites |
| --- | --- |
| iPhone or iPad | PLG-008, PLG-009, PLG-016, PLG-017, PLG-019, and the samples of the camera, the microphone, the location, notifications, haptics, motion and Bluetooth |
| Apple TV | PLG-008, PLG-009, PLG-017, PLG-019 and the keyboard of the search |
| Android phone or tablet | PLG-008, PLG-009, PLG-015, PLG-016, PLG-017, PLG-019, and the samples |

The posture of the foldable iPhone in DeviceHub is the one check that no API of the system drives, so it stays out of routine runs.

## Browser

The web runs in Chrome, which `haylen test` starts without a window. Install Chrome or Chromium first.

| What | Command |
| --- | --- |
| GPU tests on WebGPU and WebGL2 | `haylen test --gpu --target web` builds the GPU tests to WebAssembly, opens them in a page and reads the results back. Add `--graphics-api webGpu` or `--graphics-api webGl2` for one API. |
| The checks of the page | `haylen test --target web` runs the error screen check, the wake lock check, the fullscreen check, the plugin check, the editor check and the memory check, which only a browser can show. Add `--graphics-api webGpu` or `--graphics-api webGl2` for one API. Headless Chrome on Linux cannot present a WebGPU canvas, so there they take `--graphics-api webGl2`. |
| The test project | `haylen app run samples/tests --target web` serves the page. Open the address it prints, add `?graphicsApi=webGl2` to choose WebGL2, and start run-all from the menu. |

Headless Chrome draws through SwiftShader, its software rasterizer, so its GPU times measure the processor. To draw on the real GPU of the machine, start Chrome with `--enable-gpu`, and with `--use-angle=metal` on a Mac, then open the same pages. Safari and Firefox open the page of `haylen app run` too, for a check by hand.

## Cloud

Every platform but the Apple TV and the Apple Vision Pro, which no cloud rents as devices, can be tested on rented machines. Prices change, so check them before a session. The ones below are on demand, in US dollars, at the time of writing.

| Platform | Where | How | Approximate cost |
| --- | --- | --- | --- |
| Linux, software rasterizers | Any small Linux machine, such as an EC2 `t3.large` or an Azure `D2s_v5` | Run the GPU tests on Mesa lavapipe and llvmpipe, as in a container. | About US$ 0.08 to 0.10 an hour |
| Linux, NVIDIA | EC2 `g4dn.xlarge` | [GPU machines on EC2](#gpu-machines-on-ec2) | About US$ 0.53 an hour |
| Linux, AMD | Azure `Standard_NG8ads_V620_v1` | [Microsoft Azure](#microsoft-azure) | About US$ 0.70 to 1.00 an hour |
| Windows, NVIDIA | EC2 `g4dn.xlarge` with Windows | [GPU machines on EC2](#gpu-machines-on-ec2) | About US$ 0.71 an hour |
| Windows, AMD | Azure `Standard_NG8ads_V620_v1` with Windows | [Microsoft Azure](#microsoft-azure) | A little more than the Linux machine |
| Windows, WARP | Any Windows machine without a GPU, such as an Azure `D2s_v5` | Run the GPU tests with `HAYLEN_TEST_GPU_SOFTWARE=1`. | About US$ 0.15 to 0.20 an hour |
| macOS, Mac Catalyst and the Apple simulators | EC2 Mac, such as `mac2.metal` (Apple M1) or `mac2-m2.metal` (Apple M2) | [Macs on EC2](#macs-on-ec2) | About US$ 0.65 to 0.90 an hour, for at least 24 hours |
| iPhone and iPad | Device Farm | [Real devices on Device Farm](#real-devices-on-device-farm) | About US$ 0.17 per device minute |
| Android, emulator | A Linux machine that runs KVM, such as an Azure `D4s_v5` with nested virtualization or an EC2 `.metal` instance | Install the Android SDK and run `haylen test --gpu --target android`, which starts the emulator without a window. | About US$ 0.20 an hour on Azure, much more on an EC2 `.metal` instance |
| Android, real devices | Device Farm | [Real devices on Device Farm](#real-devices-on-device-farm) | About US$ 0.17 per device minute |
| Browser, software | Any small Linux machine with Chrome | `haylen test --gpu --target web` and `haylen test --target web --graphics-api webGl2` | About US$ 0.08 to 0.10 an hour |
| Browser, real GPU | EC2 `g4dn.xlarge` or Azure `Standard_NG8ads_V620_v1`, with Chrome started with `--enable-gpu` | The same pages as [Browser](#browser). | The price of the machine |
| Browsers on Device Farm | Device Farm desktop browsers, such as Chrome, Firefox and Edge | A Selenium session that opens the page of `haylen app run --target web`, served where the browsers reach it. | About US$ 0.005 per browser minute |

### Amazon Web Services

Amazon rents GPU machines by the hour and real Android and iOS devices by the minute. The tools are the AWS command line (`aws`) with your access keys in the environment variables `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`. Never write the keys into a file, a log or the repository.

#### GPU machines on EC2

| Instance | GPU | Tests | Approximate cost |
| --- | --- | --- | --- |
| `g4dn.xlarge`, Linux | NVIDIA Tesla T4 | Vulkan and OpenGL on the NVIDIA driver | About US$ 0.53 an hour on demand |
| `g4dn.xlarge`, Windows | NVIDIA Tesla T4 | Direct3D 12, Direct3D 11, Vulkan and OpenGL on the NVIDIA GRID driver | About US$ 0.71 an hour on demand |

- New accounts often have no quota for spot GPU instances, so use on demand, or ask for spot quota first.
- EC2 offers no AMD instance `g4ad` in `us-east-1`, `us-east-2` and `us-west-2`, so AMD GPUs come from [Microsoft Azure](#microsoft-azure).
- Install the NVIDIA GRID driver that Amazon publishes in the buckets `ec2-windows-nvidia-drivers` and `ec2-linux-nvidia-drivers`. The machine downloads it from an address that `aws s3 presign` makes with an expiry of a few minutes, so no key ever reaches the machine.
- On Windows, turn off the basic display adapter of the machine before the OpenGL tests, or the window gets the software driver. The GRID driver then shows two displays, so the outputs of the test project open on the second one.
- On Linux, the GRID driver has four display heads without monitors. An X server whose device section holds `Option "ConnectedMonitor" "DFP-0, DFP-1"`, the mode validation `NoEdidModes` and a `MetaModes` of two modes has two displays for the windowed runs and the outputs. Close a window there through the window manager, such as with `wmctrl -c`, since `xdotool windowclose` destroys it under the app.
- Give the disk at least 80 GB on Windows, where the driver installer needs room.
- Windows builds the engine with what [Building](build.md#requirements) lists for the target `windows`, which a fresh machine lacks: Visual Studio Build Tools, Git for Windows, and Strawberry Perl with its NASM. Run the windowed checks from a scheduled task of the session that autologon opens, with its console hidden so it never takes the keyboard from the app.
- The desktop SDK that continuous integration builds needs the glibc of its runner, so a machine with an older Ubuntu builds its own SDK with `haylen sdk build --target linux`.
- Ubuntu empties `/tmp` at every boot, so keep downloads in the home folder. A `shutdown` in the `bootcmd` of cloud-init runs before systemd takes it, so arm the shutdown timer from a systemd unit that runs after `multi-user.target`, which also arms it again after a reboot.

#### Macs on EC2

EC2 rents whole Macs on dedicated hosts. Apple's license makes the shortest allocation of a host 24 hours, so a session costs at least a day of the host, about US$ 16 to 22. Batch every Apple test into that day.

1. Allocate a host with `aws ec2 allocate-hosts --instance-type mac2.metal`, and start a macOS instance on it.
2. Install Xcode and the platforms you need, such as `xcodebuild -downloadPlatform visionOS`.
3. Run `haylen test --gpu`, `haylen test --gpu --target catalyst`, and the simulator commands of [Simulators and emulators](#simulators-and-emulators).
4. Terminate the instance and release the host with `aws ec2 release-hosts` once 24 hours have passed.

The paravirtual GPU of a virtual Mac is not the GPU of a real one, while an EC2 Mac runs on a real Mac, so its Metal results count as hardware results.

#### Real devices on Device Farm

Device Farm runs in `us-west-2` and costs about US$ 0.17 per device minute. The UI tests of an app run there through the tooling, on the phones that `--device-farm` names by the names that Device Farm gives them:

```sh
haylen test --ui samples/tests --target android --device-farm "Google Pixel 9,Samsung Galaxy S24"
haylen test --ui samples/tests --target ios --device-farm "Apple iPhone 16"
```

The run builds the app and the runner, the test APK that instruments itself on Android and the runner of the team of the app as an IPA on iOS, creates a project of Device Farm for the run, uploads both, runs them once on each device with a timeout of 30 minutes per device, prints what each device ended with and copies the files that Device Farm keeps of each job, such as the log and the video, into `ui-tests/device-farm-<platform>` of the build folder of the app. Whatever the run ends with, it deletes the project with its uploads and its run, and then checks that Device Farm no longer lists it. The AWS command line takes the credentials from the environment. A run of the test project and the GPU app on one device takes about 5 to 20 minutes. It rents real iPhones and iPads too: build the test project and the GPU test app as signed `.ipa` files for your team, upload them, and run them the same way.

1. Build the debug APK of the test project with `haylen app build samples/tests --target android`, and the GPU test app with `haylen test --gpu --target android`, which leaves it in `build/engine/android-gpu-tests-<abi>-debug/apk/haylen-gpu-tests.apk`.
2. Create a project, upload the APKs and a custom test spec that starts the app in its run-all mode, and collect logcat.
3. Choose devices of different GPUs, such as Adreno, Mali, PowerVR and Xclipse, and of old and new Android versions. The free call `aws devicefarm get-device-pool-compatibility` shows which devices accept the app before you pay.
4. Give every job a short timeout, so a hang cannot bill its full default time.

A custom test spec runs a shell script of the test package on a host that reaches the device through `adb -s $DEVICEFARM_DEVICE_UDID`, and keeps what the script writes into `$DEVICEFARM_LOG_DIR`. The `adb` of that host refuses `logcat -v raw`, so read the log in another format. Set the secure setting `immersive_mode_confirmations` to `confirmed` before the first start of an app, or the hint of the system covers it. The GPU test app keeps its results in its own files, which `adb exec-out run-as com.haylen.gputests tar -c -C files gpu-test-output` copies out. The D-pad keys of `adb shell input keyevent` move the focus of the test project, and `pm clear` before each start gives the menu the focus of a first start.

### Microsoft Azure

Azure rents the AMD Radeon PRO V620, which covers AMD GPUs. The tool is the Azure command line (`az`), logged in with `az login`.

| Size | GPU | Tests | Approximate cost |
| --- | --- | --- | --- |
| `Standard_NG8ads_V620_v1`, Linux | AMD Radeon PRO V620 | Vulkan on RADV and on the AMD driver, and OpenGL on the AMD driver, with the kernel that the note below names | About US$ 0.70 to 1.00 an hour |
| `Standard_NG8ads_V620_v1`, Windows | AMD Radeon PRO V620 | Direct3D 12, Direct3D 11, Vulkan and OpenGL on the AMD driver | A little more, for the Windows license |

Prepare the subscription once:

```sh
az provider register -n Microsoft.Compute
az provider register -n Microsoft.Quota
az extension add --name quota
az quota update --resource-name StandardNGADSV620v1Family \
  --scope /subscriptions/<subscription>/providers/Microsoft.Compute/locations/eastus2 \
  --limit-object value=8 --resource-type dedicated
```

The quota of 8 virtual processors holds one machine at a time, so run the Linux machine and the Windows machine one after the other.

The V620 of these machines is a virtual function of a shared GPU, whose driver must load firmware through the host. The Ubuntu 24.04 image of Azure blacklists `amdgpu`, and the `amdgpu` of the newest kernel of the image fails there with `PSP firmware loading failed`. The `amdgpu` of the Azure kernel `6.8.0-1071-azure` loads the firmware, so install that kernel, take `amdgpu` off the blacklist and add Mesa RADV and the Vulkan and OpenGL drivers of AMD. The `amdgpu-dkms` that Azure names does not compile for that kernel, so the module of the kernel is the driver. The OpenGL driver of AMD takes over the EGL of the machine, so Mesa radeonsi does not run beside it. Check the driver version that Azure supports for the size before renting it again.

## What the hardware covers

These machines and devices ran the whole battery on real GPUs, beyond what continuous integration, the simulators and the emulator show.

| Machine or device | GPU and driver | What passes |
| --- | --- | --- |
| EC2 `g4dn.2xlarge`, Windows Server 2025 | NVIDIA Tesla T4, GRID driver 596 | The engine tests. The GPU tests on Direct3D 12, Direct3D 11, Vulkan and OpenGL. The test project in the player on each of the four APIs. The installer of a Lua app, which installs, starts, closes and uninstalls it cleanly, and a C++ app in Debug and Release. Unicode text typed into a field, and an output on the second display. |
| EC2 `g4dn.2xlarge`, Ubuntu 24.04 | NVIDIA Tesla T4, GRID driver 595 | The engine tests. The GPU tests on Vulkan and OpenGL with an X server of two displays. The test project in the player on Vulkan and OpenGL. Text typed into a field, an output on the second display, and the `.deb` and the AppImage of a Lua app, each installed and started. |
| Device Farm, Samsung Galaxy S25, Android 16 | Qualcomm Adreno 830 | The GPU tests on Vulkan and OpenGL ES, the run of every test of the test project, the system keyboard of a field and the back key. |
| Device Farm, Google Pixel 9, Android 16 | ARM Mali-G715 | The same as the Galaxy S25. |
| Device Farm, Google Pixel 10, Android 17 | Imagination PowerVR D-Series DXT-48 | The GPU tests on Vulkan and OpenGL ES, the system keyboard of a field and the back key. |
| Device Farm, Xiaomi Redmi Note 13, Android 15 | ARM Mali-G57 MC2 | The same as the Pixel 10. |
| Azure `Standard_NG8ads_V620_v1`, Ubuntu 24.04 | AMD Radeon PRO V620, Mesa RADV 25.2 and the AMD driver 25.10 | The GPU tests on Vulkan on RADV and on the AMD driver with the validation layers, and on OpenGL through EGL without a display. The benchmarks and the stress tours on both APIs. |
| Azure `Standard_NG8ads_V620_v1`, Windows Server 2025 | AMD Radeon PRO V620, AMD driver 25.1 | The GPU tests on Direct3D 12, Direct3D 11, Vulkan and OpenGL. The benchmarks and the stress tours in a window on the four APIs. |

The stress samples show how much each phone carries on Vulkan, as frames per second and the mean time of a frame:

| Device | Many Sprites, 20,000 fish | Many Bodies, 3,000 toys at rest | Busy Interface, 12 live cards |
| --- | --- | --- | --- |
| Samsung Galaxy S25 | 120, 7.7 ms | 120, 5.0 ms | 100, 9.9 ms |
| Google Pixel 9 | 35, 28.7 ms | 60, 15.3 ms | 60, 16.0 ms |
| Google Pixel 10 | 38, 26.0 ms | 60, 14.7 ms | 60, 14.7 ms |
| Xiaomi Redmi Note 13 | 20, 50.5 ms | 36, 27.8 ms | 36, 27.5 ms |

What the hardware cannot show yet:

- The Mali-G57 measures no time on the GPU, since its OpenGL ES has no timer queries and its Vulkan driver offers no timestamps that the Vulkan device reads, so the readouts say "Not measured" there.
- A server of Windows has no touch screen, so the touch keyboard of Windows, which shows for a field only on a touch device, needs a tablet or a laptop with a touch screen.
- Mesa radeonsi runs on no rented GPU, since the OpenGL driver of AMD takes over the EGL of the Azure machine, as [Microsoft Azure](#microsoft-azure) says.

## Saving money

Rented machines bill while they run, whether they test or wait. Follow these rules:

- **Build first, rent second.** Build every binary before any machine starts: Linux binaries in a container of the same processor family, and Windows binaries on a Windows machine. A rented machine only receives the binaries and runs them.
- **One batch.** Plan every test and run them all in one session, never one idea per session.
- **A timer on every machine.** Start each machine with a shutdown timer, such as `shutdown -h +90` in its startup script, and with the instruction to delete itself on shutdown.
- **Delete, then check.** Delete every machine, disk, key pair, security group, bucket and project you created as soon as the results are back. Then check with the listing commands (`aws ec2 describe-instances`, `aws devicefarm list-projects`, `az group exists`) that nothing is left.
- **One group per session.** On Azure, create everything in one resource group, so `az group delete` removes it all. The first virtual network of a region makes Azure create the group `NetworkWatcherRG` outside it, so delete that group too when the session created it.
