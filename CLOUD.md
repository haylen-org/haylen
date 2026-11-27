# ☁️ Cloud Tracker

## 📊 Overview

| Status | Service | Running now | This month | Share | Cost status |
| --- | --- | --- | --- | --- | --- |
| 🟢 | 🔶 AWS EC2 | batch5b and batch6 (on-demand) | US$29.30 (estimate) | US$110 | 🟢 |
| 🟡 | 🔷 Azure | windows-kb355-shared | US$5.65 (estimate) | US$10 | 🟡 |
| 🟢 | 🏠 Own Machines | The Mac, with the web checks of KB-362 and the full tests of the 3D renderer | No cost | No cost | 🟢 |
| ⚪ | 📱 AWS Device Farm | None | US$0 | US$0, any run needs a go | 🟢 |
| 🟡 | ⚙️ CI | The push run of `b1bc0d93c` | Not measured | Public repository | 🟢 |

Last updated: 2026-11-27 08:15 UTC

## 🏃 Running Now

### 🔶 AWS EC2

| Status | Session | Platform | Machine type | Market | Zone | Started | Ends at | What it runs | Price per hour | Cost so far |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | batch5b | 🐧 Linux | c8i.8xlarge (32 vCPU, 64 GB) | on-demand, after two spot reclaims in a row | us-east-1 | 2026-11-27 05:55 | 2026-11-27 12:59, extended for the first wave of the organization cards | physics3d (8 commits, sanitizers, fuzzers, web, bench), the animation3d node components `932f18af5`, the first 3D renderer commits `cd753af65`, the rerun of the shared cache step of KB-355, the first commit of KB-360, the full rerun of KB-355 and KB-356 at `a4ca5b8e3`, KB-361 `24cd1dca1` and the second group of KB-363 `23e0be552` | US$1.50 | US$1.65 (estimate) |
| 🟠 | batch6 | 🐧 Linux | c8i.8xlarge (32 vCPU, 64 GB) | on-demand | us-east-1 | 2026-11-27 06:05 | 2026-11-27 16:18, extended for the physics3d sweep, now about US$15 instead of the US$10.50 of its estimate | KB-345 and KB-347: Clang, thin LTO and lld on Linux, path maps in every object, about 14 builds without the cache and the link memory measurements, KB-350 `fe7b5cf3c`, the 3D navigation `10f67d769`, the first commit of KB-364 `691dc67bc`, the first commit of KB-363 `31a5762a0` and the physics3d vocabulary sweep with KB-173 and KB-176 to KB-178 (12 commits to `379e678a3`) | US$1.50 | US$1.90 (estimate) |

### 🔷 Azure

| Status | Session | Platform | Machine type | Market | Region | Started | Ends at | What it runs | Price per hour | Cost so far |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟡 | windows-kb355-shared | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 (AMD V620) | regular, deallocated between batches | eastus2 | 2026-11-27 07:50 | Within 2 hours | The regression of KB-355 under MSVC in two trees with one cache, the new commits `819ba2dc2` and `a4ca5b8e3` and Direct3D 11 at the head | US$0.64 | starting |

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

## 🧹 Open Cleanup Items

| Status | Found on | Service | What is left | Why | Removed on |
| --- | --- | --- | --- | --- | --- |
| ⚪ | None | | | | |

## 💰 Budget

Monthly limit: US$120. Month: 2026-11. Days left in the month: 4.

| Cost status | Service | Share | Spent this month | Figure from | Left | Daily allowance | Projected month end |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | 🔶 AWS EC2 | US$110 (Linux batches US$75, the monthly cloud Mac day US$30, the image US$5) | US$29.10 | Billing data of 2026-11-26 (US$23.85), plus estimates after it | US$80.90 | US$20.20 | US$50 |
| 🟡 | 🔷 Azure | US$10 | US$5.65 | Estimate, since the usage listing of this subscription returns nothing | US$4.35 | US$1.10 | US$7 |
| 🟢 | 📱 AWS Device Farm | US$0, any run needs a go | US$0 | | | | US$0 |
| 🟢 | ⚙️ CI | Public repository | Not measured | | | | |
| 🟢 | Cache bucket | Inside the shares above | Not measured | | | | |
| 🟢 | **Total** | US$120 | US$32.80 | | US$87.20 | US$21.80 | US$57 |

## 🗓️ Planned

| Status | Batch | Service | Platforms | Jobs | Estimated duration | Estimated cost | Cost status | Planned for |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🔵 | Full CI matrix | ⚙️ CI | Every platform of CI | main, once the current block of streams lands | 1 to 2 h | Public repository | 🟢 | After batch4 lands |

