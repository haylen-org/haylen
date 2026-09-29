# Official plugins

Every folder here is an official Haylen plugin: a plugin package with its `plugin.json`, its Lua API under `source/`, its README and its native parts for each platform, as the [plugin guide](../docs/plugins.md) describes. The folder of a plugin is named after its id.

`python3 make.py plugin list` lists the official plugins, and `python3 make.py plugin add <id> --app <app>` copies one into `plugins/<id>/` of an app and lists it in the `plugins` section of its `app.json`. `python3 make.py plugin new plugins/<id>` starts a new plugin here from `templates/plugin/`.
