# Sign-In

The local plugin of [Tiny Island](../../README.md) that signs the player in with the account of an Android device: Credential Manager answers on Android and the identity service of the same account answers in the browser. The game lists it in its `app.json` and calls it from its settings screen, and `haylen.py` builds it into the project of each platform as the [plugin guide](../../../../../docs/platform/plugins.md) describes.

## Setup

Both platforms sign in with the web client id that the identity service of the account gives the game, which `app.json` gives the plugin:

```json
{
    "plugins": {
        "sign-in": {"clientId": "1234567890-abc.apps.example.com"}
    }
}
```

| Platform | Setup |
| --- | --- |
| Android | The Android client of the same project of the identity service names the package and the signing certificate of the APK. The module brings Credential Manager and the ID token library of the identity service. |
| Web | The web client lists the origin that serves the page among its authorized JavaScript origins. The page loads the script of the identity service, which does not send the resource policy that `require-corp` asks for, so a server of the page sends no `Cross-Origin-Embedder-Policy`, as the development server does, or `credentialless`. |

Without a client id, `signIn` fails with a message that names the key to set.

## Lua API

```lua
local signIn = require('sign-in')

if signIn.available then
    local account, err = signIn.signIn():await()
    print(account and account.email or err.message)
end
```

| Member | Meaning |
| --- | --- |
| `signIn.available` | Whether the native part of the plugin runs on this platform, which Android and the web have. |
| `signIn.signIn()` | Calls `sign-in.signIn`, which answers `{idToken, email, name, picture}` for the account the player picks, or fails when the player closes the prompt. |
