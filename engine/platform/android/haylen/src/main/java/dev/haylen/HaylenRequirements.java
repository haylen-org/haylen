package dev.haylen;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ComponentInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

// Tells what the app holds: the permissions, components and meta-data of its merged manifest, the classes of its build and the links it opens. An app may leave out what a plugin needs, so a plugin checks what it needs before it calls a system API that needs it, and a missing requirement logs once what is missing and how to add it and fails the call with the code `unsupported` instead of crashing the app.
public final class HaylenRequirements {
    // What a feature needs from the app, with the file that adds it and the snippet that the file takes.
    public static final class Requirement {
        private final Kind kind;
        private final String name;
        private final String description;
        private final String file;
        private final String snippet;
        private final String instructions;

        private Requirement(Kind kind, String name, String description, String file, String snippet, String instructions) {
            this.kind = kind;
            this.name = name;
            this.description = description;
            this.file = file;
            this.snippet = snippet;
            this.instructions = instructions;
        }

        // A permission that the merged manifest declares, which the app adds in `platforms.android.manifest.permissions` of `haylen.json` when no library brings it. A normal permission, such as `VIBRATE`, is granted exactly when it is declared, while a dangerous one still needs the person to grant it.
        public static Requirement permission(String name) {
            String snippet = "{\"platforms\": {\"android\": {\"manifest\": {\"permissions\": [\"" + name + "\"]}}}}";
            String instructions = "Add \"" + snippet + "\" to \"" + APP_MANIFEST + "\", and leave the permission out of \"platforms.android.manifest.removedPermissions\".";
            return new Requirement(Kind.PERMISSION, name, "the permission \"" + name + "\"", APP_MANIFEST, snippet, instructions);
        }

        // A class of the build, which a dependency of the Android module of the plugin brings.
        public static Requirement className(String name, String dependency) {
            return module(Kind.CLASS, name, "the class \"" + name + "\"", BUILD_SCRIPT, "implementation(\"" + dependency + "\")");
        }

        // A meta-data entry of the application, with the value it takes.
        public static Requirement metaData(String name, String value) {
            return module(Kind.META_DATA, name, "the meta-data \"" + name + "\"", MODULE_MANIFEST, "<meta-data android:name=\"" + name + "\" android:value=\"" + value + "\" />");
        }

        public static Requirement activity(String className) {
            return module(Kind.ACTIVITY, className, "the activity \"" + className + "\"", MODULE_MANIFEST, "<activity android:name=\"" + className + "\" android:exported=\"false\" />");
        }

        public static Requirement service(String className) {
            return module(Kind.SERVICE, className, "the service \"" + className + "\"", MODULE_MANIFEST, "<service android:name=\"" + className + "\" android:exported=\"false\" />");
        }

        // A content provider with the authority, which the class implements.
        public static Requirement provider(String authority, String className) {
            return module(Kind.PROVIDER, authority, "a provider with the authority \"" + authority + "\"", MODULE_MANIFEST, "<provider android:name=\"" + className + "\" android:authorities=\"" + authority + "\" android:exported=\"false\" />");
        }

        // Links with the scheme that open the app, which reach its plugins through `HaylenLinkActivity`.
        public static Requirement urlScheme(String scheme) {
            String filter = "<intent-filter><action android:name=\"android.intent.action.VIEW\" /><category android:name=\"android.intent.category.DEFAULT\" /><category android:name=\"android.intent.category.BROWSABLE\" /><data android:scheme=\"" + scheme + "\" /></intent-filter>";
            return module(Kind.URL_SCHEME, scheme, "an activity that opens the links with the scheme \"" + scheme + "\"", MODULE_MANIFEST, "<activity android:name=\"dev.haylen.HaylenLinkActivity\" android:exported=\"true\">" + filter + "</activity>");
        }

        // The service of the audio that plays in the background, which the library `haylen-playback` brings to an app that declares `audio.background` in `haylen.json`.
        static Requirement backgroundAudio() {
            String snippet = "{\"audio\": {\"background\": true}}";
            String instructions = "Add \"" + snippet + "\" to \"" + APP_MANIFEST + "\".";
            return new Requirement(Kind.SERVICE, HaylenPlaybackService.class.getName(), "the service of the audio that plays in the background", APP_MANIFEST, snippet, instructions);
        }

