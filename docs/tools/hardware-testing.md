# 🧪 Hardware testing

This page explains how to test Haylen on every platform and GPU it supports, from the command line, without continuous integration, in three places: locally, on machines, simulators, emulators and devices you have, in the browser, and remotely in the cloud, on GPUs and phones rented from Amazon Web Services and Microsoft Azure, with the approximate cost of each.

The tests themselves are described in [Testing](testing.md). This page is about where to run them.

## What to run

Every place runs the same suites:

- The engine tests: `haylen test`.
- The GPU tests, the conformance suite and the reference scenes of the 2D renderer on each graphics API: `haylen test --gpu`, or one API with `--graphics-api <name>`.
- The test project in its run-all mode, which runs every test of `samples/tests` for two seconds each and ends with the line "Ran N tests, and none failed.".
- The benchmarks on each API: `haylen bench --gpu`, described in [Benchmarks](benchmarks.md).
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
| A Mac | Metal on macOS and Mac Catalyst, and the Apple simulators. | `haylen test --gpu`, `haylen test --gpu --platform catalyst` |
| A Windows PC | Direct3D 12, Direct3D 11, Vulkan and OpenGL, on each GPU of the machine. | `haylen test --gpu`, then again with `HAYLEN_TEST_GPU_LOW_POWER=1` and `HAYLEN_TEST_GPU_SOFTWARE=1` |
| A Linux PC | Vulkan and OpenGL on the GPU of the machine, on X11 and Wayland. | `haylen test --gpu` |
| Any machine with Docker | Linux with Vulkan on Mesa lavapipe and OpenGL on Mesa llvmpipe, which are software rasterizers. | Run `haylen test --gpu` inside an Ubuntu container with the Mesa packages that the CI workflow installs. |

The windowed runs of the test project need a desktop session that nobody is using, since the app opens a window. GPU tests draw offscreen and need none, except OpenGL on Windows, whose hidden window needs an interactive session.

### Simulators and emulators

| Target | Command |
| --- | --- |
| iOS and iPadOS simulators | `haylen test --gpu --platform ios-simulator --device <name>` |
| tvOS simulator | `haylen test --gpu --platform tvos-simulator --device <name>` |
| visionOS simulator | `haylen test --gpu --platform visionos-simulator --device <name>`. Install the visionOS platform first with `xcodebuild -downloadPlatform visionOS`. |
| Android emulator | `haylen test --gpu --platform android` builds the GPU test app, installs it, runs it on Vulkan and OpenGL ES, reads the results back and uninstalls it. It starts an emulator without a window when none is running. |

Run the test project with `haylen run samples/tests --platform <target>`. Start its run-all mode from the developer console of the run with `lua require('harness.runner').start()`, so no input reaches the device.

### Real devices

| Device | Command |
| --- | --- |
| iPhone and iPad | `haylen test --gpu --platform ios --device <name or UDID>`. Unlock the device and keep it unlocked during the run. Sign with your own team through `HAYLEN_APPLE_TEAM`. |
| Apple TV | `haylen test --gpu --platform tvos --device <name or UDID>`. Pair it with Xcode first. It must be awake with no screen saver showing. |
| Apple Vision Pro | `haylen test --gpu --platform visionos --device <name or UDID>` |
| Android phone or tablet | `haylen test --gpu --platform android --device <serial>` |

Each run installs a signed test app, runs it, copies the results back and uninstalls it.

## Browser

The web runs in Chrome, which `haylen.py` starts without a window. Install Chrome or Chromium first.

| What | Command |
| --- | --- |
| GPU tests on WebGPU and WebGL2 | `haylen test --gpu --platform web` builds the GPU tests to WebAssembly, opens them in a page and reads the results back. Add `--graphics-api webGpu` or `--graphics-api webGl2` for one API. |
| The checks of the page | `haylen test --platform web` runs the wake lock check and the memory check, which only a browser can show. |
| The test project | `haylen run samples/tests --platform web` serves the page. Open the address it prints, add `?graphicsApi=webGl2` to choose WebGL2, and start run-all from the menu. |

