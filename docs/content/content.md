# Protected content

The content system stores the files of an app in a form that no archive tool opens and that no one changes unnoticed: the files are split into chunks, each chunk is compressed and encrypted on its own, the chunks live in immutable shards, and signed manifests with encrypted catalogs say which chunks make which file. The same format serves an app of a few megabytes and one of hundreds of gigabytes, since only the number of shards and chunks grows, and an update adds new shards next to the old ones instead of rewriting them. The engine reads such content through the same `io::Package` interface as a folder, so asset loading, `require` and `app.json` work the same on it.

This guide describes the formats, the cryptography, how content is built and updated, how the engine reads it, and what the protection guarantees. The code lives in `engine/src/content/`, in the namespace `haylen::content`, with the cryptography in `crypto/` and the wire formats in `format/`, the bootstrap that release builds compile in `engine/include/haylen/content/`, and the content tool in `engine/tools/content/`, and one implementation serves both the side that writes content and the side that reads it. The [loading guide](loading.md) explains the same system from the side of an app: autoloads, preload groups and scene loads, deliveries, caches, the packages of each store, updates and keys, with examples to copy.

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

The package of each manifest is a `content::HpakPackage`. Lookups, `exists`, `getFileSize` and listings read the decrypted catalog alone: a lookup is a binary search over the sorted paths, and a folder is one range of them. A shard opens the first time a chunk of it is read, which reads its header, checks it against the reference of the manifest and decrypts its index into memory bounded by the entry limit of the index, so mounting a release of any size costs nothing until it is used. A reader of a file, `content::HpakFileReader`, finds the chunks that cover a range in the catalog, reads and decrypts only those, and keeps the last chunk it decrypted, so sequential reads decrypt each chunk once. Several threads read at once. A shard opens once however many threads ask for it, and at most 16 shards of a manifest stay open: when another shard opens past that bound, the shard used least recently closes its file and drops its index once the reads that hold it end, and it opens again when a read needs it, so a release of any number of shards holds a bounded number of files and indexes. The records of a shard come through the one reader of its file, which reads one range at a time, and when the release files are a zip archive, all of them share the lock of the archive. Authenticating, decrypting and decoding a chunk hold no lock, so threads that read different chunks do that work at the same time.

Every chunk that reaches a caller passed four checks: the record header matches the index of its shard and the catalog in its IDs, its sizes and its codec, so a reader never relies on a size the record does not have, the encryption authenticates the record and its header, the encoded bytes decode to exactly the declared size, and the decoded bytes have the content ID the catalog names. The catalog is part of the signed manifest, so even someone who extracted the content key cannot change what an app reads without the signing key. Decrypted bytes that fail a check never leave the reader, the decrypted encoded bytes are wiped as soon as they are decoded, the chunk a reader keeps is wiped before the next chunk takes its place and when the reader ends, and a decrypted catalog is wiped when it ends, also when it fails its checks.

## Packages and readers

Every package, a folder, a zip archive, a package in memory or a protected release, reads files in ranges through the same interface.

| Method | What it does |
| --- | --- |
| `exists(path)` | Tells whether a file exists. |
| `getFileSize(path)` | Returns the size of a file without reading it, as a 64-bit number. |
| `openReader(path)` | Returns an `io::PackageReader`, whose `read(offset, target)` reads any range of the file and whose `getSize()` returns its size. Several threads may share a reader. |
| `readRange(path, offset, size)` | Reads one range of at most `Package::kMaxReadSize` bytes, fewer when the file ends first, and throws for a longer range, which streams through a reader instead. |
| `read(path)` | Reads a whole file of at most `Package::kMaxReadSize`, one gibibyte, and throws for a larger one, which streams through a reader instead. |
| `list(folder)` | Lists the files under a folder, sorted. A folder package that cannot read one of the folders throws instead of listing part of the files. |

