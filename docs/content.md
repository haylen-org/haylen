# Protected content

The content system stores the files of an app in a form that no archive tool opens and that no one changes unnoticed: the files are split into chunks, each chunk is compressed and encrypted on its own, the chunks live in immutable shards, and signed manifests with encrypted catalogs say which chunks make which file. The same format serves an app of a few megabytes and one of hundreds of gigabytes, since only the number of shards and chunks grows, and an update adds new shards next to the old ones instead of rewriting them. The engine reads such content through the same `io::Package` interface as a folder, so asset loading, `require` and `app.json` work the same on it.

This guide describes the formats, the cryptography, how content is built and updated, how the engine reads it, and what the protection guarantees. The code lives in `engine/src/content/`, in the namespace `haylen::content`, with the cryptography in `crypto/` and the wire formats in `format/`, and one implementation serves both the side that writes content and the side that reads it.

## The parts of a release

| Part | What it is |
| --- | --- |
| Chunk | A piece of one file, up to 4 MiB, identified by its content ID, the BLAKE2b-256 digest of its bytes. |
| Record | A chunk as it is stored: compressed when that saves space, encrypted and authenticated with XChaCha20-Poly1305, and identified by its stored ID. |
| Shard | An immutable `.hpak` file of records with an encrypted index, named after its shard ID, such as `9f2c…e1.hpak`. |
| Catalog | The encrypted table of the files of a domain: their paths, sizes and chunks, and the shard of every chunk. It is the only place that names paths. |
| Manifest | A signed `.hmanifest` file: a public envelope that names the app, the domain, the generation, the compatibility range, the keys and the shards, followed by the encrypted catalog. |
| Channel descriptor | A signed pointer from an update channel to its current content manifest, with the generation of that manifest. |

A release has two domains, each with its own manifest. The app domain, `app.hmanifest`, holds `app.json`, the Lua modules under `source` and the manifest and Lua modules of every plugin under `plugins`, which belong to one build of the app, with every Lua module compiled into bytecode. The content domain, `content.hmanifest`, holds the assets under `content`, which updates may replace. Reading a catalog checks that every file belongs to the domain of its manifest, so a content manifest can never carry code, and that every file of the app domain is required.

## Reading a release

```cpp
#include "content/ReleasePackage.hpp"

std::shared_ptr<const io::Package> files = io::Package::openDirectory(releaseFolder);
std::unique_ptr<io::Package> package = content::ReleasePackage::open(files, keys, trustedKeys, compatibility);
const std::vector<std::uint8_t> map = package->readAsset("maps/island.tmj");
```

`ReleasePackage::open` reads `app.hmanifest` and `content.hmanifest` from a package of release files, verifies the signature of each with the trusted verifying keys, checks each against the `Compatibility` of the running build, decrypts the catalogs with the `KeyRing` of the app, and returns one package that shows `app.json`, `source` and `plugins` from the app domain and `content` from the content domain, exactly as a folder would. Its shards are found in the same package of files by their names. A manifest of the wrong domain raises `CorruptManifest`, and nothing in the files is trusted before the signature that covers it is verified.

The package of each manifest is a `content::HpakPackage`. Lookups, `exists`, `getFileSize` and listings read the decrypted catalog alone: a lookup is a binary search over the sorted paths, and a folder is one range of them. A shard opens the first time a chunk of it is read, which reads its header, checks it against the reference of the manifest and decrypts its index into memory bounded by the entry limit of the index, so mounting a release of any size costs nothing until it is used. A reader of a file, `content::HpakFileReader`, finds the chunks that cover a range in the catalog, reads and decrypts only those, and keeps the last chunk it decrypted, so sequential reads decrypt each chunk once. Several threads read at once: each shard opens once under a lock of its own, and the reads of records and their decryption hold no lock that other files share.

Every chunk that reaches a caller passed four checks: the record header matches the index of its shard and the catalog, the encryption authenticates the record and its header, the encoded bytes decode to exactly the declared size, and the decoded bytes have the content ID the catalog names. The catalog is part of the signed manifest, so even someone who extracted the content key cannot change what an app reads without the signing key. Decrypted bytes that fail a check never leave the reader, the decrypted encoded bytes are wiped as soon as they are decoded, and the chunk a reader keeps is wiped when the reader ends.

## Packages and readers

Every package, a folder, a zip archive, a package in memory or a protected release, reads files in ranges through the same interface.

