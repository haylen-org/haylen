package dev.haylen;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.util.Log;
import android.webkit.MimeTypeMap;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AlertDialog;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// The native dialogs of the app: messages in an `AlertDialog` of AppCompat, and the pickers of files to open and of the destination of a save of the Storage Access Framework, one picker at a time. The activity registers the launchers of the pickers under stable keys before it starts, so a picker that answers after the activity or the process ended finds a launcher, whose answer no call waits for anymore and is logged and dropped. Picked files are copied into the folder of the dialog and saved data is written on a thread of the dialogs.
final class HaylenDialogs {
    // Creates a document of the MIME type that each save asks for, which `CreateDocument` fixes when it is registered. The input is the suggested name and the MIME type.
    private static final class CreateFile extends ActivityResultContract<String[], Uri> {
        @Override
        public Intent createIntent(Context context, String[] input) {
            return new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType(input[1]).putExtra(Intent.EXTRA_TITLE, input[0]);
        }

        @Override
        public Uri parseResult(int resultCode, Intent intent) {
            return intent != null && resultCode == Activity.RESULT_OK ? intent.getData() : null;
        }
    }

    // The picker that shows: the id of its dialog and the folder of the copies of picked files or the data of a save.
    private static final class Picking {
        final long id;
        final File folder;
        final byte[] data;

        Picking(long id, File folder, byte[] data) {
            this.id = id;
            this.folder = folder;
            this.data = data;
        }
    }

    private static final String TAG = "haylen";
    private static final String ANY_TYPE = "*/*";
    private static final String BYTES_TYPE = "application/octet-stream";

    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final ExecutorService worker = Executors.newSingleThreadExecutor(task -> {
        Thread thread = new Thread(task, "haylen-dialogs");
        thread.setDaemon(true);
        return thread;
    });
    private static final Map<Long, AlertDialog> messages = new HashMap<>();
    private static ActivityResultLauncher<String[]> openFile;
    private static ActivityResultLauncher<String[]> openFiles;
    private static ActivityResultLauncher<String[]> saveFile;
    private static Picking picking;

    private HaylenDialogs() {}

    // Called in `onCreate` of every activity, before it starts.
    static void activityCreated(HaylenActivity activity) {
        openFile = activity.getActivityResultRegistry().register("haylen.dialogs.openFile", activity, new ActivityResultContracts.OpenDocument(), uri -> opened(uri == null ? null : Collections.singletonList(uri)));
        openFiles = activity.getActivityResultRegistry().register("haylen.dialogs.openFiles", activity, new ActivityResultContracts.OpenMultipleDocuments(), uris -> opened(uris == null || uris.isEmpty() ? null : uris));
        saveFile = activity.getActivityResultRegistry().register("haylen.dialogs.saveFile", activity, new CreateFile(), HaylenDialogs::created);
    }

    // The messages of an activity that goes away close with it, since the engine that showed them stopped.
    static void activityDestroyed() {
        for (AlertDialog message : new ArrayList<>(messages.values())) {
            message.dismiss();
        }
        messages.clear();
    }

