package dev.haylen.tinyisland;

import android.app.Activity;
import android.os.CancellationSignal;
import androidx.credentials.Credential;
import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.CustomCredential;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.exceptions.GetCredentialException;
import com.google.android.libraries.identity.googleid.GetGoogleIdOption;
import com.google.android.libraries.identity.googleid.GoogleIdTokenCredential;
import dev.haylen.HaylenBridge;
import dev.haylen.HaylenPlugin;
import dev.haylen.HaylenPluginContext;
import org.json.JSONException;
import org.json.JSONObject;

// Native part of the Google sign-in of Tiny Island on Android. It answers `google-sign-in.signIn` with the Google account the player picks through Credential Manager, with the web client id of the game's Google Cloud project from the parameter `clientId` of the plugin.
public final class GoogleSignInPlugin extends HaylenPlugin {
    private HaylenPluginContext context;

    @Override
    public void onLoad(HaylenPluginContext context) {
        this.context = context;
        context.register("signIn", (params, reply) -> signIn(context.config().optString("clientId", ""), reply));
    }

    private void signIn(String clientId, HaylenBridge.Reply reply) {
        Activity activity = context.activity();
        if (clientId.isEmpty()) {
            reply.failure("Google sign-in needs the web client id of the game's Google Cloud project. Set \"plugins.google-sign-in.clientId\" in the \"app.json\" of the game.");
            return;
        }
        if (activity == null) {
            reply.failure("Google sign-in needs the activity of the game, which is not running.");
            return;
        }

        GetGoogleIdOption option = new GetGoogleIdOption.Builder().setServerClientId(clientId).setFilterByAuthorizedAccounts(false).setAutoSelectEnabled(true).build();
        GetCredentialRequest request = new GetCredentialRequest.Builder().addCredentialOption(option).build();
        CredentialManager.create(activity).getCredentialAsync(activity, request, new CancellationSignal(), activity::runOnUiThread, new CredentialManagerCallback<GetCredentialResponse, GetCredentialException>() {
            @Override
            public void onResult(GetCredentialResponse response) {
                Credential credential = response.getCredential();
                if (!(credential instanceof CustomCredential) || !GoogleIdTokenCredential.TYPE_GOOGLE_ID_TOKEN_CREDENTIAL.equals(credential.getType())) {
                    reply.failure("Credential Manager returned a credential that is not a Google account.");
                    return;
                }
                try {
                    reply.success(describe(GoogleIdTokenCredential.createFrom(credential.getData())));
                } catch (JSONException error) {
                    reply.failure(error.getMessage());
                }
            }

            @Override
            public void onError(GetCredentialException error) {
                reply.failure("Google sign-in failed: \"" + error.getType() + "\" " + error.getMessage());
            }
        });
    }

    private static JSONObject describe(GoogleIdTokenCredential account) throws JSONException {
        JSONObject result = new JSONObject();
        result.put("idToken", account.getIdToken());
        result.put("email", account.getId());
        result.put("name", account.getDisplayName());
        result.put("picture", account.getProfilePictureUri() == null ? JSONObject.NULL : account.getProfilePictureUri().toString());
        return result;
    }
}
