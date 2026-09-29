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
import org.json.JSONException;
import org.json.JSONObject;

// Answers auth.google.signIn with the Google account the player picks through Credential Manager. The web client id of the game's Google Cloud project comes from the app, never from the engine.
final class GoogleSignInPlugin {
    private GoogleSignInPlugin() {}

    static void register(String serverClientId) {
        HaylenBridge.register("auth.google.signIn", (params, reply) -> signIn(serverClientId, reply));
    }

    private static void signIn(String serverClientId, HaylenBridge.Reply reply) {
        if (serverClientId.isEmpty()) {
            reply.failure("Google sign-in needs the web client id of the game's Google Cloud project. Set googleServerClientId=<id> in ~/.gradle/gradle.properties or pass -PgoogleServerClientId=<id> to Gradle.");
            return;
        }

        Activity activity = HaylenBridge.activity();
        GetGoogleIdOption option = new GetGoogleIdOption.Builder().setServerClientId(serverClientId).setFilterByAuthorizedAccounts(false).setAutoSelectEnabled(true).build();
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
                reply.failure("Google sign-in failed: " + error.getType() + " " + error.getMessage());
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