Next cloud Mac day: not planned. Card KB-346 prepares it.

## 🧩 Platforms

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
| 🟢 | 🎮 GPU on Windows | 2026-11-27 07:30 | Azure AMD V620, desktop session | KB-350 `fe7b5cf3c` | ✅ Direct3D 11 177, Direct3D 12 178, WARP 177 and 178, none failed | |

## 📜 Batch Log

| Status | Date (UTC) | Session | Service | Platforms | Machine or devices | Duration | Cost | Jobs and results | Cache hit rate | Deleted and proven |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 🟢 | 2026-11-27 07:30 | windows-kb355-kb350 | 🔷 Azure | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 | 97 min over five starts | US$1.05 (estimate) | ✅ KB-355 MSVC with warnings as errors, 3417 tests, paths only of the new folder, ✅ KB-350 Direct3D 11 and 12 on the GPU and WARP with its regression, ✅ KB-356 fix five of five, while main never failed in five runs | 96.1 % | ✅ deallocated, IP and rule removed |
| 🟢 | 2026-11-27 05:30 | windows-varn-repin | 🔷 Azure | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 | 55 min over four starts | US$0.60 (estimate) | ✅ gpu-build with the Varn package cache, ✅ Direct3D 11, ✅ Direct3D 12, ✅ Vulkan, ✅ OpenGL, ✅ WARP | | ✅ deallocated, IP and rule removed |
| 🔴 | 2026-11-27 05:40 | batch5 | 🔶 AWS EC2 | 🐧 Linux | c8i.8xlarge spot | 10 min, reclaimed by AWS | US$0.11 (estimate) | ⏳ lost: physics3d and the animation3d node components, rerun on demand in batch5b | | ✅ |
| 🔴 | 2026-11-27 04:16 | batch4 | 🔶 AWS EC2 | 🐧 Linux | c8i.8xlarge spot | 65 min, reclaimed by AWS | US$0.73 (estimate) | ✅ physics3d commits 1 to 7 (clang and gcc), ❌ navigation3d: its new regression test fails on all 32 bakes (clang, gcc, asan), ⏳ lost: physics3d commit 8 and heads, navigation3d tsan, animation3d node components | | ✅ |
| 🟠 | 2026-11-27 05:10 | windows-kb055-2 | 🔷 Azure | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 | 25 min | US$0.27 (estimate) | ✅ GpuSurfaceTest.ReadsTheInputOfAFrameBehindOneFrameAtMost on Direct3D 11, Direct3D 12, WARP, Vulkan and OpenGL, ❌ 11 reference scenes per API (a cached object with another job's data path, KB-355), ❌ one OpenGL readback on the first run after boot (KB-356) | | ✅ deallocated, IP and rule removed |
| 🟢 | 2026-11-27 04:33 | windows-diagnosis | 🔷 Azure | 🪟 Windows | Standard_NG8ads_V620_v1 | 10 min | US$0.11 (estimate) | ✅ Diagnosis: the prebuilt OpenSSL package of Varn is not found by `find_package` under MSVC (KB-354), proven with a probe project | | ✅ deallocated, IP and rule removed |
| 🔴 | 2026-11-27 04:25 | windows-kb055 | 🔷 Azure | 🪟 Windows, 🎮 GPU | Standard_NG8ads_V620_v1 | 4 min, plus an earlier start whose bundle upload failed | US$0.30 (estimate) | ❌ KB-055 gpu-build: configure found no OpenSSL crypto library in the package cache, so no test ran | | ✅ deallocated, IP and rule removed |
| 🟠 | 2026-11-27 01:32 | batch3 | 🔶 AWS EC2 | 🐧 Linux, 🤖 Android, 🌐 Web, 🎮 GPU | c8i.8xlarge on-demand, because the spot quota was full | 168 min | US$4.20 (estimate) | ✅ KB-055, ✅ nodes3d, ✅ test-hygiene (short), ❌ animation3d head TSan (the navigation race, KB-352), ❌ old physics3d bundle (three defects, fixed for batch4) | 39 % | ✅ |
| 🟢 | 2026-11-27 00:00 | laptop-hygiene | 🏠 Own Machines | 🪟 Windows | Windows laptop | 56 min | US$0 | ✅ test-hygiene `6e09a22b7`, MSVC, 3330 of 3330 | | not needed |
| 🔴 | 2026-11-26 22:10 | batch2 | 🔶 AWS EC2 | 🐧 Linux, 🤖 Android | c8i.8xlarge spot | 200 min | US$2.20 (estimate) | ✅ test-hygiene, ✅ KB-051 OpenGL ES layers, ❌ physics3d, ❌ animation3d, ❌ nodes3d (a script path error), four attached runs lost when the machine was deleted under them | 10 % | ✅ |
| 🟢 | 2026-11-26 22:00 | laptop-kb055 | 🏠 Own Machines | 🪟 Windows, 🎮 GPU | Windows laptop | 56 min | US$0 | ✅ KB-055 MSVC 3339 of 3339, ✅ Direct3D 11 and 12 on three GPUs | | not needed |

## 🗄️ Persistent Resources

| Status | Service | Resource | Kind | Region | Why it is kept | Monthly cost |
| --- | --- | --- | --- | --- | --- | --- |
| ⚪ | 🔶 AWS EC2 | haylen-batch-linux-20261126-2037 and its snapshot | Prepared image | us-east-1 | The Linux batch machines start warm from it | US$1 |
| 🟢 | 🔷 Azure | haylen-batch-windows with its OS disk, NIC, NSG and VNet | Deallocated VM | eastus2 | The Windows GPU batch machine with its toolchains | US$5.90 (the disk) |
| ⚪ | Cache bucket | haylen-ci on Cloudflare R2 | Bucket | ENAM | The compiler cache and the dependency caches, objects expire after 30 days | Storage plus operations |

## 📏 Quotas

| Status | Service | Quota (family and region) | Current limit | Needed by the plan | Request filed on |
| --- | --- | --- | --- | --- | --- |
| 🟢 | 🔶 AWS EC2 | All Standard Spot Instance Requests, us-east-1 | 64 vCPU | 64 vCPU, two batch machines at once | 2026-11-27, granted |
| 🟢 | 🔶 AWS EC2 | Running On-Demand Standard instances, us-east-1 | 1848 vCPU | 32 vCPU | |
| 🟢 | 🔶 AWS EC2 | Mac dedicated hosts, mac-m4, us-east-1 | 1 host | 1 host, one day a month | 2026-11-26, granted |
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

## 📋 Rules

- **Layout:** The title `# ☁️ Cloud Tracker` and the 📊 Overview, the summary of each service, open the file. Nothing sits above the Overview. The other sections follow from most to least important: what runs now, open cleanup problems, the budget, what is planned, the latest result per platform, the batch history, then the lasting records (persistent resources, quotas, measured durations). These rules and the 🚦 Legend close the file. The title and the section titles stay exactly as written here.
- **Writer:** Only the coordinator writes this file.
- **Updates:** The file changes right after every change of state: a launch, a timer extension, an attach, a run or build that starts or ends, a spend check, a quota answer, a go from the person, a deletion or a leftover. Each update sets `Last updated`, the status emoji of every row it touches and the matching Overview row.
- **Times and costs:** Times are UTC. Each cost says `estimate` (price seen × time) until the billing data arrives, and then `billed`.
- **Sensitive data:** None. No credentials, keys, tokens, passwords, connection strings, IP addresses, host names, presigned URLs, device serials or UDIDs, personal or customer data, e-mail addresses, account or subscription ids, or private absolute paths. Devices are named by role, accounts by provider and credentials by variable name.
- **Unused rows:** Remove the rows and sections of the services and platforms the plan does not use. A section in use that becomes empty keeps its `None` row, with ⚪ where the table has a Status column.
- **Running Now:** "What it runs" names the lines of work and their commits. A row leaves when its machine, run or build ends, and its result goes to the 📜 Batch Log. Market is spot, on-demand or dedicated host. "Ends at" is the shutdown timer, or for a cloud Mac the planned host release (at least 24 hours after allocation).
- **Platforms:** Each row keeps only the latest result, so a broken platform shows even when nothing runs.
- **Budget:** The daily allowance is the share left ÷ the days left.
- **Batch Log:** One row per finished batch, run or build, from every service, own machines included (cost 0). The status covers the run and its cleanup: 🟡 cleanup not proven yet, 🟢 every test passed and cleanup is proven, 🟠 some tests failed but the run and cleanup are fine, 🔴 the run was lost or something was left. CI and Xcode Cloud runs leave nothing to delete, so their "Deleted and proven" is `not needed`. "Jobs and results" reads like `✅ physics (gcc), ❌ render (asan)`, and "Deleted and proven" holds ✅, ❌ or `not needed`.
- **Persistent Resources:** "Kind" is a prepared image, snapshot, deallocated VM, disk or bucket. A persistent VM shows ⚪ while deallocated and 🟢 while a batch uses it.

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

Services: 🔶 AWS EC2, 🔷 Azure, 🏠 Own Machines, 📱 AWS Device Farm, ⚙️ CI.