Headless Chrome draws through SwiftShader, its software rasterizer, so its GPU times measure the processor. To draw on the real GPU of the machine, start Chrome with `--enable-gpu`, and with `--use-angle=metal` on a Mac, then open the same pages. Safari and Firefox open the page of `haylen run` too, for a check by hand.

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
| Android, emulator | A Linux machine that runs KVM, such as an Azure `D4s_v5` with nested virtualization or an EC2 `.metal` instance | Install the Android SDK and run `haylen test --gpu --platform android`, which starts the emulator without a window. | About US$ 0.20 an hour on Azure, much more on an EC2 `.metal` instance |
| Android, real devices | Device Farm | [Real devices on Device Farm](#real-devices-on-device-farm) | About US$ 0.17 per device minute |
| Browser, software | Any small Linux machine with Chrome | `haylen test --gpu --platform web` and `haylen test --platform web` | About US$ 0.08 to 0.10 an hour |
| Browser, real GPU | EC2 `g4dn.xlarge` or Azure `Standard_NG8ads_V620_v1`, with Chrome started with `--enable-gpu` | The same pages as [Browser](#browser). | The price of the machine |
| Browsers on Device Farm | Device Farm desktop browsers, such as Chrome, Firefox and Edge | A Selenium session that opens the page of `haylen run --platform web`, served where the browsers reach it. | About US$ 0.005 per browser minute |

### Amazon Web Services

Amazon rents GPU machines by the hour and real Android and iOS devices by the minute. The tools are the AWS command line (`aws`) with your access keys in the environment variables `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`. Never write the keys into a file, a log or the repository.

#### GPU machines on EC2

| Instance | GPU | Tests | Approximate cost |
| --- | --- | --- | --- |
| `g4dn.xlarge`, Linux | NVIDIA Tesla T4 | Vulkan and OpenGL on the NVIDIA driver | About US$ 0.53 an hour on demand |
| `g4dn.xlarge`, Windows | NVIDIA Tesla T4 | Direct3D 12, Direct3D 11, Vulkan and OpenGL on the NVIDIA GRID driver | About US$ 0.71 an hour on demand |

- New accounts often have no quota for spot GPU instances, so use on demand, or ask for spot quota first.
- Use an image with the NVIDIA driver installed, or install the driver that Amazon publishes for the instance type.
- On Windows, turn off the basic display adapter of the machine before the OpenGL tests, or the window gets the software driver.
- Give the disk at least 80 GB on Windows, where the driver installer needs room.

#### Macs on EC2

EC2 rents whole Macs on dedicated hosts. Apple's license makes the shortest allocation of a host 24 hours, so a session costs at least a day of the host, about US$ 16 to 22. Batch every Apple test into that day.

1. Allocate a host with `aws ec2 allocate-hosts --instance-type mac2.metal`, and start a macOS instance on it.
2. Install Xcode and the platforms you need, such as `xcodebuild -downloadPlatform visionOS`.
3. Run `haylen test --gpu`, `haylen test --gpu --platform catalyst`, and the simulator commands of [Simulators and emulators](#simulators-and-emulators).
4. Terminate the instance and release the host with `aws ec2 release-hosts` once 24 hours have passed.

The paravirtual GPU of a virtual Mac is not the GPU of a real one, while an EC2 Mac runs on a real Mac, so its Metal results count as hardware results.

#### Real devices on Device Farm

Device Farm runs in `us-west-2` and costs about US$ 0.17 per device minute. A run of the test project and the GPU app on one device takes about 5 to 20 minutes. It rents real iPhones and iPads too: build the test project and the GPU test app as signed `.ipa` files for your team, upload them, and run them the same way.

1. Build the debug APK of the test project with `haylen run samples/tests --platform android --build-only`, and the GPU test app with `haylen test --gpu --platform android`.
2. Create a project, upload the APKs and a custom test spec that starts the app in its run-all mode, and collect logcat.
3. Choose devices of different GPUs, such as Adreno, Mali, PowerVR and Xclipse, and of old and new Android versions. The free call `aws devicefarm get-device-pool-compatibility` shows which devices accept the app before you pay.
4. Give every job a short timeout, so a hang cannot bill its full default time.

### Microsoft Azure

Azure rents the AMD Radeon PRO V620, which covers AMD GPUs. The tool is the Azure command line (`az`), logged in with `az login`.

| Size | GPU | Tests | Approximate cost |
| --- | --- | --- | --- |
| `Standard_NG8ads_V620_v1`, Linux | AMD Radeon PRO V620 | Vulkan on RADV and OpenGL on radeonsi | About US$ 0.70 to 1.00 an hour |
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

## Saving money

Rented machines bill while they run, whether they test or wait. Follow these rules:

- **Build first, rent second.** Build every binary before any machine starts: Linux binaries in a container of the same processor family, and Windows binaries on a Windows machine. A rented machine only receives the binaries and runs them.
- **One batch.** Plan every test and run them all in one session, never one idea per session.
- **A timer on every machine.** Start each machine with a shutdown timer, such as `shutdown -h +90` in its startup script, and with the instruction to delete itself on shutdown.
- **Delete, then check.** Delete every machine, disk, key pair, security group, bucket and project you created as soon as the results are back. Then check with the listing commands (`aws ec2 describe-instances`, `aws devicefarm list-projects`, `az group exists`) that nothing is left.
- **One group per session.** On Azure, create everything in one resource group, so `az group delete` removes it all.
