// Page code of Tiny Island, which answers auth.google.signIn with Google Identity Services, loaded only when the player first signs in.
// Google sign-in needs the web client id of the game's Google Cloud project here, and a server that allows the Google script, such as make.py run --platform web --coep off.
const googleClientId = "";

Module.preRun.push(() => {
    let library;
    const loadLibrary = () => {
        library = library || new Promise((resolve, reject) => {
            const script = document.createElement("script");
            script.src = "https://accounts.google.com/gsi/client";
            script.onload = resolve;
            script.onerror = () => reject(new Error("Google Identity Services could not be loaded."));
            document.head.appendChild(script);
        });
        return library;
    };
    const decode = (token) => JSON.parse(atob(token.split(".")[1].replace(/-/g, "+").replace(/_/g, "/")));

    Module.haylen.register("auth.google.signIn", async () => {
        if (!googleClientId) {
            throw new Error("Google sign-in needs the web client id of the game's Google Cloud project in platform/web/app.js.");
        }
        await loadLibrary();
        return new Promise((resolve, reject) => {
            google.accounts.id.initialize({
                client_id: googleClientId,
                use_fedcm_for_prompt: true,
                callback: (response) => {
                    const claims = decode(response.credential);
                    resolve({ idToken: response.credential, email: claims.email, name: claims.name, picture: claims.picture || null });
                },
            });
            google.accounts.id.prompt((moment) => {
                if (moment.isSkippedMoment() || (moment.isDismissedMoment() && moment.getDismissedReason() !== "credential_returned")) {
                    reject(new Error("The player closed the Google sign-in prompt."));
                }
            });
        });
    });
});