| Method | What it does |
| --- | --- |
| `exists(path)` | Tells whether a file exists. |
| `getFileSize(path)` | Returns the size of a file without reading it, as a 64-bit number. |
| `openReader(path)` | Returns an `io::PackageReader`, whose `read(offset, target)` reads any range of the file and whose `getSize()` returns its size. Several threads may share a reader. |
| `readRange(path, offset, size)` | Reads one range, fewer bytes when the file ends first. |
| `read(path)` | Reads a whole file of at most `Package::kMaxReadSize`, one gibibyte, and throws for a larger one, which streams through a reader instead. |
| `list(folder)` | Lists the files under a folder, sorted. |

A zip archive stays on disk and reads each entry when it is asked for. An entry stored without compression seeks directly, and a compressed entry decompresses forward, from its start again when a read goes back. Zip archives serve development, such as `haylen --dev app.zip` and the archives a browser editor hands to the web runtime, and protected releases use shards. Lua reads ranges with `assets.bytes(path, offset, count)` and sizes with `assets.fileSize(path)`, as the [assets reference](lua-api/assets.md#assetsbytespath-offset-count) shows.

## Building content

`content::ReleaseBuilder` builds the release of an app package into a folder of its own: `app.hmanifest` with the shards of the app domain and `content.hmanifest` with the shards of the content domain. The app domain takes `app.json`, every file under `source` and, for every plugin that `app.json` lists, its `plugin.json` and the files under its `source`, and the content domain takes every file under `content`. Nothing else of the package folder takes part, so platform projects, notes, plugins the app does not list and the `.DS_Store` files of file managers never reach a release, and the builder decides the domain of every file by its folder, so no file of `source` can ever become content. `app.json` must be valid, as the engine reads it.

```cpp
content::ReleaseBuilder builder(keys, activeKeyId, signingKey, std::make_shared<const content::RecordCache>(cacheFolder));
content::ReleaseBuilder::Result result = builder.build(*io::Package::openDirectory(appFolder), releaseFolder, earlierReleaseFolder, {.profile = "apple", .appBuild = 1002003});
```

The output folder must not exist yet. A release that ships with an app serves its build alone, so both manifests name the build as their whole range of app builds, generation 1 and no channel. A build that names the folder of an earlier release of the app reuses its shards, as described below, and links the shards it keeps into the new folder, or copies them where the file system cannot link, after it checks each one against the size and the digest that the earlier manifest signs, so a damaged earlier release stops the build instead of reaching the new one.

`content::ContentBuilder` builds the shards and the plain catalog of one domain, and `Manifest::write` encrypts the catalog and signs the manifest. The builder takes the files in path order, whatever order they arrive in, and reads each in windows of at most one maximum chunk, so no file is ever loaded whole. It first splits every file into chunks and identifies them, and then stores the chunks it has not stored yet, reading their bytes again by range: a chunk whose content ID an earlier chunk of the build or of the earlier release given to `reuse` has already stored is referenced where it is instead of being stored again. So identical chunks are stored once, a renamed or copied file stores nothing new, and an update stores only the chunks that changed. A file whose bytes change between the two passes stops the build, since a chunk is stored only with the bytes its ID names.

An earlier shard stays in the release only while the files still use at least half of its record bytes, and the chunks they still use of a shard below that move into the new shards, so a long line of updates never carries more dead bytes than live ones, while an ordinary update leaves every earlier shard as it is. New chunks are sealed and written into new shards, and the shard list of the result keeps the earlier shards in their order, followed by the new ones, so an update never rewrites an old shard.

A new shard closes before a record would take it past the target size, one gibibyte by default, unless the record continues a file no larger than a sixteenth of the target, which stays whole in one shard. A shard also closes at its limit of records, and a single large file spans as many shards as it needs. The target is a parameter of the builder because a store with a stricter limit per file packs smaller shards of the same format. A shard grows under a temporary name and takes the name of its ID only when it is complete.

The same files and keys always build the same bytes. Chunk boundaries depend only on the bytes of each file, compression is deterministic, the nonce of every record derives from the record itself, and no time, path, user name or random value enters a shard or a manifest, so an unchanged chunk is byte for byte the same record in every build, a clean build of the same files repeats every file, and an update leaves the files of an unchanged domain byte for byte as they were, which keeps the patches of stores that compare files small. The result reports the new chunks, their plain and stored bytes, the chunks it took from the build cache, the chunks and bytes it reused and the earlier shards it dropped.

### Build cache

`content::RecordCache` keeps the sealed records of earlier builds in a folder, by the ID of the content key that sealed them, the encoder, which names the version of Zstandard and the compression profile, and the content ID of their chunk. A build that finds the record of a chunk there copies it into the new shard instead of compressing and encrypting the chunk again, which is most of the work of a build. Records hold only ciphertext, so the cache holds no plain content, and sealing is deterministic, so a cached record is the record a fresh build writes: a build with a warm cache, a cold one or none writes the same bytes. An entry serves only when it authenticates under the key and decodes to the chunk it names, so a damaged or foreign entry only costs the time to seal its chunk again, and deleting the cache never changes a build. `haylen.py` keeps the cache of each app in `build/apps/<app>-<hash>/content-cache/`.

## Lua bytecode

A release ships no Lua text. `content::LuaCompiler` compiles every Lua module of the app domain, the files ending in `.lua` under `source` and under `source` of every plugin, with the Lua that the engine itself runs, and the catalog marks each one as `LuaBytecode` under the path of its module, so `require('scenes.menu')`, the autoloads and `source/main.lua` resolve exactly as they do in development. A module that does not compile stops the build with the message of Lua, which names the file and the line. Compiling is deterministic, so an unchanged module keeps its chunk and its record from build to build, and a release loads its modules without parsing them.

The bytecode keeps all of its debug information: the chunk name is the package path of the module, such as `source/scenes/menu.lua`, and the lines and the names of local variables stay, so an error in a release reads exactly as in development, with its module, its line and messages such as `attempt to index a nil value (local 'value')`. No folder of the machine that built the release enters a chunk, and the bytecode is encrypted in its shard like every other file. The error screen of a release shows the message and the stack without the excerpt of source lines, since a release keeps no source text.

Bytecode of the wrong format can crash the virtual machine, so its format is part of the release. `LuaCompiler::getAbi` names it from the header that Lua writes into every chunk, such as `lua-5.5-f0-i4-x4-l8-n8-le`: the version of Lua, its format, the sizes of an `int`, an instruction, a Lua integer and a Lua number, and the byte order. The app manifest records it, and a build whose Lua differs refuses the release with `LuaBytecodeIncompatible` before it reads a module. Every target of the engine, the 32-bit ARM of Android TV and the web included, runs a little-endian Lua with those sizes, so the bytecode the content tool compiles on the build machine loads everywhere.

Bytecode never checks the values it builds, so app code still loads chunks only as text: `load`, `loadfile` and `dofile` refuse bytecode and `string.dump` does not exist. The module loader of the engine is the one place that loads bytecode, and only a file that `io::Package::isLuaBytecode` reports, which only the package of a protected release does, for the files its signed and encrypted catalog marks. A bytecode file in a folder, a zip archive or a package in memory loads as text and fails, so bytecode that does not come from a release built with the keys of the app never runs. The loader wipes the bytes of a module once Lua read them.

## Chunking

The chunker, `content::Chunker`, implements `fastcdc-v1`, a content-defined chunker with these fixed parameters, which content built with version 1 depends on.

| Parameter | Value |
| --- | --- |
| Minimum chunk | 256 KiB |
| Target chunk | 1 MiB |
| Maximum chunk | 4 MiB |
| Hash | A gear hash: `hash = (hash << 1) + gear[byte]`, from zero at the minimum size of each chunk. |
| Gear table | 256 values of SplitMix64 from the seed `0x6861796c656e7631`. |
| Cut before the target | When the top 22 bits of the hash are zero. |
| Cut after the target | When the top 18 bits of the hash are zero. |
| Cut at the maximum | Always. |

A file up to the target size stays one chunk, and chunks never cross from one file into another. An insertion or a deletion changes only the chunks around it, and every chunk after them finds its old boundaries again, keeps its bytes and its ID, and is reused by an update. The tests pin the cuts of a known input, as an independent implementation computes them.

## Compression

Each chunk is compressed on its own, so any chunk decodes without the others. A codec and its profile pin every setting that shapes the stored bytes, and both are part of the stored ID and of the authenticated record header.

| Codec | Profile | Meaning |
| --- | --- | --- |
| 0, none | 0 | The chunk as it is. |
| 1, Zstandard | 1 | Level 12 in one frame that records its content size, without a checksum, which the encryption makes redundant, and without a dictionary. |

A chunk is stored as it is when compression saves less than a thirty-second of it, as with media that is compressed already. Decoding needs exactly the declared size: a frame that would decode to more or fewer bytes, or that asks for a window larger than any chunk needs, is rejected, which stops decompression bombs.

## Cryptography

The engine uses [Monocypher](https://monocypher.org), an audited library without dependencies that builds the same on every target, the web included, and implements no primitive itself.

| Use | Algorithm |
| --- | --- |
| Records, shard indexes and catalogs | XChaCha20-Poly1305 with a 256-bit key, a 192-bit nonce and a 128-bit tag. |
| Content IDs, stored IDs, shard IDs, manifest IDs and key IDs | BLAKE2b-256. |
| Key and nonce derivation | Keyed BLAKE2b. |
| Manifests and channel descriptors | Ed25519 signatures. |

### Keys

The content of an app is encrypted under its content key, 32 random bytes of its own, so no two apps share a key. The engine never uses the key itself: `content::ContentKey` derives a subkey for each purpose, keeps only the subkeys and the key ID, and wipes them when it ends. A label below is its length in one byte followed by its ASCII characters.

| Value | Derivation |
| --- | --- |
| Key ID | BLAKE2b-256 keyed with the content key, over the label `haylen/hpak/v1/key-id`. |
| Subkey of a purpose | BLAKE2b-256 keyed with the content key, over the label of the purpose followed by the key ID. |
| Purposes | `haylen/hpak/v1/chunk` for records, `haylen/hpak/v1/shard-index` for shard indexes, `haylen/hpak/v1/catalog` for catalogs and `haylen/hpak/v1/nonce` for nonces. |

Manifests and shard headers name keys by their IDs, never by their bytes. A `content::KeyRing` holds every key an app can decrypt with and finds them by ID, so content under a new key and installed content under the previous one coexist while keys rotate: each shard is opened with the key its header names, and each catalog with the key its manifest names. A key the ring lacks raises `UnknownKeyId` with the ID. The private signing key exists only where content is published, and an app carries only the verifying keys it trusts, each named by its ID, the BLAKE2b-256 digest of the label `haylen/hpak/v1/signing-key-id` followed by the public key.

### Keys in the app

An app opens the release it ships with the bootstrap that its release build compiles in, `content::Bootstrap` in `haylen/content/Bootstrap.hpp`: the identifier of the app, its build number and its content profile, which a release must match, the public keys that sign its manifests, and the `content::KeyProvider` of its content keys. The content tool writes the bootstrap of each app from its key folder as C++ source, `HaylenBootstrap.cpp`, which `haylen.py` compiles into the release build of every platform, and a static member of that source installs it with `Bootstrap::install` before `main`. So every app carries keys of its own, and no key sits in a resource, a manifest, `app.json`, `Info.plist`, the Android manifest, `BuildConfig` or a script.

The bootstrap holds the content keys in an `content::EmbeddedKeyProvider`: each key as two binary constants, a mask, BLAKE2b-256 keyed with the key over the label `haylen/bootstrap/v1/mask`, and the key combined byte by byte with the BLAKE2b-256 digest of the label `haylen/bootstrap/v1/pad`, the mask and the key ID. The binary therefore never holds a key, nor its hexadecimal or Base64 text, and the same keys always write the same bootstrap, so an unchanged app builds the same binary. This hides the keys from tools that search binaries for them, and it is no cryptographic protection: the app must hold its keys to run offline, and someone who studies its code can recover them, which the [security](#security) section accepts. The real protection of content is the authenticated encryption of its chunks and the signatures of its manifests.

The runtime opens the package that ships with an app through `ReleasePackage::openBundled`, which opens a folder that holds `app.hmanifest` as a protected release with the bootstrap of the app, and any other package as it is, which is what development builds ship. It asks the provider for every key it lists, derives the subkeys of each one into the `KeyRing` and wipes the key, and a key that the provider cannot give, or that does not match its ID, raises `KeyUnavailable`, which names the key ID and asks to install the app again. A release in a build without a bootstrap raises `KeyUnavailable` too, and never opens in any other way. A C++ app may install a bootstrap of its own with a provider of its own, such as one that fetches keys from the service of an online game, and the format never depends on how the keys arrived. No key reaches Lua.

### Nonces

Each nonce derives from everything that identifies the message it encrypts, so the same message always encrypts to the same bytes, which keeps unchanged records identical across builds, while two different messages never share a nonce, since that would take a collision of BLAKE2b. Every nonce is BLAKE2b-192 keyed with the nonce subkey, over the label of the purpose of its message followed by the identity of the message.

| Message | Identity |
| --- | --- |
| Record | The first 88 bytes of its header: its codec, profile, sizes, content ID and stored ID, which digest the encoded bytes. |
| Shard index | The BLAKE2b-256 digest of the first 104 bytes of the shard header followed by the plain index. |
| Catalog | The BLAKE2b-256 digest of the context of its manifest, the envelope before its catalog fields, followed by the plain catalog. |

The generation of a release takes part in no key and in no nonce of a record, so a new manifest never changes the bytes of an unchanged record. A change of these derivations is a new version of the format.

### Identifiers

| Identifier | Digest |
| --- | --- |
| Content ID | BLAKE2b-256 of the plain chunk. |
| Stored ID | BLAKE2b-256 of the codec byte, the profile byte and the encoded chunk. |
| Shard ID | BLAKE2b-256 of the label `haylen/hpak/v1/shard-id`, the key ID, and the stored ID and the 64-bit record offset of every record in the order of the file. |
| Shard file digest | BLAKE2b-256 of the whole shard file. |
| Application | BLAKE2b-256 of the label `haylen/hpak/v1/application` followed by the identifier of the app. |
| Manifest ID | BLAKE2b-256 of the label `haylen/hpak/v1/manifest-id` followed by the signed envelope. |

## Formats

Every field is an unsigned integer in little-endian order or a run of bytes, at the offset the tables give, and no structure is the memory layout of a C++ type. Offsets and sizes are 64 bits wide everywhere. Reserved bytes are zero, and a reader rejects them otherwise. A string is a 16-bit length followed by its bytes.

### HPAK shard

A shard is a header of 160 bytes, the records one after another, and the encrypted index, which ends the file.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | Magic: `HPAK` followed by the bytes `0D 0A 1A 0A`. |
| 8 | 2 | Format version, 1. |
| 10 | 2 | Header size, 160. |
| 12 | 4 | Flags, zero. |
| 16 | 32 | Shard ID. |
| 48 | 32 | Key ID. |
| 80 | 8 | Offset of the index. |
| 88 | 8 | Size of the index, 64 bytes per entry. |
| 96 | 8 | Number of entries. |
| 104 | 24 | Nonce of the index. |
| 128 | 16 | Reserved. |
| 144 | 16 | Tag of the index. |

The first 144 bytes of the header are the additional data of the encryption of the index, so the index authenticates the header too.

### Record

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Magic: `HPKR`. |
| 4 | 2 | Record version, 1. |
| 6 | 1 | Codec. |
| 7 | 1 | Profile. |
| 8 | 8 | Plain size, from 1 byte to 4 MiB. |
| 16 | 8 | Encoded size, the plain size when stored as it is and smaller when compressed. |
| 24 | 32 | Content ID. |
| 56 | 32 | Stored ID. |
| 88 | 24 | Nonce. |
| 112 | 16 | Tag. |
| 128 | Encoded size | Ciphertext of the encoded chunk. |

The first 112 bytes of the header are the additional data of its encryption, so no field changes without failing authentication.

### Shard index

The plain index is one entry of 64 bytes per record, sorted by stored ID without repeats, and holds no path.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 32 | Stored ID. |
| 32 | 8 | Offset of the record. |
| 40 | 8 | Encoded size. |
| 48 | 8 | Plain size. |
| 56 | 1 | Codec. |
| 57 | 1 | Profile. |
| 58 | 6 | Reserved. |

Every record lies between the end of the header and the start of the index, and no two records overlap. A shard holds at most 2097152 records, which bounds its index at 128 MiB.

### Catalog

The plain catalog is a header of 64 bytes followed by four tables, in this order, which fill it exactly.

| Offset | Size | Header field |
| --- | --- | --- |
| 0 | 4 | Magic: `HCAT`. |
| 4 | 2 | Catalog version, 1. |
| 6 | 2 | Reserved. |
| 8 | 8 | Number of files. |
| 16 | 8 | Number of chunks. |
| 24 | 8 | Number of parts. |
| 32 | 8 | Size of the string table. |
| 40 | 24 | Reserved. |

| Offset | Size | File, 40 bytes, sorted by path without repeats |
| --- | --- | --- |
| 0 | 8 | Offset of the path in the string table. |
| 8 | 4 | Length of the path, at most 1024 bytes. |
| 12 | 1 | Kind: 0 for a file, 1 for Lua bytecode. |
| 13 | 1 | Delivery: 0 required, 1 prefetch, 2 on demand. |
| 14 | 2 | Reserved. |
| 16 | 8 | Size. |
| 24 | 8 | First part. |
| 32 | 8 | Number of parts. |

| Offset | Size | Chunk, 88 bytes, sorted by stored ID without repeats |
| --- | --- | --- |
| 0 | 32 | Stored ID. |
| 32 | 32 | Content ID. |
| 64 | 8 | Plain size. |
| 72 | 8 | Encoded size. |
| 80 | 4 | Shard, its position in the shard list of the manifest. |
| 84 | 1 | Codec. |
| 85 | 1 | Profile. |
| 86 | 2 | Reserved. |

| Offset | Size | Part, 16 bytes |
| --- | --- | --- |
| 0 | 8 | Chunk, its position in the chunk table. |
| 8 | 8 | Offset in the file where the chunk starts. |

The string table holds the paths of the files one after another. A path is a normalized package path, such as `content/maps/island.tmj`, in valid UTF-8 without control characters or backslashes. The parts of a file follow each other without gaps and end exactly at its size, and an empty file has none. A chunk that several files or one file several times use is listed once, so a file of 300 GiB made of one repeated chunk takes 16 bytes per part.

The delivery of a file says when its encrypted bytes must be on the device: before the app starts, in the background after it starts, or only when the app asks for the file. It never says when a file is decoded into memory, and every file of the app domain is required. Only the app domain holds Lua bytecode, and only as Lua modules of the app or of a plugin, files ending in `.lua` under `source` or `plugins/<id>/source`, in a manifest that names the ABI of the bytecode.

### Manifest

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | Magic: `HMAN` followed by the bytes `0D 0A 1A 0A`. |
| 8 | 2 | Manifest version, 1. |
| 10 | 2 | Reserved. |
| 12 | 4 | Size of the signed envelope, counted from offset 0. |
| 16 | 1 | Domain: 0 for the app, 1 for the content. |
| 17 | 1 | Reserved. |
| 18 | 2 | HPAK format version, 1. |
| 20 | 2 | Catalog version, 1. |
| 22 | 2 | Reserved. |
| 24 | 32 | Application digest. |
| 56 | 32 | Signing key ID. |
| 88 | 32 | Content key ID. |
| 120 | 8 | Generation. |
| 128 | 32 | ID of the previous manifest, zero for none. |
| 160 | 8 | Minimum app build. |
| 168 | 8 | Maximum app build. |
| 176 | String | Profile, the platform and store the content was made for, of at most 64 lowercase letters, digits, dashes and dots, and never empty. |
| | String | Channel, with the characters of a profile, empty for none. |
| | String | Lua ABI, the format of the Lua bytecode of the catalog, with the characters of a profile, empty for a catalog without bytecode. |
| | 8 | Number of shards. |
| | 72 each | Shards: shard ID, file size and file digest. |
| | 8 | Size of the encrypted catalog. |
| | 32 | Digest of the encrypted catalog. |
| | 24 | Nonce of the catalog. |
| | 16 | Tag of the catalog. |
| Envelope size | 64 | Ed25519 signature of the envelope. |
| Envelope size + 64 | Catalog size | Encrypted catalog, which ends the file. |

The envelope before the catalog fields, its context, is the additional data of the encryption of the catalog. Reading a manifest trusts only the size of the envelope before the signature, and only to find it: the signing key ID at its fixed offset picks the trusted key, the signature is verified over the whole envelope, and only then is any other field read. The catalog is accepted when its digest matches the one the envelope signs and its encryption authenticates. No field of the envelope names a path.

### Channel descriptor

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | Magic: `HCHN` followed by the bytes `0D 0A 1A 0A`. |
| 8 | 2 | Descriptor version, 1. |
| 10 | 2 | Reserved. |
| 12 | 32 | Application digest. |
| 44 | 32 | Signing key ID. |
| 76 | 8 | Generation. |
| 84 | 32 | ID of the content manifest. |
| 116 | String | Channel. |
| End − 64 | 64 | Ed25519 signature of everything before it. |

## Compatibility and generations

A `content::Compatibility` holds what the running build accepts: the digest of its app, its build number, its profile and the ABI of its Lua. A manifest of another app, made for another profile, or whose range of app builds leaves the build out raises `ManifestIncompatible` with the reason, and a manifest whose Lua bytecode has another ABI raises `LuaBytecodeIncompatible`. The versions of the shard and catalog formats are checked when a manifest is read, and unknown versions raise `UnsupportedVersion`.

The publisher of a channel raises the generation with every release. `ChannelDescriptor::offersUpdate` tells whether a descriptor offers content newer than the generation and manifest the app accepted last, and raises `ManifestRollbackRejected` for an older generation, or for the same generation with another manifest, so a server that replays an old descriptor cannot roll an app back. A rollback is published as a new generation that names the earlier content. `ChannelDescriptor::describes` tells whether a manifest is the content manifest that a descriptor names for its app, channel and generation.

## The content tool

`haylen-content` is the host tool that builds, verifies, inspects, compares and publishes releases with the format library of the engine, so no format or cryptography exists twice. `haylen.py engine --platform desktop` builds it into `build/artifacts/desktop/<os>-<arch>/` next to the desktop player, and `haylen.py` runs it, so a developer works with the `content` commands of `haylen.py`, and the release builds of the platforms run it on their own.

| Command of `haylen.py` | What it does |
| --- | --- |
| `content build <app> --platform <platform>` | Builds the release of the app for the content profile of the platform into `build/apps/<app>-<hash>/release/<profile>/`, reusing the release before it and the build cache of the app. |
| `content verify <app> --platform <platform> [--release <folder>]` | Checks that the release folder holds the two manifests, the shards they name and nothing else, every shard against the size and digest its manifest signs, and every file by reading it whole, which authenticates every chunk. |
| `content inspect <app> --platform <platform> [--release <folder>] [--chunks]` | Lists each manifest with its generation, profile, range of app builds and shards, and each file with its delivery, size, chunks, stored bytes and shards, with the savings of deduplication, and every chunk with its offset and IDs with `--chunks`. |
| `content diff <app> <earlier> <later>` | Compares two releases domain by domain: the chunks they share, add and drop, the new shards, the files added, changed and removed, and the bytes an app that holds the earlier release downloads to reach the later one. |
| `content publish <app> --platform <platform> --output <tree> [--channel stable]` | Publishes the content domain as the next generation of an update channel in a tree of static files. |
| `content compact <app> <tree> [--keep 2]` | Deletes the manifests and packs of a publication tree that no channel reaches within its last generations. |
| `content keys <app> [--rotate]` | Shows the key folder of the app with the IDs of its keys and its public key, and with `--rotate` adds a content key. |

The tool also writes the bootstrap of an app with `haylen-content bootstrap --keys <folder> --profile <profile> --build <number> --output <file>`, which the release builds of `haylen.py` run on their own, into a file that only its owner reads and that keeps its bytes when nothing changed.

```sh
python3 haylen.py content build ~/apps/my-game --platform android
python3 haylen.py content inspect ~/apps/my-game --platform android
python3 haylen.py content diff ~/apps/my-game old-release build/apps/my-game-1a2b3c4d/release/android
python3 haylen.py content publish ~/apps/my-game --platform android --channel stable --output ~/cdn/my-game
```

The profile of a release names the platform family it was made for: `apple` for every target of the Apple project, `android`, `windows` and `linux`. The build number of an app comes from its version, `1.2.3` being 1002003, as on Android.

### Key folders

The keys of an app live in a key folder of their own, outside every repository, project and build folder: `~/Library/Application Support/Haylen/keys/<identifier>/` on macOS, `%APPDATA%\Haylen\keys\<identifier>\` on Windows and `$XDG_CONFIG_HOME/haylen/keys/<identifier>/`, which defaults to `~/.config/haylen/keys/<identifier>/`, on Linux. The folder holds `keys.json`, which names the app, its content key IDs in the order they were added and its signing key ID, one `content-<key ID>.key` file of 32 bytes per content key, the last of which encrypts new content, and `signing.key`, the 32-byte seed of the Ed25519 signing key. The folder is readable by its owner alone (`0700`, and `0600` for its files), and no command prints a key: they show IDs and the public key only.

`haylen.py` creates the keys of an app the first time a command needs them, from the random generator of the system, and warns that the folder needs a backup, since every later release of the app must be built with the same keys, or installed apps cannot read it. `content keys --rotate` adds a content key that encrypts the content of later builds while the earlier keys stay for content that installed apps already hold. Continuous integration receives a copy of the key folders as a secret and names their parent folder with `HAYLEN_KEYS_DIR`, and there `haylen.py` never creates keys, so a missing folder stops the build instead of producing a release no installed app can read:

```yaml
env:
  HAYLEN_KEYS_DIR: ${{ runner.temp }}/haylen-keys
steps:
  - run: |
      mkdir -p "$HAYLEN_KEYS_DIR"
      echo "${{ secrets.HAYLEN_CONTENT_KEYS }}" | base64 --decode | tar -x -C "$HAYLEN_KEYS_DIR"
```

The command `tar -c -C ~/Library/Application\ Support/Haylen/keys com.example.mygame | base64` prints the text of that secret.

### Publication trees

A publication tree is a folder of static files that any web server or content delivery network serves, every one named by an opaque ID, so no name tells what a file holds:

```text
channels/<channel>.hchannel    The signed pointer of a channel to its current content manifest, with its generation.
manifests/<id>.hmanifest       Every published content manifest, named by its ID.
packs/<ab>/<id>.hpak           Every published shard, named by its ID under the first two digits of the ID.
```

`content publish` builds the content domain of the app as the next generation of the channel, reusing the packs of the current one, so it adds only the packs of new chunks and a new manifest, which names the previous one and serves the build of the app and every later build. Manifests and packs are immutable: a publish that would write other bytes under a name that exists stops before it changes anything. New packs grow in `staging/` of the tree and move into place, the new manifest follows, the whole new generation verifies against the files in the tree, and only then does the channel pointer move, written under another name and renamed over the old one, so a publish that fails at any point leaves every channel at its earlier generation. Only the content domain is published, since the code of an app belongs to the build that ships it. `content compact` keeps the current generation of every channel and the ones before it up to `--keep`, two by default, and deletes the manifests and packs that none of them names.

## Limits

Every parser treats its bytes as hostile: it checks every offset and size against the bytes it has before it uses them, checks counts before it multiplies them, and allocates only what those checks allow.

| Limit | Value |
| --- | --- |
| Chunk | 4 MiB plain, and never more encoded. |
| Records of a shard | 2097152, so an index takes at most 128 MiB. |
| Shards of a manifest | 1048576. |
| Catalog | 512 MiB. |
| Envelope of a manifest | 128 MiB. |
| Path | 1024 bytes. |
| Profile and channel | 64 bytes. |
| Whole read of a file | 1 GiB, beyond which a file streams through a reader. |

## Errors

Failures of protected content raise `content::Error`, a `std::runtime_error` whose code tells a damaged install from a missing key or a rejected update. Messages name files, shards and IDs, and never keys or decrypted bytes.

| Code | Cause |
| --- | --- |
| `UnsupportedFormat` | The bytes are not the structure they should be, such as a file that is no shard. |
| `UnsupportedVersion` | A structure has a version, or uses a format version, that this build does not read. |
| `CorruptHeader` | A shard header is malformed, its file has another size than its manifest names, or the file holds another shard. |
| `CorruptIndex` | A shard index fails authentication or its checks. |
| `CorruptChunk` | A record header is malformed, does not match its index and catalog, or its encoded bytes do not decode to the declared size. |
| `CorruptCatalog` | A catalog fails its checks or holds a file its domain does not allow. |
| `CorruptManifest` | A manifest or a channel descriptor is malformed, or a manifest belongs to the other domain. |
| `InvalidOffset` | A shard index places a record outside the data of its shard or over another record. |
| `ChunkAuthenticationFailed` | A record fails authentication, so its shard is damaged or was changed. |
| `ChunkHashMismatch` | A record decodes to bytes other than its content ID names. |
| `CatalogAuthenticationFailed` | A catalog does not match the digest its manifest signs or fails authentication. |
| `ManifestSignatureInvalid` | A manifest or a descriptor is signed by a key the app does not trust, or its signature is not valid. |
| `ManifestIncompatible` | A manifest is for another app, profile or range of builds. |
| `ManifestRollbackRejected` | A descriptor offers an older generation, or the same generation with another manifest. |
| `UnknownKeyId` | The app has no content key with the ID a shard or a manifest names. |
| `KeyUnavailable` | The provider of the app cannot give a key it lists, or the app ships a release and its build holds no bootstrap. |
| `MissingChunk` | A shard does not hold a chunk its catalog places in it. |
| `MissingShard` | The file of a shard that a manifest names is missing. |
| `LuaBytecodeIncompatible` | The app manifest holds Lua bytecode of another ABI than the Lua of the running build loads. |

## Security

The content system protects against the practical threats to the files of a client app: browsing an app bundle or an APK, extracting it with archive tools, copying images, data and scripts, crawling named assets, and changing content without the signing key, whether by accident or on purpose. Shards hold only ciphertext and opaque IDs, catalogs that name paths are encrypted, every chunk is authenticated, and the chain from the signed manifest to the content ID of every chunk means that content that decrypts still cannot be changed without the signing key.

It does not protect against a skilled attacker who controls the device: one who inspects memory, captures the GPU, instruments the code that decrypts, or recovers the content key from the app, which must hold it to run offline. That is the boundary of every app that runs content on the client, and the content key of each app is its own, so recovering it exposes only that app.
