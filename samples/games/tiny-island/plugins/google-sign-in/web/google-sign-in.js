// Native part of the Google sign-in of Tiny Island on the web. It answers `google-sign-in.signIn` with Google Identity Services, which it loads when the player first signs in, and needs a server that allows the Google script, such as `haylen.py run --platform web --coep off`.
export default function load(context) {
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

    context.register("signIn", async () => {
        if (!context.config.clientId) {
            throw new Error('Google sign-in needs the web client id of the game\'s Google Cloud project. Set "plugins.google-sign-in.clientId" in the "app.json" of the game.');
        }
        await loadLibrary();
        return new Promise((resolve, reject) => {
            google.accounts.id.initialize({
                client_id: context.config.clientId,
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
}
