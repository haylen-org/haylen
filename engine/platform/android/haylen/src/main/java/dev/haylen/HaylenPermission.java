package dev.haylen;

import android.content.Context;
import android.content.pm.PackageManager;

// A permission of `system.permission`, such as `camera`, as a set of permissions of the manifest that the engine asks the person for with the prompt of the system. A plugin declares the permission in the `permissions` of its `plugin.json`, registers it with `HaylenPluginContext.registerPermission` and overrides what differs from the default: why the device lacks the capability, the permissions of the manifest it needs on the Android that runs, what its prompt asks for, whether the app holds it, its state where Android asks for nothing and a permission the person must grant first.
public class HaylenPermission {
    private final String name;
    private final String capability;
    private final String[] permissions;

    // The name of `system.permission`, the capability in words for the sentences of answers, such as `the camera`, and the permissions of the manifest that it needs, such as `android.permission.CAMERA`.
    public HaylenPermission(String name, String capability, String... required) {
        this.name = name;
        this.capability = capability;
        permissions = required.clone();
    }

    public String getName() {
        return name;
    }

    public String getCapability() {
        return capability;
    }

    // The sentence that says why the device lacks the capability, such as `The device has no motion sensors.`, or `null` where it has it. A permission with a reason answers `unsupported` with it and never shows a prompt.
    public String unsupportedReason(Context context) {
        return null;
    }

    // The permissions of the manifest that it needs on the Android that runs, empty where Android asks for nothing.
    public String[] required(Context context) {
        return permissions.clone();
    }

    // What the prompt asks for, such as the precise location next to the approximate one where the manifest declares it, so the prompt offers the choice.
    public String[] requested(Context context, HaylenRequirements requirements) {
        return required(context);
    }

    // Whether the app holds it.
    public boolean isGranted(Context context) {
        for (String permission : required(context)) {
            if (context.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    // The state where `required` is empty, `granted` or `denied`, such as notifications before Android 13, which the person may turn off in the settings. A device without the capability answers through `unsupportedReason` instead.
    public String stateWithoutPrompt(Context context) {
        return "granted";
    }

    // A permission the person must grant first, which Android asks for in a prompt of its own, such as the location before the location in the background, or `null`.
    public HaylenPermission prerequisite() {
        return null;
    }
}
