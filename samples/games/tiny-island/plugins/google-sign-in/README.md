# Google Sign-In

The local plugin of [Tiny Island](../../README.md) that signs the player in with a Google account: Credential Manager answers on Android and Google Identity Services in the browser. The game lists it in its `app.json` and calls it from its settings screen, and `make.py` builds it into the project of each platform as the [plugin guide](../../../../../docs/plugins.md) describes.

## Setup

Both platforms sign in with the web client id of the Google Cloud project of the game, which `app.json` gives the plugin:

```json
{
    "plugins": {
        "google-sign-in": {"clientId": "1234567890-abc.apps.googleusercontent.com"}
    }
}
```

| Platform | Setup |
| --- | --- |
| Android | The Android client of the same project names the package and the signing certificate of the APK. The module brings Credential Manager and the Google ID library. |
| Web | The web client lists the origin that serves the page among its authorized JavaScript origins. The page loads the Google script, which does not send the resource policy that `require-corp` asks for, so the server runs with `--coep off` or `--coep credentialless`. |

Without a client id, `signIn` fails with a message that names the key to set.

## Lua API

```lua
local googleSignIn = require('google-sign-in')

if googleSignIn.available then
    local account, err = googleSignIn.signIn():await()
    print(account and account.email or err.message)
end
```

| Member | Meaning |
| --- | --- |
| `googleSignIn.available` | Whether the native part of the plugin runs on this platform, which Android and the web have. |
| `googleSignIn.signIn()` | Calls `google-sign-in.signIn`, which answers `{idToken, email, name, picture}` for the account the player picks, or fails when the player closes the prompt. |
