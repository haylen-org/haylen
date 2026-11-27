# ☁️ Cloud Tracker

Only the coordinator writes this file. The title and the section titles stay exactly as written here. It is updated right after every change of state: a launch, a timer extension, an attach, a run or build that starts or ends, a spend check, a quota answer, a go from the person, a deletion or a leftover. Each update sets `Last updated`, the status emoji of every row it touches and the matching Overview row.

Times are UTC. Each cost says `estimate` (price seen × time) until the billing data arrives, and then `billed`. This file never holds sensitive data: no credentials, keys, tokens, passwords, connection strings, IP addresses, host names, presigned URLs, device serials or UDIDs, personal or customer data, e-mail addresses, account or subscription ids, or private absolute paths. Devices are named by role, accounts by provider and credentials by variable name.

Last updated: 2026-11-27 04:50 UTC

## 🚦 Legend

Every row of a table with a Status column starts with one status emoji, so the state shows at a glance.

| Emoji | Status | Used for |
| --- | --- | --- |
| 🟢 | Good | Running as planned, finished with every test passing and cleanup proven, quota enough |
| 🟡 | Waiting | Starting, queued, uploading, waiting for the person's go, quota request pending |
| 🟠 | Attention | Timer close while jobs remain, on-demand fallback, cost above its estimate, finished with failed tests |
| 🔴 | Problem | Machine lost, run or build failed to run, credential failure, over budget, resource left after cleanup |
| ⚪ | Idle | Deallocated, stopped, released, nothing running |
| 🔵 | Planned | Queued for a later batch or day |

Results of jobs and tests: ✅ passed, ❌ failed, ⏭️ skipped for a missing capability, ⏳ running.

Cost of a service's share this month: 🟢 under 50 %, 🟡 50 % to under 80 %, 🟠 80 % to 100 %, 🔴 over 100 %. Cost of one batch against the daily allowance: 🟢 inside it, 🟠 needs the person's go, 🔴 started above it.

Platforms: 🐧 Linux, 🪟 Windows, 🍎 Apple (macOS, Mac Catalyst, iOS, iPadOS, tvOS, visionOS), 🤖 Android, 🌐 Web, 🎮 GPU work.

Services: ☁️ AWS EC2, 🔷 Azure, 🏠 Own Machines, 📱 AWS Device Farm, ⚙️ CI.

## 📊 Overview

| Status | Service | Running now | This month | Share | Cost status |
| --- | --- | --- | --- | --- | --- |
| 🟢 | ☁️ AWS EC2 | batch4 (spot) | US$28.30 (estimate) | US$110 | 🟢 |
| ⚪ | 🔷 Azure | None | US$4.60 (estimate) | US$10 | 🟡 |
| 🟢 | 🏠 Own Machines | The Mac, with incremental test builds of physics3d | No cost | No cost | 🟢 |
| ⚪ | 📱 AWS Device Farm | None | US$0 | US$0, any run needs a go | 🟢 |
| 🟡 | ⚙️ CI | The push run of `b1bc0d93c` | Not measured | Public repository | 🟢 |

## 🧩 Platforms

The latest result of each platform, so a broken platform shows even when nothing runs.

| Status | Platform | Last checked (UTC) | Where it ran | Commit | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 🟠 | 🐧 Linux | 2026-11-27 04:20 | batch3 | KB-055, nodes3d and animation3d heads | ✅ KB-055, ✅ nodes3d, ✅ animation3d, ❌ thread sanitizer on navigation3d | The navigation race is KB-352, and its fix runs in batch4 |
| 🟢 | 🪟 Windows | 2026-11-27 01:00 | Windows laptop | test-hygiene `6e09a22b7` | ✅ MSVC with warnings as errors, 3330 of 3330 | |
| 🟢 | 🍎 macOS | 2026-11-26 | The Mac | KB-055 | ✅ engine tests 3340, ✅ Metal GPU tests 115 | |
| ⚪ | 🍎 Mac Catalyst | | | | | Not checked since this tracker started |
| 🟢 | 🍎 iOS | 2026-11-26 | iOS simulator on the Mac | KB-055 | ✅ GPU tests 109 | |
| ⚪ | 🍎 iPadOS | | | | | Not checked since this tracker started |
| ⚪ | 🍎 tvOS | | | | | Not checked since this tracker started |
| ⚪ | 🍎 visionOS | | | | | Not checked since this tracker started |
| 🟢 | 🤖 Android | 2026-11-27 03:30 | batch3, the emulator image of CI | KB-055 `304b884d8` | ✅ GPU tests on Vulkan and OpenGL ES | |
| 🟢 | 🌐 Web | 2026-11-27 03:00 | batch3, headless browsers | KB-055 `304b884d8` | ✅ WebGPU and WebGL2, ✅ web SDK | |
| 🟢 | 🎮 GPU on Linux | 2026-11-27 03:00 | batch3, lavapipe and llvmpipe | KB-055 `304b884d8` | ✅ Vulkan, ✅ OpenGL | |
| 🟡 | 🎮 GPU on Windows | 2026-11-26 | Windows laptop | KB-055 `189417281` | ✅ Direct3D 11 and 12 on three GPUs over SSH, ⏭️ the surface readback test, which needs a desktop session | The Azure desktop run waits for a batch that builds OpenSSL in the tree (KB-354) |