        // What only the Android module of a plugin brings, with a file relative to the module.
        private static Requirement module(Kind kind, String name, String description, String file, String snippet) {
            String instructions = "Add \"" + snippet + "\" to \"" + file + "\" of the Android module of the plugin.";
            return new Requirement(kind, name, description, file, snippet, instructions);
        }

        // One of `permission`, `class`, `metaData`, `activity`, `service`, `provider` and `urlScheme`.
        public String kind() {
            return kind.label;
        }

        // The permission, the class, the meta-data, the component, the authority or the scheme.
        public String name() {
            return name;
        }

        // The file that adds the requirement: `haylen.json` of the app, or a file of the Android module of the plugin, relative to the module.
        public String file() {
            return file;
        }

        public String snippet() {
            return snippet;
        }

        // The sentence that tells how to add the requirement.
        String instructions() {
            return instructions;
        }

        String description() {
            return description;
        }

        Map<String, Object> toMap() {
            Map<String, Object> map = new LinkedHashMap<>();
            map.put("kind", kind.label);
            map.put("name", name);
            map.put("file", file);
            map.put("snippet", snippet);
            return map;
        }
    }

    private enum Kind {
        PERMISSION("permission"),
        CLASS("class"),
        META_DATA("metaData"),
        ACTIVITY("activity"),
        SERVICE("service"),
        PROVIDER("provider"),
        URL_SCHEME("urlScheme");

        final String label;

        Kind(String label) {
            this.label = label;
        }
    }

    private static final String TAG = "haylen";
    private static final String APP_MANIFEST = "haylen.json";
    private static final String MODULE_MANIFEST = "src/main/AndroidManifest.xml";
    private static final String BUILD_SCRIPT = "build.gradle.kts";
    private static final int PACKAGE_FLAGS = PackageManager.GET_PERMISSIONS | PackageManager.GET_ACTIVITIES | PackageManager.GET_SERVICES | PackageManager.GET_PROVIDERS | PackageManager.GET_META_DATA;

    // The merged manifest stays the same while the process lives, so it is read once. What was logged is kept by owner and requirement, so each one logs once.
    private static PackageInfo packageInfo;
    private static final Set<String> logged = Collections.synchronizedSet(new HashSet<>());

    private final Context context;
    private final String owner;

    // The owner names what needs the requirements at the start of a sentence, such as `The plugin "share-sheet"`.
    HaylenRequirements(Context context, String owner) {
        this.context = context.getApplicationContext();
        this.owner = owner;
    }

    // What needs the requirements, at the start of a sentence.
    String owner() {
        return owner;
    }

    // Whether the merged manifest declares the permission.
    public boolean hasPermission(String name) {
        String[] requested = readPackage().requestedPermissions;
        return requested != null && Arrays.asList(requested).contains(name);
    }

    // Whether the app holds the permission now, which a dangerous permission gets once the person grants it.
    public boolean isGranted(String name) {
        return context.checkSelfPermission(name) == PackageManager.PERMISSION_GRANTED;
    }

    // Whether the build of the app has the class, which R8 may have removed or a dependency may lack.
    public boolean hasClass(String name) {
        try {
            Class.forName(name, false, context.getClassLoader());
            return true;
        } catch (ClassNotFoundException | LinkageError error) {
            return false;
        }
    }

    public boolean hasMetaData(String name) {
        Bundle metaData = readPackage().applicationInfo.metaData;
        return metaData != null && metaData.containsKey(name);
    }

    public boolean hasActivity(String className) {
        return declares(readPackage().activities, className);
    }

    public boolean hasService(String className) {
        return declares(readPackage().services, className);
    }

    public boolean hasProvider(String authority) {
        ProviderInfo[] providers = readPackage().providers;
        for (int index = 0; providers != null && index < providers.length; ++index) {
            if (providers[index].authority != null && Arrays.asList(providers[index].authority.split(";")).contains(authority)) {
                return true;
            }
        }

        return false;
    }

