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

A release has two domains, each with its own manifest. The app domain, `app.hmanifest`, holds `app.json`, the Lua modules under `source` and the manifest and Lua modules of every plugin under `plugins`, which belong to one build of the app. The content domain, `content.hmanifest`, holds the assets under `content`, which updates may replace. Reading a catalog checks that every file belongs to the domain of its manifest, so a content manifest can never carry code, and that every file of the app domain is required.

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

`content::ContentBuilder` builds the shards and the plain catalog of one domain from the files of a source package, and `Manifest::write` encrypts the catalog and signs the manifest.

```cpp
content::ContentBuilder builder(outputFolder, contentKey);
builder.reuse(previousCatalog, previousManifest.getEnvelope().shards);
content::ContentBuilder::Result result = builder.build(sourcePackage, {{.path = "content/maps/island.tmj"}, {.path = "content/music/theme.ogg"}});
std::vector<std::uint8_t> manifest = content::Manifest::write(envelope, result.catalog, *contentKey, signingKey);
```

The builder takes the files in path order, whatever order they arrive in, and reads each in windows of at most one maximum chunk, so no file is ever loaded whole. It splits a file into chunks, and a chunk whose content ID an earlier chunk of the build or of the earlier release given to `reuse` has already stored is referenced where it is instead of being stored again. So identical chunks are stored once, a renamed or copied file stores nothing new, and an update stores only the chunks that changed. New chunks are sealed and written into new shards, and the shard list of the result keeps the earlier shards that its files still use, in their order, followed by the new ones, so an update never rewrites an old shard.

A new shard closes before a record would take it past the target size, one gibibyte by default, unless the record continues a file no larger than a sixteenth of the target, which stays whole in one shard. A shard also closes at its limit of records, and a single large file spans as many shards as it needs. The target is a parameter of the builder because a store with a stricter limit per file packs smaller shards of the same format. A shard grows under a temporary name and takes the name of its ID only when it is complete.

The same files and keys always build the same bytes. Chunk boundaries depend only on the bytes of each file, compression is deterministic, the nonce of every record derives from the record itself, and no time, path, user name or random value enters a shard, so an unchanged chunk is byte for byte the same record in every build, which keeps the patches of stores that compare files small. The result reports the new chunks, their plain and stored bytes, and the chunks and bytes it reused.

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
| 12 | 1 | Kind, 0 for a file. |
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

The delivery of a file says when its encrypted bytes must be on the device: before the app starts, in the background after it starts, or only when the app asks for the file. It never says when a file is decoded into memory, and every file of the app domain is required.

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

A `content::Compatibility` holds what the running build accepts: the digest of its app, its build number and its profile. A manifest of another app, made for another profile, or whose range of app builds leaves the build out raises `ManifestIncompatible` with the reason. The versions of the shard and catalog formats are checked when a manifest is read, and unknown versions raise `UnsupportedVersion`.

The publisher of a channel raises the generation with every release. `ChannelDescriptor::offersUpdate` tells whether a descriptor offers content newer than the generation and manifest the app accepted last, and raises `ManifestRollbackRejected` for an older generation, or for the same generation with another manifest, so a server that replays an old descriptor cannot roll an app back. A rollback is published as a new generation that names the earlier content. `ChannelDescriptor::describes` tells whether a manifest is the content manifest that a descriptor names for its app, channel and generation.

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
| `MissingChunk` | A shard does not hold a chunk its catalog places in it. |
| `MissingShard` | The file of a shard that a manifest names is missing. |

## Security

The content system protects against the practical threats to the files of a client app: browsing an app bundle or an APK, extracting it with archive tools, copying images, data and scripts, crawling named assets, and changing content without the signing key, whether by accident or on purpose. Shards hold only ciphertext and opaque IDs, catalogs that name paths are encrypted, every chunk is authenticated, and the chain from the signed manifest to the content ID of every chunk means that content that decrypts still cannot be changed without the signing key.

It does not protect against a skilled attacker who controls the device: one who inspects memory, captures the GPU, instruments the code that decrypts, or recovers the content key from the app, which must hold it to run offline. That is the boundary of every app that runs content on the client, and the content key of each app is its own, so recovering it exposes only that app.
