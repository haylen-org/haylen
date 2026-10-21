# 📦 Content

Content brings the files of an app to the device and into memory, protects them in releases and updates them.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Content system | What a protected release is and how it works: its chunks, shards, cryptography and formats, the content tool and publication, the delivery settings and packs, downloads, updates, the content manager, the store of the browser, the local store, performance and security. | [`content.md`](content.md) |
| Loading | How the code and assets of an app reach the device and memory in development and in a release: startup, autoloads, assets, preload groups, scenes, delivery, caches, what each platform and store ships, updates and keys. | [`loading.md`](loading.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.assets` | Loading package files now or in the background, and preload groups. | [`lua-api/assets.md`](../lua-api/assets.md) |
| `haylen.content` | Which content plays and which assets are on the device, assets brought down ahead of time, and the updates of the content. | [`lua-api/content.md`](../lua-api/content.md) |