    // Whether an activity of the app opens links with the scheme.
    public boolean handlesScheme(String scheme) {
        Intent view = new Intent(Intent.ACTION_VIEW, Uri.parse(scheme + "://")).addCategory(Intent.CATEGORY_BROWSABLE).setPackage(context.getPackageName());
        return !queryActivities(view).isEmpty();
    }

    // Throws a `HaylenBridge.Failure` with the code `unsupported` when the app lacks any of the requirements, whose data lists each missing one in `missing` as `{kind, name, file, snippet}`, and logs each missing one once.
    public void require(Requirement... requirements) throws HaylenBridge.Failure {
        List<Requirement> missing = new ArrayList<>();
        for (Requirement requirement : requirements) {
            if (!has(requirement)) {
                missing.add(requirement);
            }
        }

        if (missing.isEmpty()) {
            return;
        }

        List<Map<String, Object>> data = new ArrayList<>();
        for (Requirement requirement : missing) {
            report(requirement, Log.WARN, "the calls that need it fail with the code \"unsupported\"");
            data.add(requirement.toMap());
        }

        throw new HaylenBridge.Failure(owner + " needs " + describe(missing) + ", which the app lacks.", "unsupported", Collections.singletonMap("missing", data));
    }

    // The descriptions of the requirements as one list of a sentence, such as "a, b and c".
    static String describe(List<Requirement> requirements) {
        List<String> descriptions = new ArrayList<>();
        for (Requirement requirement : requirements) {
            descriptions.add(requirement.description());
        }

        if (descriptions.size() == 1) {
            return descriptions.get(0);
        }

        String leading = String.join(", ", descriptions.subList(0, descriptions.size() - 1));
        return leading + " and " + descriptions.get(descriptions.size() - 1);
    }

    // Logs once per process what the owner needs, what happens without it and how to add it.
    void report(Requirement requirement, int priority, String consequence) {
        if (logged.add(owner + "\n" + requirement.kind() + "\n" + requirement.name())) {
            Log.println(priority, TAG, owner + " needs " + requirement.description() + ", which the app lacks, so " + consequence + ". " + requirement.instructions());
        }
    }

    private boolean has(Requirement requirement) {
        return switch (requirement.kind) {
            case PERMISSION -> hasPermission(requirement.name);
            case CLASS -> hasClass(requirement.name);
            case META_DATA -> hasMetaData(requirement.name);
            case ACTIVITY -> hasActivity(requirement.name);
            case SERVICE -> hasService(requirement.name);
            case PROVIDER -> hasProvider(requirement.name);
            case URL_SCHEME -> handlesScheme(requirement.name);
        };
    }

    private static boolean declares(ComponentInfo[] components, String className) {
        for (int index = 0; components != null && index < components.length; ++index) {
            if (components[index].name.equals(className)) {
                return true;
            }
        }

        return false;
    }

    private PackageInfo readPackage() {
        synchronized (HaylenRequirements.class) {
            if (packageInfo == null) {
                packageInfo = queryPackage();
            }

            return packageInfo;
        }
    }

    // Android 13 takes the flags of package queries as objects and deprecates the integer flags.
    @SuppressWarnings("deprecation")
    private PackageInfo queryPackage() {
        PackageManager manager = context.getPackageManager();
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return manager.getPackageInfo(context.getPackageName(), PackageManager.PackageInfoFlags.of(PACKAGE_FLAGS));
            }

            return manager.getPackageInfo(context.getPackageName(), PACKAGE_FLAGS);
        } catch (PackageManager.NameNotFoundException error) {
            throw new IllegalStateException("The package of the app is not installed.", error);
        }
    }

    @SuppressWarnings("deprecation")
    private List<ResolveInfo> queryActivities(Intent intent) {
        PackageManager manager = context.getPackageManager();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return manager.queryIntentActivities(intent, PackageManager.ResolveInfoFlags.of(0));
        }

        return manager.queryIntentActivities(intent, 0);
    }
}