## 🏃 Running Now

"What it runs" names the lines of work and their commits. A row leaves this section when its machine, run or build ends, and its result goes to the batch log.

### ☁️ AWS EC2

| Status | Session | Platform | Machine type | Market | Zone | Started | Ends at | What it runs | Price per hour | Cost so far |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | batch4 | 🐧 Linux | c8i.8xlarge (32 vCPU, 64 GB) | spot | us-east-1c | 2026-11-27 04:16 | 2026-11-27 10:16 | physics3d (8 commits with clang and GCC, sanitizers, fuzzers, web, bench) and the navigation3d bake fix `9bfd40dd1` (clang, GCC, both sanitizers) | US$0.67 | US$0.20 (estimate) |

### 🔷 Azure

| Status | Session | Platform | Machine type | Market | Region | Started | Ends at | What it runs | Price per hour | Cost so far |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ⚪ | None | | | | | | | | | |

### 🏠 Own Machines

| Status | Machine (by role) | Platform | Lock held | Started | What it runs | Free disk |
| --- | --- | --- | --- | --- | --- | --- |
| 🟢 | The Mac | 🍎 macOS | Only during incremental test builds | 2026-11-27 | physics3d: the character body and the rigid character, outside the bundles | 111 GB |
| ⚪ | Windows laptop | 🪟 Windows | No | | Idle, a clean clone at main | 80 GB |

### 📱 AWS Device Farm

| Status | Run | Platform | Devices (model and GPU) | Physical or virtual | Started | Job timeout | Tests | Device minutes | Cost so far |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ⚪ | None | | | | | | | | |

### ⚙️ CI

| Status | Run | Workflow | Ref | Started by | Started | Runners | Minutes | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟡 | Push run | CI | main `b1bc0d93c` | Push | 2026-11-27 04:33 | Linux and macOS | | ⏳ |

## 🗄️ Persistent Resources

| Status | Service | Resource | Kind | Region | Why it is kept | Monthly cost |
| --- | --- | --- | --- | --- | --- | --- |
| ⚪ | ☁️ AWS EC2 | haylen-batch-linux-20261126-2037 and its snapshot | Prepared image | us-east-1 | The Linux batch machines start warm from it | US$1 |
| ⚪ | 🔷 Azure | haylen-batch-windows with its OS disk, NIC, NSG and VNet | Deallocated VM | eastus2 | The Windows GPU batch machine with its toolchains | US$5.90 (the disk) |
| ⚪ | Cache bucket | haylen-ci on Cloudflare R2 | Bucket | ENAM | The compiler cache and the dependency caches, objects expire after 30 days | Storage plus operations |

## 💰 Budget

Monthly limit: US$120. Month: 2026-11. Days left in the month: 4.

| Cost status | Service | Share | Spent this month | Figure from | Left | Daily allowance | Projected month end |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | ☁️ AWS EC2 | US$110 (Linux batches US$75, the monthly cloud Mac day US$30, the image US$5) | US$28.30 | Billing data of 2026-11-26 (US$23.85), plus estimates after it | US$81.70 | US$20.40 | US$50 |
| 🟡 | 🔷 Azure | US$10 | US$4.60 | Estimate, since the usage listing of this subscription returns nothing | US$5.40 | US$1.35 | US$7 |
| 🟢 | 📱 AWS Device Farm | US$0, any run needs a go | US$0 | | | | US$0 |
| 🟢 | ⚙️ CI | Public repository | Not measured | | | | |
| 🟢 | Cache bucket | Inside the shares above | Not measured | | | | |
| 🟢 | **Total** | US$120 | US$32.80 | | US$87.20 | US$21.80 | US$57 |

The daily allowance is the share left ÷ the days left.

## 📏 Quotas

| Status | Service | Quota (family and region) | Current limit | Needed by the plan | Request filed on |
| --- | --- | --- | --- | --- | --- |
| 🟢 | ☁️ AWS EC2 | All Standard Spot Instance Requests, us-east-1 | 64 vCPU | 64 vCPU, two batch machines at once | 2026-11-27, granted |
| 🟢 | ☁️ AWS EC2 | Running On-Demand Standard instances, us-east-1 | 1848 vCPU | 32 vCPU | |
| 🟢 | ☁️ AWS EC2 | Mac dedicated hosts, mac-m4, us-east-1 | 1 host | 1 host, one day a month | 2026-11-26, granted |
| 🟢 | 🔷 Azure | StandardNGADSV620v1Family, eastus2 | 8 vCPU | 8 vCPU, one VM | |