    // Called from the frame thread of the engine with the JSON of `AndroidDialogJson` and the data of a save.
    static void show(long id, byte[] json, byte[] data) {
        String text = new String(json, StandardCharsets.UTF_8);
        mainThread.post(() -> {
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null) {
                fail(id, "failed", "The app has no activity to show the dialog over.");
                return;
            }
            try {
                JSONObject request = new JSONObject(text);
                switch (request.getString("kind")) {
                    case "message" -> showMessage(activity, id, request);
                    case "openFiles" -> pick(id, request.getBoolean("multiple") ? openFiles : openFile, types(request.getJSONArray("extensions")), new File(request.getString("folder")), null);
                    default -> pick(id, saveFile, new String[] {request.getString("name"), type(request.getJSONArray("extensions"))}, null, data);
                }
            } catch (ActivityNotFoundException error) {
                picking = null;
                fail(id, "unsupported", "This device has no app that picks documents, as Android TV devices often lack one.");
            } catch (JSONException | RuntimeException error) {
                picking = null;
                fail(id, "failed", "The dialog failed to show: " + error.getMessage());
            }
        });
    }

    // Called from the frame thread of the engine when the app gives a dialog up. A message closes, while a picker belongs to another app, which the app cannot close, so its answer goes nowhere.
    static void cancel(long id) {
        mainThread.post(() -> {
            AlertDialog message = messages.remove(id);
            if (message != null) {
                message.dismiss();
            }
            if (picking != null && picking.id == id) {
                picking = null;
            }
        });
    }

    // The buttons are the positive, the negative and the neutral button of the dialog, in the order of the app. The back button and a touch outside dismiss the message without a button.
    private static void showMessage(HaylenActivity activity, long id, JSONObject request) throws JSONException {
        JSONArray buttons = request.getJSONArray("buttons");
        AlertDialog.Builder builder = new AlertDialog.Builder(activity).setMessage(request.getString("text"));
        String title = request.getString("title");
        if (!title.isEmpty()) {
            builder.setTitle(title).setIcon(request.getString("messageKind").equals("info") ? android.R.drawable.ic_dialog_info : android.R.drawable.ic_dialog_alert);
        }
        builder.setPositiveButton(buttons.getString(0), (dialog, which) -> answer(id, 0));
        if (buttons.length() > 1) {
            builder.setNegativeButton(buttons.getString(1), (dialog, which) -> answer(id, 1));
        }
        if (buttons.length() > 2) {
            builder.setNeutralButton(buttons.getString(2), (dialog, which) -> answer(id, 2));
        }
        builder.setOnCancelListener(dialog -> {
            messages.remove(id);
            nativeDismissed(id);
        });
        messages.put(id, builder.show());
    }

    private static void answer(long id, int button) {
        messages.remove(id);
        nativeButton(id, button);
    }

    // The engine may ask for several dialogs in one frame, while one picker shows at a time.
    private static void pick(long id, ActivityResultLauncher<String[]> launcher, String[] input, File folder, byte[] data) {
        if (picking != null) {
            fail(id, "failed", "Another picker shows. Ask for the next one once it answered.");
            return;
        }
        picking = new Picking(id, folder, data);
        launcher.launch(input);
    }

    // The files are copied into the folder of the dialog, each under its name, or in a numbered folder when another picked file took the name.
    private static void opened(List<Uri> uris) {
        Picking current = take("The document picker");
        if (current == null) {
            return;
        }
        if (uris == null) {
            nativeDismissed(current.id);
            return;
        }
        ContentResolver resolver = HaylenBridge.activity().getContentResolver();
        worker.execute(() -> {
            try {
                JSONArray copies = new JSONArray();
                for (Uri uri : uris) {
                    String name = displayName(resolver, uri);
                    File copy = new File(current.folder, name);
                    for (int index = 2; copy.exists(); ++index) {
                        copy = new File(new File(current.folder, String.valueOf(index)), name);
                    }
                    copyFile(resolver, uri, copy);
                    copies.put(new JSONObject().put("name", name).put("path", copy.getAbsolutePath()));
                }
                nativeFiles(current.id, HaylenBridge.utf8(copies.toString()));
            } catch (IOException | JSONException | RuntimeException error) {
                fail(current.id, "failed", "The picked files could not be copied: " + error.getMessage());
            }
        });
    }

    // The data is written into the document, and the answer names it without a path, since documents have none.
    private static void created(Uri uri) {
        Picking current = take("The document creator");
        if (current == null) {
            return;
        }
        if (uri == null) {
            nativeDismissed(current.id);
            return;
        }
        ContentResolver resolver = HaylenBridge.activity().getContentResolver();
        worker.execute(() -> {
            try (OutputStream output = resolver.openOutputStream(uri, "wt")) {
                if (output == null) {
                    throw new IOException("The document \"" + uri + "\" cannot be written.");
                }
                output.write(current.data);
                output.flush();
                nativeSaved(current.id, HaylenBridge.utf8(displayName(resolver, uri)));
            } catch (IOException | RuntimeException error) {
                fail(current.id, "failed", "The file could not be saved: " + error.getMessage());
            }
        });
    }

    // A picker that answers after its activity or its process ended finds no dialog waiting, since the engine that asked for it stopped.
    private static Picking take(String picker) {
        Picking current = picking;
        picking = null;
        if (current == null) {
            Log.i(TAG, picker + " answered after the app that asked for it stopped, so the answer is dropped.");
        }
        return current;
    }

    // The MIME types of the extensions, or every type when an extension has none that Android knows, since the picker would hide those files otherwise.
    private static String[] types(JSONArray extensions) throws JSONException {
        Set<String> types = new LinkedHashSet<>();
        for (int index = 0; index < extensions.length(); ++index) {
            String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extensions.getString(index).toLowerCase(Locale.ROOT));
            if (type == null) {
                return new String[] {ANY_TYPE};
            }
            types.add(type);
        }
        return types.isEmpty() ? new String[] {ANY_TYPE} : types.toArray(new String[0]);
    }

    // A document takes one MIME type: the one of the first extension that Android knows, or plain bytes.
    private static String type(JSONArray extensions) throws JSONException {
        for (int index = 0; index < extensions.length(); ++index) {
            String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extensions.getString(index).toLowerCase(Locale.ROOT));
            if (type != null) {
                return type;
            }
        }
        return BYTES_TYPE;
    }

    // A name that the provider leaves out, or that holds a separator, still gives a file name.
    private static String displayName(ContentResolver resolver, Uri uri) {
        String name = null;
        try (Cursor cursor = resolver.query(uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                name = cursor.getString(0);
            }
        }
        if (name == null || name.isEmpty()) {
            name = uri.getLastPathSegment() != null ? uri.getLastPathSegment() : "file";
        }
        return name.replace('/', '_');
    }

    private static void copyFile(ContentResolver resolver, Uri uri, File copy) throws IOException {
        File parent = copy.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("The folder \"" + parent + "\" cannot be created.");
        }
        try (InputStream input = resolver.openInputStream(uri); OutputStream output = new FileOutputStream(copy)) {
            if (input == null) {
                throw new IOException("The document \"" + uri + "\" cannot be read.");
            }
            byte[] buffer = new byte[65536];
            for (int read; (read = input.read(buffer)) > 0; ) {
                output.write(buffer, 0, read);
            }
        }
    }

    private static void fail(long id, String code, String message) {
        nativeFailed(id, HaylenBridge.utf8(code), HaylenBridge.utf8(message));
    }

    private static native void nativeButton(long id, int button);

    private static native void nativeFiles(long id, byte[] json);

    private static native void nativeSaved(long id, byte[] name);

    private static native void nativeDismissed(long id);

    private static native void nativeFailed(long id, byte[] code, byte[] message);
}
