# ☁️ Cloud tracker

This file tracks the rented machines of the batch line, the persistent cloud resources, the budget, the quotas and the batches, and the coordinator keeps it current at every launch, extension, finish and deletion. Times are UTC. Costs are estimates (price seen at launch × hours) until the billing data arrives, and then they are marked as billed. This file never holds a credential, a key, an IP address or a presigned URL.

Last updated: 2026-11-27 03:25 UTC

## Running now

### AWS

| Session | Machine type | Market | Region or zone | Started (UTC) | Shuts down at (UTC) | What it runs | Price per hour | Cost so far | State |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| batch3 | c8i.8xlarge (32 vCPU, 64 GB) | on-demand (the spot quota was full, see Quotas) | us-east-1d | 2026-11-27 01:32 | 2026-11-27 07:32 (timer) | animation3d (5 commits, sanitizers), KB-055 (2 commits, GPU tests, Android emulator), nodes3d (4 commits, heads), physics3d (old bundle, heads) | US$1.50 | about US$2.90 (estimate) | running, deleted when its jobs end |

### Azure

| Session | Machine type | Market | Region or zone | Started (UTC) | Shuts down at (UTC) | What it runs | Price per hour | Cost so far | State |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| None | | | | | | | | | |

### Google Cloud

| Session | Machine type | Market | Region or zone | Started (UTC) | Shuts down at (UTC) | What it runs | Price per hour | Cost so far | State |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| None | | | | | | | | | |

### Own machines

| Session | Machine (by role) | Lock held | Started (UTC) | What it runs | State |
| --- | --- | --- | --- | --- | --- |
| physics3d-queries | Daily Mac | only during its incremental test builds | 2026-11-27 | the 3D physics tests of the inner body and the rigid character | working |
| None | Windows laptop | no | | | idle, clean clone at main, 80 GB free |

Market is spot, on-demand, deallocated or own. "What it runs" names the lines of work and their commits.

## Persistent resources

| Provider | Resource | Kind | Region | Why it is kept | Monthly cost |
| --- | --- | --- | --- | --- | --- |
| AWS | haylen-batch-linux-20261126-2037 and its snapshot | Prepared image | us-east-1 | The Linux batch machines start warm from it | about US$1 |
| Azure | haylen-batch-windows with its OS disk, NIC, NSG and VNet | Deallocated VM | eastus2 | The Windows GPU batch machine (AMD V620) with its toolchains | about US$5.90 (disk) |
| Cloudflare R2 | haylen-ci | Bucket | ENAM | The compiler cache and the dependency caches, objects expire after 30 days | storage plus operations |

## Budget

| Item | Value |
| --- | --- |
| Monthly limit | US$120 for every cloud cost together |
| Split per provider | Monthly cloud Mac day about US$30, Azure about US$10, image storage about US$5, Linux machines about US$75 |
| Spend this month (per provider, with the date of the figure) | AWS US$23.85 billed through 2026-11-26, plus about US$2.90 of today's batch3 (estimate). Azure about US$4 (estimate: a few hours of the VM and the disk, since the usage listing returns nothing for this subscription) |
| Share left | about US$89 |
| Daily allowance (share left ÷ days left) | about US$22 a day for the 4 days left of November; the Linux share alone allows about US$12 a day |
| Projected month end | about US$40 to 45, with one or two batches a day |

## Quotas

| Provider | Quota (family and region) | Current limit | Pending request | Requested on |
| --- | --- | --- | --- | --- |
| AWS | All Standard Spot Instance Requests, us-east-1 | 32 vCPU (one c8i.8xlarge) | none, to ask: 64 | |
| AWS | Running On-Demand Standard instances, us-east-1 | 1848 vCPU | none | |
| AWS | Mac dedicated hosts, mac-m4, us-east-1 | 1 host | approved | 2026-11-26 |
| Azure | StandardNGADSV620v1Family, eastus2 | 8 vCPU (one VM) | none | |

## Measured durations

| Kind of job | Machine | Duration | Measured on |
| --- | --- | --- | --- |
| One commit with clang and GCC, `haylen test` from a fresh tree | c8i.8xlarge shared by 4 jobs | about 30 to 35 min | 2026-11-27 |
| A 2-commit stream with GPU tests, web SDK and Android emulator | c8i.8xlarge shared by 4 jobs | about 2 h | 2026-11-26 |
| Short hygiene run (head with clang, GCC and a diagnostic) | c8i.8xlarge shared | 43 min | 2026-11-27 |
| Cold `haylen test` with MSVC | Windows laptop | 56 min | 2026-11-26 |
| Fresh Mac tree with the remote cache | Daily Mac, 4 jobs | 21 to 25 min | 2026-11-26 |

## Batch log

| Date (UTC) | Machine | Duration | Cost | Jobs | Result per job | Cache hit rate | Deletion confirmed |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-11-26 22:10 | c8i.8xlarge spot, batch2 | 200 min | about US$2.20 (estimate) | hygiene, physics, nodes3d, animation, KB-051 GLES, KB-055 and late attaches | hygiene passed; physics, animation and nodes3d found defects; KB-051 GLES passed; four late attaches were lost when the machine was deleted | 10 % | Yes |
| 2026-11-26 | Windows laptop | about 1 h each | free | KB-055 and test-hygiene on MSVC | both passed | | not needed |

## Open cleanup items

| Found on | Provider | What is left | Why | Removed on |
| --- | --- | --- | --- | --- |
| None | | | | |

## Planned

| Batch | Machine | Jobs | Estimated duration | Estimated cost | Planned for |
| --- | --- | --- | --- | --- | --- |
| batch4 | c8i.8xlarge, spot once batch3 is fully terminated | physics3d (8 commits), plus the fixes that batch3 asks for | about 3 to 4 h | about US$2.50 spot, or about US$6 on-demand | after batch3 ends |

Next cloud Mac day: not planned. Card KB-346 prepares it.