A zip archive stays on disk and reads each entry when it is asked for. An entry stored without compression seeks directly, and a compressed entry decompresses forward, from its start again when a read goes back. Zip archives serve development, such as `haylen --dev app.zip` and the archives a browser editor hands to the web runtime, and protected releases use shards. Lua reads ranges with `assets.bytes(path, offset, count)` and sizes with `assets.fileSize(path)`, as the [assets reference](../lua-api/assets.md#assetsbytespath-offset-count) shows. The methods `openAssetReader`, `readAsset` and `readAssetRange` take paths relative to `content`.

## Streaming

A file that streams never exists whole in memory, decrypted or not, and never on disk: its reader reads the ranges a consumer asks for, and a protected release decrypts and decompresses only the chunks that cover them, so a file of any size costs a window of its bytes and the chunk the reader keeps. These consumers stream:

| Consumer | How it reads |
| --- | --- |
| Streamed sounds, with `stream = true` | Loading reads the header alone. Each voice decodes through a window of 64 KiB on the I/O pool into about a second of frames ahead of its cursor, a seek reads the range at the new cursor, and the thread that mixes never reads, as the [audio guide](../audio/audio.md#streamed-sounds) describes. |
| `assets.bytes(path, offset, count)`, `readRange`, `openReader` and `openAssetReader` | They read only the range asked for, which serves large data files of an app. |
| Asset types whose `assets::Manager::Type::streams` returns true for their options | Their decoder receives a reader of the file instead of its bytes and runs on the I/O pool, which is how a new kind of large media streams. |

Every other consumer reads a file whole, within `Package::kMaxReadSize`, because it needs all of the file at once.

| Consumer | Why it reads the whole file |
| --- | --- |
| Textures, and the images of atlases, particles, bitmap fonts and Tiled tilesets | The image decoder needs the whole compressed image, and the pixels it uploads take more memory than the file. |
| Fonts, of the `font` asset type and of the UI | The rasterizer and the shaper look up tables anywhere in the file for as long as the font lives. |
| Tiled maps and worlds, JSON, localization tables, shaders, SVG images, `app.json` and plugin manifests | The parser builds a document from the whole text, and the document takes more memory than the text, so reading the text in ranges would not lower the peak. |
| Lua modules | A module compiles or loads as one chunk. |
| `assets.bytes(path)` without a range | It returns the whole file as one Lua string, and the ranges of the same function serve large files. |
| Video streams | They carry frames that native code pushes, such as a camera or a decoder of a plugin, so no video file passes through a package. |

## Building content

The class `content::ReleaseBuilder` builds the release of an app package into a folder of its own: `app.hmanifest` with the shards of the app domain and `content.hmanifest` with the shards of the content domain. The app domain takes `app.json`, every file under `source` and, for every plugin that `app.json` lists, its `plugin.json` and the files under its `source`, and the content domain takes every file under `content`. Nothing else of the package folder takes part, so platform projects, notes, plugins the app does not list and the `.DS_Store` files of file managers never reach a release, and the builder decides the domain of every file by its folder, so no file of `source` can ever become content. The file `app.json` must be valid, as the engine reads it.

```cpp
content::ReleaseBuilder builder(keys, activeKeyId, signingKey, std::make_shared<const content::RecordCache>(cacheFolder));
content::ReleaseBuilder::Result result = builder.build(*io::Package::openDirectory(appFolder), releaseFolder, earlierReleaseFolder, {.profile = "apple", .appBuild = 1002003});
```

The output folder must not exist yet. A release that ships with an app serves its build alone, so both manifests name the build as their whole range of app builds, generation 1 and no channel. A build that names the folder of an earlier release of the app reuses its shards, as described below, and links the shards it keeps into the new folder, or copies them where the file system cannot link, after it checks each one against the size and the digest that the earlier manifest signs, so a damaged earlier release stops the build instead of reaching the new one.

The class `content::ContentBuilder` builds the shards and the plain catalog of one domain, and `Manifest::write` encrypts the catalog and signs the manifest. The builder takes the files in path order, whatever order they arrive in, and reads each in windows of at most one maximum chunk, so no file of the content domain is ever loaded whole. The app domain is the exception: `ReleaseBuilder` reads each of its files whole, at most `Package::kMaxReadSize`, to compile its Lua modules into bytecode, and builds the domain from those bytes in memory. It first splits every file into chunks and identifies them, and then stores the chunks it has not stored yet, reading their bytes again by range: a chunk whose content ID an earlier chunk of the build or of the earlier release given to `reuse` has already stored is referenced where it is instead of being stored again. So identical chunks are stored once, a renamed or copied file stores nothing new, and an update stores only the chunks that changed. A file whose bytes change between the two passes stops the build, since a chunk is stored only with the bytes its ID names.

An earlier shard stays in the release only while the files still use at least half of its record bytes, and the chunks they still use of a shard below that move into the new shards, so a long line of updates never carries more dead bytes than live ones, while an ordinary update leaves every earlier shard as it is. New chunks are sealed and written into new shards, and the shard list of the result keeps the earlier shards in their order, followed by the new ones, so an update never rewrites an old shard.

A new shard closes before a record would take it past the target size, one gibibyte by default, unless the record continues a file no larger than a sixteenth of the target, which stays whole in one shard. A shard also closes at its limit of records, and a single large file spans as many shards as it needs. The target is a parameter of the builder because a store with a stricter limit per file packs smaller shards of the same format. A shard grows under a temporary name and takes the name of its ID only when it is complete.

The same files and keys always build the same bytes. Chunk boundaries depend only on the bytes of each file, compression is deterministic, the nonce of every record derives from the record itself, and no time, path, user name or random value enters a shard or a manifest, so an unchanged chunk is byte for byte the same record in every build, a clean build of the same files repeats every file, and an update leaves the files of an unchanged domain byte for byte as they were, which keeps the patches of stores that compare files small. The result reports the new chunks, their plain and stored bytes, the chunks it took from the build cache, the chunks and bytes it reused, the earlier shards it dropped and the earlier shards whose files it lacked.

A build follows the release before it with `haylen-content build --previous <folder>`, whose folder may hold that release in any layout: the tool finds `app.hmanifest`, `content.hmanifest` and the shard files anywhere under it, so the folder of the last build that shipped serves as it is, such as the files of its Steam depots, where the shards of each pack sit in a folder of their own, an unpacked APK, an app bundle or a copy of a release folder. A shard can be kept only when its file is there, so the content of the shards that the folder lacks, such as the shards that only a content server holds, is built again, and the tool reports how many were missing. A folder without the two manifests, or with one of them twice, stops the build. A clean machine, such as continuous integration, has no release in its build folder, so it names the last build that shipped with `--previous-release` of `run`, `prepare`, `steam`, `dist` and `content build`, or `HAYLEN_PREVIOUS_RELEASE` of the CMake project of a C++ app, and its release keeps the shards of unchanged content byte for byte, which keeps the patches of stores small.

### Build cache

The class `content::RecordCache` keeps the sealed records of earlier builds in a folder, by the ID of the content key that sealed them, the encoder, which names the version of Zstandard and the compression profile, and the content ID of their chunk. A build that finds the record of a chunk there copies it into the new shard instead of compressing and encrypting the chunk again, which is most of the work of a build. Records hold only ciphertext, so the cache holds no plain content, and sealing is deterministic, so a cached record is the record a fresh build writes: a build with a warm cache, a cold one or none writes the same bytes. An entry serves only when it authenticates under the key and decodes to the chunk it names, so a damaged or foreign entry only costs the time to seal its chunk again, and deleting the cache never changes a build. The script `haylen.py` keeps the cache of each app in `build/apps/<app>-<hash>/content-cache/`.

## Lua bytecode

A release ships no Lua text. The class `content::LuaCompiler` compiles every Lua module of the app domain, the files ending in `.lua` under `source` and under `source` of every plugin, with the Lua that the engine itself runs, and the catalog marks each one as `LuaBytecode` under the path of its module, so `require('scenes.menu')`, the autoloads and `source/main.lua` resolve exactly as they do in development. A module that does not compile stops the build with the message of Lua, which names the file and the line. Compiling is deterministic, so an unchanged module keeps its chunk and its record from build to build, and a release loads its modules without parsing them.

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

The hierarchy stops at the purpose on purpose. A record is content-addressed: the same chunk keeps the same stored ID, the same nonce and the same bytes in every shard, patch and segment of the local store that holds it, which is what lets a patch reuse the records of earlier shards, the store keep a record once for every manifest that names it, and a collection move records between segments without decrypting them. A subkey for each shard, package or generation would encrypt the same chunk differently in each of them, so every move would rewrite it and every rebuild would change bytes that nothing changed. The context of a message therefore goes into its nonce and its authenticated data instead: a record binds its codec, profile, sizes and IDs, a shard index binds the header of its shard, and a catalog binds the envelope of its manifest, so no two messages share a nonce and none can move to another context unnoticed. Apps are isolated by their keys, since each app has its own, and the generation of a release lives only in its signed manifest, so a new generation changes no record that stayed the same.

Manifests and shard headers name keys by their IDs, never by their bytes. A `content::KeyRing` holds every key an app can decrypt with and finds them by ID, so content under a new key and installed content under the previous one coexist while keys rotate: each shard is opened with the key its header names, and each catalog with the key its manifest names. A key the ring lacks raises `UnknownKeyId` with the ID. The private signing key exists only where content is published, and an app carries only the verifying keys it trusts, each named by its ID, the BLAKE2b-256 digest of the label `haylen/hpak/v1/signing-key-id` followed by the public key.

### Keys in the app

An app opens the release it ships with the bootstrap that its release build compiles in, `content::Bootstrap` in `haylen/content/Bootstrap.hpp`: the identifier of the app, its build number and its content profile, which a release must match, the public keys that sign its manifests, and the `content::KeyProvider` of its content keys. The content tool writes the bootstrap of each app from its key folder as C++ source, `HaylenBootstrap.cpp`, which `haylen.py` compiles into the release build of every platform, and a static member of that source installs it with `Bootstrap::install` before `main`. So every app carries keys of its own, and no key sits in a resource, a manifest, `app.json`, `Info.plist`, the Android manifest, `BuildConfig` or a script.

The bootstrap holds the content keys in a `content::EmbeddedKeyProvider`: each key as two binary constants, a mask, BLAKE2b-256 keyed with the key over the label `haylen/bootstrap/v1/mask`, and the key combined byte by byte with the BLAKE2b-256 digest of the label `haylen/bootstrap/v1/pad`, the mask and the key ID. The binary therefore never holds a key, nor its hexadecimal or Base64 text, and the same keys always write the same bootstrap, so an unchanged app builds the same binary. This hides the keys from tools that search binaries for them, and it is no cryptographic protection: the app must hold its keys to run offline, and someone who studies its code can recover them, which the [security](#security) section accepts. The real protection of content is the authenticated encryption of its chunks and the signatures of its manifests.

The runtime opens the package that ships with an app through `ReleasePackage::openBundled`, which opens a folder that holds `app.hmanifest` as a protected release with the bootstrap of the app, and any other package as it is, which is what development builds ship. It asks the provider for every key it lists, derives the subkeys of each one into the `KeyRing` and wipes the key, and a key that the provider cannot give, or that does not match its ID, raises `KeyUnavailable`, which names the key ID and asks to install the app again. A release in a build without a bootstrap raises `KeyUnavailable` too, and never opens in any other way. A build with a bootstrap plays only its protected release: a folder without `app.hmanifest` in its place raises `MissingManifest`, the Windows and Linux hosts never fall back to `app.zip`, the player ignores a package that its command line names, and the web runtime refuses the files of the page that the editor API sends, so plain files that someone puts in place of a release never run. The format never depends on how the keys arrived, and no key reaches Lua.

Content published later may need a key that the app does not hold, such as an update under a rotated key that the service of an online game hands out only to players who signed in. A C++ app adds a provider of its own after the keys of its bootstrap with `Bootstrap::addKeyProvider` in `main`, before the engine starts, which joins both in a `content::CompositeKeyProvider`: the release opens with the keys of every provider, and a key it lacks goes to `KeyProvider::requestKey` of each provider in turn until one gives it. When the manifest of an update names a key that the ring lacks, the updater asks the provider for it once and reads the offer again with the new key in the ring, so the update installs as any other. A provider that does not give the key fails the check with `KeyUnavailable`, which names the manifest and the key in `manifestId` and `keyId`, while the app plays the content it holds. The ring that gains a key replaces the one before it, so content that is already open keeps the keys it opened with. A provider that obtains keys at run time keeps them, such as in the keychain or the keystore of the device, and lists them from the next start: content in the local store under a key that the provider does not hold when the app starts stays in the store, and the app plays the content before it until a start at which the provider holds its key.

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

## Release builds

The Release configuration of `haylen.py run` and `prepare` ships an app only as its protected release, with a binary that compiles in the bootstrap of the app, on every platform, as the [distribution guide](../tools/distribution.md#release-builds) describes, and inspects the built app for anything a release never holds: Lua text, the index of a development package, a file of the package as it is, symbol files and any form of a key of the app. The runtime opens the release that ships with the app with `ReleasePackage::openBundled`, from the folder `app` next to the executable on Windows and Linux, from `app` of the resources of the bundle on Apple platforms and from the flat folder `app` of the APK assets on Android, whose shards the APK keeps uncompressed so the runtime reads their ranges in place.

## The content tool

The tool `haylen-content` is the host tool that builds, verifies, inspects, compares and publishes releases with the format library of the engine, so no format or cryptography exists twice. The format has two sides: the engine library holds the reading side, the parsers, the verification, the decryption, the decoding, the downloads and the local store, while the library `haylen_content_writer` holds the writing side, the builders, the writers of shards, catalogs, manifests and channels, the sealing of records, the compressor, the chunker, the record cache, the Lua compiler and the signing key, which the tool, the tests and the benchmarks link and no app does, so an app carries no code that signs, seals or compresses content. The command `haylen.py engine --platform desktop` builds it into `build/artifacts/desktop/<os>-<arch>/` next to the desktop player, and `haylen.py` runs it, so a developer works with the `content` commands of `haylen.py`, and the release builds of the platforms run it on their own.

| Command of `haylen.py` | What it does |
| --- | --- |
| `content build <app> --platform <platform> [--previous-release <folder>]` | Builds the release of the app for the content profile of the platform into `build/apps/<app>-<hash>/release/<profile>/`, reusing the release before it and the build cache of the app. The release before it is the last one of the build folder, or the one under the folder of `--previous-release`. |
| `content verify <app> --platform <platform> [--release <folder>]` | Checks that the release folder holds the two manifests, the shards they name and nothing else, every shard against the size and digest its manifest signs, and every file by reading it whole, which authenticates every chunk. |
| `content inspect <app> --platform <platform> [--release <folder>] [--chunks]` | Lists each manifest with its generation, profile, range of app builds and shards, and each file with its delivery, size, chunks, stored bytes and shards, with the savings of deduplication, and every chunk with its offset and IDs with `--chunks`. |
| `content diff <app> <earlier> <later>` | Compares two releases domain by domain: the chunks they share, add and drop, the new shards, the files added, changed and removed, and the bytes an app that holds the earlier release downloads to reach the later one. |
| `content publish <app> --platform <platform> --output <tree> [--channel stable]` | Publishes the content domain as the next generation of an update channel in a tree of static files. |
| `content compact <app> <tree> [--keep 2]` | Deletes the manifests and packs of a publication tree that no channel reaches within its last generations. |
| `content keys <app> [--rotate]` | Shows the key folder of the app with the IDs of its keys and its public key, and with `--rotate` adds a content key. |

The command `content build` of an app with a content server also lays out the part of the release that ships with the app in `build/apps/<app>-<hash>/release/<profile>.shipped/`, which the release builds of the platforms ship, with `haylen-content stage <release> --keys <folder> --output <folder> [--deferred <folder>] [--packs <folder>] [--app <folder>] [--ship-deferred | --app-only]`, which reads the packs from the `app.json` of `--app`, puts the shards of each pack into a folder of the pack under `--packs`, ships the deferred shards too with `--ship-deferred` for an app without a server, and ships only the manifests and the app domain with `--app-only` for a web page, while the whole release stays for `content publish`, which puts the shards that stay behind on the server. The tool also writes the bootstrap of an app with `haylen-content bootstrap --keys <folder> --profile <profile> --build <number> --output <file>`, which the release builds of `haylen.py` run on their own, into a file that only its owner reads and that keeps its bytes when nothing changed.

```sh
haylen content build ~/apps/my-game --platform android
haylen content inspect ~/apps/my-game --platform android
haylen content diff ~/apps/my-game old-release build/apps/my-game-1a2b3c4d/release/android
haylen content publish ~/apps/my-game --platform android --channel stable --output ~/cdn/my-game
```

The profile of a release names the platform family it was made for: `apple` for every target of the Apple project, `android`, `windows` and `linux`. The build number of an app comes from its version, `1.2.3` being 1002003, as on Android.

### Key folders

The keys of an app live in a key folder of their own, outside every repository, project and build folder: `~/Library/Application Support/Haylen/keys/<identifier>/` on macOS, `%APPDATA%\Haylen\keys\<identifier>\` on Windows and `$XDG_CONFIG_HOME/haylen/keys/<identifier>/`, which defaults to `~/.config/haylen/keys/<identifier>/`, on Linux. The folder holds `keys.json`, which names the app, its content key IDs in the order they were added and its signing key ID, one `content-<key ID>.key` file of 32 bytes per content key, the last of which encrypts new content, and `signing.key`, the 32-byte seed of the Ed25519 signing key. The folder is readable by its owner alone, `0700` and `0600` for its files on macOS and Linux, and on Windows an access list that grants the user who created it alone and drops the entries that the folders above it pass down, and no command prints a key: they show IDs and the public key only.

The script `haylen.py` creates the keys of an app the first time a command needs them, from the random generator of the system, and warns that the folder needs a backup, since every later release of the app must be built with the same keys, or installed apps cannot read it. The command `content keys --rotate` adds a content key that encrypts the content of later builds while the earlier keys stay for content that installed apps already hold. Continuous integration receives a copy of the key folders as a secret and names their parent folder with `HAYLEN_KEYS_DIR`, and there `haylen.py` never creates keys, so a missing folder stops the build instead of producing a release no installed app can read:

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

The command `content publish` builds the content domain of the app as the next generation of the channel, reusing the packs of the current one, or for the first generation the shards of the release that ships with the app, which it publishes next to the new packs, so an installed app downloads only what changed, so it adds only the packs of new chunks and a new manifest, which names the previous one and serves the build of the app and every later build. Manifests and packs are immutable: a publish that would write other bytes under a name that exists stops there, and the packs it moved into place before stay, named by no channel, until a later publish uses them or `content compact` deletes them. A channel name has at most 64 lowercase letters, digits, dashes and dots, and any other name stops the publish before it reads or writes a file. New packs grow in `staging/` of the tree and move into place, the new manifest follows, the whole new generation verifies against the files in the tree, and only then does the channel pointer move, written under another name and renamed over the old one, so a publish that fails at any point leaves every channel at its earlier generation. Only the content domain is published, since the code of an app belongs to the build that ships it. The command `content compact` keeps the current generation of every channel and the ones before it up to `--keep`, two by default, and deletes the manifests and packs that none of them names, and a folder without `channels/` is refused as no publication tree.

## Downloads

Content that is not on the device arrives over HTTP or HTTPS through `content::HttpTransport`, which runs on the streaming client of Varn, so a response streams through the engine in pieces and is never held whole. It downloads the files of a publication tree or of a release, whole files such as channel pointers and manifests, and byte ranges of shards, and it checks every response before a byte of it reaches the engine.

| Check | What happens |
| --- | --- |
| Status | A whole file needs the status 200 and a range the status 206, and any other status fails with `UnexpectedHttpStatus` and the status in its details. |
| Range | A partial response must cover exactly the bytes asked for, as its `Content-Range` says, and a server that sends another range fails with `RangeUnsupported`. |
| Whole file instead of a range | A server that answers a range with the whole file fails with `RangeUnsupported` as soon as its head arrives, so a server without byte ranges never makes an app download a whole shard. |
| Length | A range delivers exactly its size, more fails with `RangeUnsupported`, and a body that ends early fails with `UpdateInterrupted`. |
| Validator | A download that resumes asks only for the bytes it lacks, with `If-Range` and the strong entity tag of the response that sent the bytes it holds, so the server sends the rest of the same file. When the file changed, the server answers with the whole file, and the range starts again from its first byte instead of mixing two files. A response without a strong entity tag never resumes. |
| Deadline | One deadline covers a whole request: 30 seconds plus the time the bytes take at 32 KiB per second. |
| Certificates | HTTPS verifies the certificate of the server, and a failure is `TlsFailed`. |

The class `content::RangePlanner` turns the records an app needs from one shard into few requests: records whose gap is at most 256 KiB share one request, which stays within 16 MiB, so many small records cost few requests, a small shard whose records are all needed arrives in one request, and a large shard is never fetched for a few of its records.

A server of protected content serves byte ranges of its files, sends `ETag` for them and leaves them uncompressed, since the shards are encrypted and do not compress. Every file of a publication tree is immutable except the channel pointers, so packs and manifests take long cache lifetimes, such as `Cache-Control: public, max-age=31536000, immutable`, and a channel pointer takes `Cache-Control: no-cache`. A server on another origin than a web page allows its requests with `Access-Control-Allow-Origin`, `Access-Control-Allow-Headers: Range, If-Range` and `Access-Control-Expose-Headers: Content-Range, Content-Length, ETag`.

## Delivery

Every asset of an app is `required` unless the `content` section of `app.json` says otherwise: its encrypted bytes are on the device before the app starts. The section names the server of the content and the assets that may arrive later, and an app without it needs no configuration at all.

```json
{
    "content": {
        "updates": {
            "baseUrl": "https://cdn.example.com/large-world/",
            "channel": "stable",
            "enabled": true
        },
        "delivery": [
            {"match": "world/asia/**", "mode": "onDemand"},
            {"match": "cinematics/4k/**", "mode": "prefetch"}
        ]
    }
}
```

| Setting | Default | What it decides |
| --- | --- | --- |
| `updates.baseUrl` | None, so the app has no server. | The root of the publication tree that `content publish` writes, ending with a slash: an `https` address, or an `http` address of this machine while a release is tested. It serves the updates of the content and the assets that download. Haylen cannot guess where an app hosts its content, so an app that downloads content names it. |
| `updates.channel` | `stable` | The channel whose updates the app takes, such as `beta` for testers, with at most 64 lowercase letters, digits, dashes and dots. |
| `updates.enabled` | `true` | Whether the app checks its channel when it starts and installs newer content in the background. With `false` the server still serves the assets that download, and the app checks and installs updates itself with [`haylen.content`](../lua-api/content.md). |
| `delivery` | None, so every asset is required. | Rules of a `match` pattern of paths relative to `content/`, where `**` stands for any folders, `*` for any name inside a folder and `?` for one character, and either a `mode` or a `pack`. The first rule that matches an asset decides its delivery or its pack, and an asset that no rule matches is required. Haylen cannot know which content an app needs only in some places, so the app says it. |
| `delivery[].group` | None, so the rule matches its `match` pattern. | The name of a preload group of the group manifest that `groups` names, instead of a `match` pattern. The rule takes the files of the group: the path of each file entry and every file under each folder entry, in the place of the rule, so a rule after it still matches what the group leaves. A group delivers the same files that [`assets.preload`](../lua-api/assets.md#preload-groups) loads, so the app names each set of files once. |
| `groups` | None. | The group manifest of the rules that name groups, by its path relative to `content/`, such as `groups.json`, the file that the app also hands to `assets.defineGroups`. |
| `delivery[].pack` | None, so the asset belongs to no pack. | The name of a pack that a store delivers on its own, such as an expansion that a desktop store installs on its own, with at most 64 lowercase letters, digits, dashes and dots. A pack never holds back the start of the app, never downloads in the background and never comes with an update: its assets are on the device once its store installs it. |

The settings mean the same on every platform, and only where the downloaded bytes live differs: a folder of the user on desktops, the caches of the app container on Apple platforms, which tvOS and iOS may empty when the device runs out of room, the files of the app on Android and the private file system of the origin on the web, whose quota the browser sets. An app built for a store keeps its server and its rules, and the packs of a store come from that store instead of the server. A page of a release that has no server still downloads, from the place its release stays on the site, so `updates.baseUrl` matters on the web only for updates.

A setting that is wrong stops the app before any Lua runs, on the error screen, with one of these messages, and the build tools refuse it the same way:

| Problem | Message |
| --- | --- |
| A key that the section does not know | `Unknown key "<key>" in the "content" section of "app.json".`, and the same for `content.updates` and for a rule of `content.delivery`. |
| A server that is not `https` and not this machine, or one without the ending slash | `The "content.updates.baseUrl" in "app.json" must be an "https" address that ends with a slash, such as "https://cdn.example.com/my-game/", or an "http" address of this machine, not "<value>".` |
| A channel with other characters or longer than 64 | `The "content.updates.channel" in "app.json" must have at most 64 lowercase letters, digits, dashes and dots, not "<value>".` |
| A `delivery` that is not a list of rules | `The "content.delivery" section of "app.json" must be a list of rules with "match" or "group" and a "mode" or a "pack".` |
| A rule with both or neither of `match` and `group` | `A rule of "content.delivery" in "app.json" needs either a "match" pattern or the name of a preload "group".` |
| A rule that names a group without `groups` | `A rule of "content.delivery" in "app.json" names the group "<name>", so "content.groups" must name the group manifest that defines it.` |
| A `groups` path that is empty, absolute or leaves `content/` | `The "content.groups" in "app.json" must name a file relative to the "content" folder, such as "groups.json", not "<value>".` |
| A group manifest that the package lacks | `The group manifest "content/<path>" that "content.groups" in "app.json" names is not in the package.`, and a manifest that does not follow the [format of groups](../lua-api/assets.md#preload-groups) fails with the message of `assets.defineGroups`. |
| A group that the manifest does not define | `A rule of "content.delivery" in "app.json" names the group "<name>", which "content/<path>" does not define.` |
| An entry of a group that leaves `content/` | `The group "<name>" of "content/<path>" has the path "<path>", which is not relative to the "content" folder.` |
| A rule with both or neither of `mode` and `pack` | `A rule of "content.delivery" in "app.json" needs either a "mode" or a "pack", since a store delivers a pack on its own.` |
| A pattern that is empty, absolute, or leaves `content/` | `A rule of "content.delivery" in "app.json" must match paths relative to the "content" folder, such as "world/asia/**", not "<value>".` |
| A mode other than the three | `The "mode" of a rule of "content.delivery" in "app.json" must be "required", "prefetch" or "onDemand", not "<value>".` |
| A pack name with other characters or longer than 64 | `The "pack" of a rule of "content.delivery" in "app.json" must have at most 64 lowercase letters, digits, dashes and dots, not "<value>".` |

| Delivery | When the encrypted bytes reach the device |
| --- | --- |
| `required` | Before the app starts. They ship with the app, or the engine downloads them while it shows the [update screen](#the-update-screen). |
| `prefetch` | In the background once the app runs, before anything asks for them, so a later load finds them on the device. |
| `onDemand` | Only when the app asks for them, through `assets.loadAsync`, a preload group, the `load` hook of a scene that preloads, or `content.prefetch`. |

A delivery decides only when encrypted bytes are on the device, never when an asset is decoded into memory: a required asset stays on the disk until the app loads it, and a prefetched asset costs storage and no memory until a load or a preload decodes it. A synchronous load never waits for the network, so `assets.load` of an asset that is not on the device fails with a message that names `loadAsync`, and every asynchronous path brings the asset first. Lua modules, `app.json` and plugins are always required and belong to the build of the app.

The release builder keeps the files of each delivery and of each pack in shards of their own, required ones first, so `haylen-content stage` lays out a release for a platform: the manifests, the shards of the app domain and the shards that the required files use go into the app, the shards that only prefetched and on-demand files use stay behind for the server, which `content publish` fills, and the shards of each pack go to the store that delivers the pack. Without a server the whole release ships with the app except its packs, and every other asset is on the device.

A pack is content that a store installs apart from the app, and the runtime reads its shards where the store puts them: on Windows, Linux and macOS from `packs/` next to the executable or the bundle, where the depots of [`haylen.py steam`](../tools/distribution.md#steam) install them, and from any folder that `ContentManager::addPackFolder`, or `content.addPackFolder` in Lua, names while the app runs. A store that delivers packs on request, such as the asset packs of Google Play, does so through a `content::PackSource` that its native plugin adds with `ContentManager::addPackSource`, or `content.addPackSource` in Lua: when the app loads an asset of a pack that the device lacks, the content manager asks each source in turn for the pack before the content server, and a source answers with the folder where the store installed the pack, which joins the release, or with nothing, which passes the pack on to the next source. So the code of the app loads the assets of packs the way it loads any other asset, and the store stays an implementation detail of its plugin, which needs the library of the store and so lives in a repository of its own, while the release build lays out the shards of each pack in `build/apps/<app>-<hash>/release/<profile>.packs/<pack>/` for the packaging of the store. An asset of a pack that is not installed is unavailable, and an asynchronous load of it fails with `storeAssetUnavailable`, unless the app has a server, which `content publish` fills with the shards of the packs too, so an app that sells a pack keeps it off a public server. Since the shards of a pack hold nothing else, a new build of the app keeps the shards of every unchanged pack byte for byte, and a store that compares files delivers only what changed.

## Updates

The class `content::Updater` updates the content of an installed app from the channel of a publication tree as one transaction, and `content::Downloader` brings the records it needs.

1. A check downloads the signed pointer of the channel, under an address that changes with every check so no cache answers it, verifies its signature, and checks that it names the app and the channel. A generation older than the one the app accepted last, or the same generation with another manifest, raises `ManifestRollbackRejected`, so a server that replays an old pointer never takes the app back. The same generation as the content the app plays offers nothing.
2. The check downloads the manifest the pointer names, verifies it, checks it against the build of the app, which raises `ManifestIncompatible` for content of another app, profile or range of builds and keeps the content as it is, decrypts its catalog and finds the chunks of its required files that the device lacks, in the shard files the app ships with or in the store. A chunk that the app already holds, because the new content keeps a shard or holds the same bytes, is never downloaded again.
3. An install first checks that the device has room: the records it downloads, their journal entries, the indexes of their shards, the manifest, the records that download at once, and a margin of 64 MiB or a twentieth of the download, whichever is larger. A device without that room raises `InsufficientStorage` with the bytes needed and available before a byte is written, and the content stays as it was.
4. The install downloads the header and the encrypted index of every shard it needs records from, once, which the store keeps, and then the records alone, in ranges that `RangePlanner` joins, with four requests at once. Every record is checked against the signed catalog and the index of its shard, authenticated and decoded on the task pool before the store keeps it, so nothing unverified enters the store.
5. Only once every record is verified does the install write the manifest and a new state that names the new content as active and the content before it as previous, after the records and the manifest are on the disk. The new content plays from the next start of the app.

A request for a record that another request is downloading waits for the same download, and cancelling one request stops only the downloads that no other request needs. Requests are foreground work, the required content, the assets that the app loads or brings and updates, or background work, the prefetched content, which holds at most one of the four requests that run at once and starts only while no foreground request waits, so what the app asks for never queues behind content that it may need later. A download that stops keeps the first bytes of the record it was receiving in the staging folder of the store, with the validator of the response, and the next attempt asks only for the rest of that record, in a range that starts with it. Before its first record arrives, an install writes the manifest it brings into the store and names it as pending in the state, so the collection at the next start keeps its records. A crash, a lost network or a full disk at any point leaves the content the app plays as it was, the records already verified serve the next attempt, and an attempt after a restart downloads only what is still missing.

When the app starts, the release opens the content that the state names as active, verifies its manifest again, checks it against the build and decrypts its catalog. Content that fails any of these, such as a manifest that the device damaged or lost, falls back to the previous content and then to the content that ships with the app, and the release records why in `getRecovery`, so damaged downloads never keep an app from starting. A store that the device refuses to open leaves the app with the content it ships. A collection keeps the records of the content that ships with the app, of the active and the previous content and of the content that an install is bringing, and removes the rest.

| Platform | Folder of the store |
| --- | --- |
| macOS, iOS and iPadOS, Mac Catalyst | `Application Support/<identifier>-content`, left out of backups. |
| tvOS | `Caches/<identifier>-content`, which the system may clear, after which downloads bring the content back. |
| Android | `no_backup/<identifier>-content` of the data folder of the app, which Android leaves out of its backups. |
| Windows | `%LOCALAPPDATA%\<identifier>\Content`, which never roams with the profile. |
| Linux | `$XDG_CACHE_HOME/<identifier>/content`, by default `~/.cache/<identifier>/content`. |

## The content manager

The class `content::ContentManager` of `haylen/content/ContentManager.hpp` drives the content of the running app, and `core::Engine::getContent` returns it. The module [`haylen.content`](../lua-api/content.md) gives Lua the same API, and C++ apps use it directly.

```cpp
#include "haylen/content/ContentManager.hpp"

haylen::content::ContentManager& content = engine.getContent();
if (!content.isLocal("world/asia/maps/tokyo.tmj")) {
    content.acquire({"world/asia/maps/tokyo.tmj"}, {}, [](std::optional<haylen::content::Error> error) {
        // The map is on the device unless the error says why it is not.
    });
}
```

| Method | What it does |
| --- | --- |
| `isProtected()` | Whether the app plays a protected release. |
| `getGeneration()`, `getChannel()` | The generation of the content that plays and the channel of the app. |
| `getStatus()`, `getProgress()`, `getError()` | What the content does now, the bytes of its download and the last failure with its details. |
| `isLocal(path)` | Whether every byte of an asset is on the device. |
| `acquire(paths, progress, done)` | Brings assets onto the device without loading them. |
| `addPackFolder(folder)` | Reads the shards of a pack from the folder a store installed it into while the app runs. |
| `addPackSource(source)` | Adds a `content::PackSource` that `acquire` asks for the packs that assets need, before the content server, and returns the `core::Connection` that removes it. |
| `check(done)`, `download(progress, done)`, `apply()` | Asks the channel for newer content, installs it for the next start, and starts the app again on it. |
| `cancel()`, `retry()` | Stops what runs, and tries the required content again after a failure while the app waits. |
| `getStatistics()` | Counters for debugging, without any secret: the files and the store, the reads of the content with the shards open, the chunks and bytes decoded, the time they took and the reads that run, and the downloads with their bytes, rate, waiting records, running transfers and the bytes an update reuses, which the Content section of the debug overlay shows. |

When the app starts, the manager looks on a worker thread for required content that the device lacks, while the engine holds the start of the app with `Engine::holdStart`. Content that ships with the app is there, so the app starts in the next frames. Content that is not there, as on a web page or on a device that cleared its downloads, downloads from the server first, and the app starts once all of it is verified. When downloaded content cannot come back, the manager plays the content that ships with the app instead. Once the app runs, the manager removes from the store what no retained content uses, downloads the prefetched content, and checks the channel when updates are on, which installs newer content in the background for the next start. The network and the state of the app steer that work: the required content that failed while the app waits for it downloads again as soon as the network comes back, and so do a prefetch and an automatic check or install that failed, the prefetch stops while the app is in the background and goes on once it is active, and content that the app brings, prefetches or installs downloads only when the device has room for it with the margin the store keeps free, 64 MiB or a twentieth of the download, so a device without room fails the request with `InsufficientStorage` and skips the prefetch. It publishes `contentUpdateAvailable`, `contentUpdateReady`, `contentUpdateFailed` and `contentRecovered` on the event bus, the last one once the app has started, so the listeners of its main module receive it.

### The update screen

While the app waits for required content that downloads, the engine draws the update screen in the background color, with the name and the logo of the [splash screen](../tools/distribution.md#splash-screens) of `app.json`, the logo only once it is on the device. It shows the bytes received and the size of the download, and after a failure the reason in plain words, such as an offline device or a device without room and the room it needs, with a button that tries again and, except on iOS, iPadOS and tvOS, which never let apps close themselves, a button that quits. A mouse, a finger, Enter, the south button of a gamepad and the select button of a TV remote press the focused button, and the arrow keys and the directional pad move the focus. No screen shows while the manager only finds the content on the device, and apps never draw their own screen to download their required content.

### Development

In development the app plays its folder, without downloads or updates, and the delivery of `app.json` still applies: an asset that downloads later in a release stays off the device until `assets.loadAsync`, a preload group or `content.prefetch` brings it, which takes one frame, so `assets.load` of such an asset fails in development exactly as in a release, and a mistake shows long before a release.

## The web

A web page ships the same protected release as every other platform, built for the `web` profile, and nothing of the package as it is: no `app.zip`, no `content/` and no Lua text. The site of a release build holds the runtime of the app for WebGPU and for WebGL2, linked with the bootstrap of the app, and the release in `app/`:

```text
index.html, loader.js, loader.css, app.js, config.json, splash.png
webgpu/haylen.js, webgpu/haylen.wasm, webgpu/haylen-audio-worklet.js, webgpu/haylen-content-store.js
webgl2/...                       The same files for WebGL2.
app/app.hmanifest
app/content.hmanifest
app/<shard>.hpak                 Every shard of the release.
```

The page downloads the runtime, the two manifests and the shards of the app domain, which hold `app.json` and the Lua bytecode and are small, before the app starts. The content never downloads whole: the content manager reads the encrypted index of each shard it needs and then only the records of the assets that the app needs, by byte range from `app/`, so a page of a game of many gigabytes starts after a download of a few megabytes. The required content comes down while the update screen shows, the prefetched content follows in the background, and assets that download on demand come when the app loads them, exactly as on the other platforms. WebGPU and WebGL2 share the same release and the same store, since only the runtime differs.

### The content store of the browser

What the page downloads stays in the content store of the app in the private file system of the origin, a folder per app under `haylen-content`, which a worker of `haylen-content-store.js` writes through synchronous access handles, each change flushed before the next one, so the records always reach the disk before the state that names them. The next visit reads the small files of the store, its state, journal, manifests and shard indexes, before the app starts, and reads in a record only when the app needs it, so a store of many gigabytes never enters memory: records in memory stay within 256 MiB beyond the required content, which stays for the whole visit, and the ones read least recently go. The saves of the app live apart, in IndexedDB, so clearing the content store never touches them.

The page asks the browser to keep its storage with `navigator.storage.persist()`, and the room an update needs comes from `navigator.storage.estimate()`, so an update that does not fit fails with `insufficientStorage` before it writes a byte. A browser that still refuses a change past the quota, such as one that lowered the quota after its estimate, fails the change with `quotaExceeded`: the worker keeps no later change, so the files of the browser stay at the last state they hold whole, and every later change of that visit fails with the same code. Browsers decide how much an origin keeps and may clear it, which no page can prevent: content that the browser cleared downloads again the next time the app needs it. A browser without the private file system, or a second tab of the same app, which finds the store held by the first, keeps the downloads of that visit in memory and says so in the console. Such a page reports no room, so it installs no update, which fails with `insufficientStorage`, and it downloads on-demand content into memory for the visit.

### Serving a release

A server of the site serves the files of `app/` by byte range, with `206` and `Content-Range`, a strong `ETag` and `If-Range`, and without content encoding for `.hpak` files, which are encrypted and do not compress. The command `haylen run --platform web --config Release` serves the site that way on this machine. A content delivery network does it by default, and the files take long cache lifetimes, since a shard and a manifest never change under their name:

```text
app/*.hpak, app/*.hmanifest      Cache-Control: public, max-age=31536000, immutable
index.html, loader.js, config.json, */haylen.js, */haylen.wasm   Cache-Control: no-cache
```

A page whose release or content server lives on another origin needs `Access-Control-Allow-Origin` for the page, `Access-Control-Allow-Headers: Range, If-Range` and `Access-Control-Expose-Headers: Content-Range, Content-Length, ETag` on that server.

## Local store

What downloads bring stays on the device in the local content store of the app, `content::LocalStore`, in a folder of its own apart from the saves of the app, so clearing it never touches them. It never holds a plain byte: records stay sealed as they arrived, shard indexes stay encrypted, manifests stay signed, and no file of the store names a path.

```text
state-a, state-b               The state, written into each slot in turn.
journal-<n>                    The journal of the records: where each one lies, by stored ID.
segments/<n>.hseg              Records one after another, as the shards held them.
manifests/<id>.hmanifest       The signed manifests the app retains.
indexes/<shard>.hidx           The header and the encrypted index of shards whose records came by download.
staging/<shard>-<offset>.hpart The first bytes of a record whose download stopped.
```

Records go into segments that close near 256 MiB, so a store of hundreds of gigabytes holds a few hundred files and never rewrites a large one. Each record that a segment receives gets an entry of 96 bytes in the journal: its stored ID, the ID of the key that sealed it, its segment, its offset and its size, with a checksum. The journal only grows, so a crash can tear only its last entry, which the store finds and drops when it opens. A segment that the platform evicted or that lost its end takes the records of its entries with it, and downloads bring them back.

The state names the content manifest the app plays, the one before it and the one that an install is bringing, the generation the app accepted last from its channel and the journal of the records. It goes into two slots in turn, `state-a` and `state-b`, each with a sequence number and a BLAKE2b-256 checksum, so a write that a crash or a loss of power tears leaves the other slot whole, and the store opens with the whole slot of the higher sequence. A new state becomes durable only after every record it relies on, since the store flushes the segments and the journal to the disk before it writes the slot. A new build of the app starts again from the content it ships with, keeping the records and the generation it accepted, and a store of another app opens empty.

A collection keeps the records that the manifests the app retains use, removes the others, and moves the live records of a segment less than half alive into the newest segment, when the device has room for them and the margin of the store, and otherwise removes only the segments without a live record. It writes a new journal of the live records, switches to it with a new state and only then deletes what it replaced, so a collection that a crash interrupts leaves the store as it was, and one that runs when the app stops ends between two records and leaves the store as it was too. A reader never meets a segment that a collection removes, since the removal waits for the reads that run.

Every record of the store passes the same checks on every read as a record of a shard: its header must match the signed catalog, and its encryption must authenticate. A record that fails leaves the store with `CacheCorrupt`, and the next download replaces it.

## Performance

The command `haylen bench --suite content` measures the work of protected content on this machine and prints the largest single allocation of each measurement, so a reader that starts to hold whole files shows at once. These are the results of one Release build on an Apple M5 Pro with the machine loaded by other work, so they are a ceiling, and absolute values vary from machine to machine.

| Measurement | Result | Largest allocation |
| --- | --- | --- |
| Building the release of 64 MiB of incompressible data, 16 MiB of text and 2000 small files | 135 MiB/s | 4.0 MiB |
| Opening the release, with both signatures checked and both catalogs decrypted | 1.2 ms | 0.3 MiB |
| Reading an incompressible file in order | 355 MiB/s | 2.3 MiB |
| Reading a compressible file in order | 1011 MiB/s | 4.0 MiB |
| 2000 reads of 4 KiB at random offsets of one file | 6.5 s | 2.4 MiB |
| Finding 2000 files in the catalog | 0.6 ms | 0 MiB |
| Planning the download ranges of 100 thousand records | 0.7 ms | 2.0 MiB |
| Adding 256 records of 256 KiB to the local store | 171 MiB/s | 0.25 MiB |

The largest allocation of every read stays at the size of one chunk, at most 4 MiB, whatever the size of the file or the release. A read authenticates, decrypts and decompresses the whole chunk that holds its first byte, so reading a file in order costs each chunk once, while a small read at a random place costs a whole chunk, about 3 ms for a chunk of 1 MiB of incompressible data. Formats that read small pieces at random places, such as a database of many records, keep each piece in a file of its own or read their file in order.

## Limits

Every parser treats its bytes as hostile: it checks every offset and size against the bytes it has before it uses them, checks counts before it multiplies them, and allocates only what those checks allow.

| Limit | Value |
| --- | --- |
| Chunk | 4 MiB plain, and never more encoded. |
| Records of a shard | 2097152, so an index takes at most 128 MiB. |
| Shards of a manifest | 1048576. |
| Open shards of a manifest | 16. |
| Catalog | 512 MiB. |
| Envelope of a manifest | 128 MiB. |
| Path | 1024 bytes. |
| Profile and channel | 64 bytes. |
| Whole read of a file | 1 GiB, beyond which a file streams through a reader. |

## Errors

Failures of protected content raise `content::Error` of `haylen/content/Error.hpp`, a `std::runtime_error` whose code tells a damaged install from a missing key, a network that is down or a rejected update. Its details hold the HTTP status of a failed request and the message of the platform, each empty when it does not apply. The method `isRetryable` tells whether trying again may succeed without a new build of the app: a network that comes back, a server error, a timeout, a status 408 or 429, room that the user frees on the device, or a damaged record of the store that a download replaces. Messages and details name files, shards and IDs, and never keys or decrypted bytes.

| Code | Cause |
| --- | --- |
| `UnsupportedFormat` | The bytes are not the structure they should be, such as a file that is no shard. |
| `UnsupportedVersion` | A structure has a version, or uses a format version, that this build does not read. |
| `CorruptHeader` | A shard header is malformed, a shard file is smaller than its header or has another size than its manifest names, the file holds another shard, or the content tool finds a shard file that is not the one its manifest signs. |
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
| `KeyUnavailable` | The provider of the app cannot give a key it lists, the app ships a release and its build holds no bootstrap, or no provider gives the key of an update, which `keyId` names. |
| `MissingChunk` | A shard does not hold a chunk its catalog places in it. |
| `MissingShard` | The file of a shard that a manifest names is missing. |
| `MissingManifest` | A publication tree lacks a manifest that a channel names, or a release build finds plain files or nothing in the place of the protected release it plays. |
| `LuaBytecodeIncompatible` | The app manifest holds Lua bytecode of another ABI than the Lua of the running build loads. |
| `NetworkOffline` | The server could not be reached. |
| `NetworkTimeout` | A request passed its deadline. |
| `NetworkCancelled` | A download was cancelled. |
| `NetworkFailed` | A request failed in another way, such as an address the transport refuses or a redirect it does not follow. |
| `TlsFailed` | The secure connection failed, such as a certificate that does not verify. |
| `RangeUnsupported` | The server does not serve byte ranges as protected content needs. |
| `UnexpectedHttpStatus` | The server answered with a status other than the file or the range. |
| `UpdateInterrupted` | A download ended before all of its bytes arrived. |
| `DiskFull` | The device has no room for what the store writes. |
| `QuotaExceeded` | The browser refused a change of the store past the quota of the site, after which the worker keeps no later change, so the files of the browser stay at the last state they hold whole and the page plays the content it held before. |
| `PermissionDenied` | The device does not let the store write its folder. |
| `CacheCorrupt` | A file of the local store is damaged, such as a record that fails its checks, which the store removes so a download replaces it. |
| `InsufficientStorage` | The device lacks the room an update needs, which the error names with the room that is free. |
| `StoreAssetUnavailable` | Content is not on the device and the app names no server to download it from, such as an asset of a [pack](#delivery) that its store did not install. |

## Checks that need devices

The tests of the engine run every part of the content system on the desktop, with a local server and folders that behave like the stores of the platforms, and the releases run on the desktop player, the Linux and Windows apps, the iOS and tvOS simulators, Mac Catalyst, the Android emulator and the browsers. A few behaviors belong to devices and stores that no simulator reproduces, so a project checks them on real hardware before it ships:

| Check | Why it needs a device |
| --- | --- |
| A tvOS or iOS device that empties the caches of the app when it runs out of room, and the downloads that follow | Simulators never empty the caches on their own, so the tests remove the files of the store themselves instead. |
| Android TV with a remote, through the update screen and the downloads | The emulator images of Android TV run the same code as phones, and only a TV checks the remote and the overscan of a real screen. |
| The packs of a store, such as an asset pack of the App Store or an expansion that a desktop store installs | A store installs them only for an app it distributes, so the tests point the engine at folders that stand in for them. |
| The quota of a browser on a phone and the eviction of the storage of a site | Desktop browsers rarely evict, and phones decide by the room they have left. |

## Security

The content system protects against the practical threats to the files of a client app: browsing an app bundle or an APK, extracting it with archive tools, copying images, data and scripts, crawling named assets, and changing content without the signing key, whether by accident or on purpose. Shards hold only ciphertext and opaque IDs, catalogs that name paths are encrypted, every chunk is authenticated, and the chain from the signed manifest to the content ID of every chunk means that content that decrypts still cannot be changed without the signing key.

It does not protect against a skilled attacker who controls the device: one who inspects memory, captures the GPU, instruments the code that decrypts, or recovers the content key from the app, which must hold it to run offline. That is the boundary of every app that runs content on the client, and the content key of each app is its own, so recovering it exposes only that app.