## ⏱️ Measured Durations

| Platform | Kind of job | Machine or service | Duration | Measured on |
| --- | --- | --- | --- | --- |
| 🐧 Linux | One commit with clang and GCC, `haylen test` from a fresh tree | c8i.8xlarge shared by 4 jobs | 30 to 35 min | 2026-11-27 |
| 🐧 Linux | A 2-commit stream with GPU tests, the web SDK and the Android emulator | c8i.8xlarge shared by 4 jobs | 2 h | 2026-11-27 |
| 🐧 Linux | A 4-commit stream with both sanitizers, the web SDK and a benchmark | c8i.8xlarge shared by 4 jobs | 2 h 45 min | 2026-11-27 |
| 🐧 Linux | Short hygiene run (the head with clang, GCC and a diagnostic) | c8i.8xlarge shared | 43 min | 2026-11-27 |
| 🪟 Windows | Cold `haylen test` with MSVC | Windows laptop | 56 min | 2026-11-26 |
| 🍎 macOS | A fresh tree with the remote cache | The Mac, 4 jobs | 21 to 25 min | 2026-11-26 |

## 📜 Batch Log

One row per finished batch, run or build, from every service, own machines included (cost 0). The status covers the run and its cleanup: 🟡 cleanup not proven yet (CI runs leave nothing to delete, so "Deleted and proven" is `not needed`), 🟢 every test passed and cleanup is proven, 🟠 some tests failed but the run and cleanup are fine, 🔴 the run was lost or something was left.

| Status | Date (UTC) | Service | Platforms | Machine or devices | Duration | Cost | Jobs and results | Cache hit rate | Deleted and proven |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | 2026-11-27 04:33 | 🔷 Azure | 🪟 Windows | Standard_NG8ads_V620_v1 | 10 min | US$0.11 (estimate) | ✅ Diagnosis: the prebuilt OpenSSL package of Varn is not found by `find_package` under MSVC (KB-354), proven with a probe project | | ✅ deallocated, IP and rule removed |
| 🔴 | 2026-11-27 04:25 | 🔷 Azure | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 | 4 min, plus an earlier start whose bundle upload failed | US$0.30 (estimate) | ❌ KB-055 gpu-build: configure found no OpenSSL crypto library in the package cache, so no test ran | | ✅ deallocated, IP and rule removed |
| 🟠 | 2026-11-27 01:32 | ☁️ AWS EC2 | 🐧 Linux, 🤖 Android, 🌐 Web, 🎮 GPU | c8i.8xlarge on-demand, because the spot quota was full | 168 min | US$4.20 (estimate) | ✅ KB-055, ✅ nodes3d, ✅ test-hygiene (short), ❌ animation3d head TSan (the navigation race, KB-352), ❌ old physics3d bundle (three defects, fixed for batch4) | 39 % | ✅ |
| 🟢 | 2026-11-27 00:00 | 🏠 Own Machines | 🪟 Windows | Windows laptop | 56 min | US$0 | ✅ test-hygiene `6e09a22b7`, MSVC, 3330 of 3330 | | not needed |
| 🔴 | 2026-11-26 22:10 | ☁️ AWS EC2 | 🐧 Linux, 🤖 Android | c8i.8xlarge spot | 200 min | US$2.20 (estimate) | ✅ test-hygiene, ✅ KB-051 OpenGL ES layers, ❌ physics3d, ❌ animation3d, ❌ nodes3d (a script path error), four attached runs lost when the machine was deleted under them | 10 % | ✅ |
| 🟢 | 2026-11-26 22:00 | 🏠 Own Machines | 🪟 Windows, 🎮 GPU | Windows laptop | 56 min | US$0 | ✅ KB-055 MSVC 3339 of 3339, ✅ Direct3D 11 and 12 on three GPUs | | not needed |

## 🧹 Open Cleanup Items

| Status | Found on | Service | What is left | Why | Removed on |
| --- | --- | --- | --- | --- | --- |
| ⚪ | None | | | | |

## 🗓️ Planned

| Status | Batch | Service | Platforms | Jobs | Estimated duration | Estimated cost | Cost status | Planned for |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🔵 | Full CI matrix | ⚙️ CI | Every platform of CI | main, once the current block of streams lands | 1 to 2 h | Public repository | 🟢 | After batch4 lands |

Next cloud Mac day: not planned. Card KB-346 prepares it.
