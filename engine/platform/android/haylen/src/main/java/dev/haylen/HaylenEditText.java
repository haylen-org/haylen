package dev.haylen;

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import java.nio.charset.StandardCharsets;
import java.util.function.Consumer;
import org.json.JSONException;
import org.json.JSONObject;

// The hidden field that edits the focused text field of the engine. The activity adds it over the app, the engine places it over the field it edits, and the software keyboard and every input method type into it. After each change its text, selection and composing span go back to the engine, while touches, gamepads and the keys it leaves alone still reach the engine.
final class HaylenEditText extends EditText {
    // The actions, keyboards, return keys and capitalizations follow the order of their enums in haylen/platform/TextInput.hpp.
    private static final int ACTION_SUBMIT = 0;
    private static final int ACTION_NEXT = 1;
    private static final int ACTION_CANCEL = 2;
    private static final int ACTION_DISMISSED = 3;
    private static final int KEYBOARD_MULTILINE = 1;
    private static final int KEYBOARD_NUMBER = 2;
    private static final int KEYBOARD_DECIMAL = 3;
    private static final int KEYBOARD_PHONE = 4;
    private static final int KEYBOARD_EMAIL = 5;
    private static final int KEYBOARD_URL = 6;
    private static final int KEYBOARD_PASSWORD = 8;
    private static final int[] RETURN_KEYS = {EditorInfo.IME_ACTION_UNSPECIFIED, EditorInfo.IME_ACTION_DONE, EditorInfo.IME_ACTION_GO, EditorInfo.IME_ACTION_NEXT, EditorInfo.IME_ACTION_SEARCH, EditorInfo.IME_ACTION_SEND};
    private static final int[] CAPITALIZATIONS = {0, InputType.TYPE_TEXT_FLAG_CAP_SENTENCES, InputType.TYPE_TEXT_FLAG_CAP_WORDS, InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS};

    private static HaylenEditText current;

    private final InputMethodManager methods;
    private long field;
    private long revision;
    private boolean multiline;
    private boolean editing;
    private boolean applying;
    private boolean reportPending;
    private boolean keyboardShown;

    HaylenEditText(Context context) {
        super(context);
        methods = context.getSystemService(InputMethodManager.class);
        setAlpha(0.0f);
        setBackground(null);
        setPadding(0, 0, 0, 0);
        setFocusable(false);
        addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence text, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence text, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable text) {
                scheduleReport();
            }
        });
        setOnEditorActionListener((view, action, event) -> {
            if (editing) {
                nativeTextAction(field, ACTION_SUBMIT);
            }
            return true;
        });
        setOnKeyListener((view, code, event) -> onEditKey(code, event));
    }

    static void attach(HaylenEditText editor) {
        current = editor;
    }

    // Called from the frame thread of the engine with the field as JSON, which the UI thread applies.
    static void edit(byte[] json) {
        try {
            JSONObject field = new JSONObject(new String(json, StandardCharsets.UTF_8));
            onUiThread(editor -> editor.apply(field));
        } catch (JSONException error) {
            throw new IllegalArgumentException("The engine sent a text field that is not JSON.", error);
        }
    }

    static void finish() {
        onUiThread(HaylenEditText::stop);
    }

    // A keyboard the user closed while a field was being edited lets that field go.
    void setKeyboardShown(boolean shown) {
        if (keyboardShown && !shown && editing) {
            nativeTextAction(field, ACTION_DISMISSED);
        }
        keyboardShown = shown;
    }

    @Override
    protected void onSelectionChanged(int start, int end) {
        super.onSelectionChanged(start, end);
        scheduleReport();
    }

    private static void onUiThread(Consumer<HaylenEditText> action) {
        Activity activity = HaylenBridge.activity();
        HaylenEditText editor = current;
        if (activity != null && editor != null) {
            activity.runOnUiThread(() -> action.accept(editor));
        }
    }

    private void apply(JSONObject value) {
        long id = value.optLong("id");
        long nextRevision = value.optLong("revision");
        int keyboard = value.optInt("keyboard");
        multiline = keyboard == KEYBOARD_MULTILINE;
        int type = inputType(keyboard, CAPITALIZATIONS[value.optInt("capitalization")], value.optBoolean("autocorrect"));
        int options = EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN | RETURN_KEYS[value.optInt("returnKey")] | (multiline ? EditorInfo.IME_FLAG_NO_ENTER_ACTION : 0);
        boolean reconfigured = getInputType() != type || getImeOptions() != options;
        if (reconfigured) {
            setInputType(type);
            setImeOptions(options);
        }

        FrameLayout.LayoutParams place = new FrameLayout.LayoutParams(Math.max(1, value.optInt("width")), Math.max(1, value.optInt("height")));
        place.leftMargin = value.optInt("x");
        place.topMargin = value.optInt("y");
        setLayoutParams(place);

        // The keyboard opens when a field starts editing, so a field that only moves never brings back a keyboard the user closed.
        boolean starting = !editing || id != field;
        if (starting || nextRevision != revision) {
            field = id;
            revision = nextRevision;
            applying = true;
            String text = value.optString("text");
            setFilters(new InputFilter[0]);
            setText(text);
            setSelection(toUtf16(text, value.optInt("selectionStart")), toUtf16(text, value.optInt("selectionEnd")));
            applying = false;
        }

        // The limit holds what the user types, while the text the engine sets always fits it already.
        int maxLength = value.optInt("maxLength");
        setFilters(maxLength > 0 ? new InputFilter[] {new InputFilter.LengthFilter(maxLength)} : new InputFilter[0]);
        editing = true;
        setFocusable(true);
        setFocusableInTouchMode(true);
        if (!hasFocus()) {
            requestFocus();
        } else if (reconfigured) {
            methods.restartInput(this);
        }
        if (starting) {
            methods.showSoftInput(this, 0);
        }
    }

    private void stop() {
        editing = false;
        methods.hideSoftInputFromWindow(getWindowToken(), 0);
        setFocusable(false);
    }

    // The editing state goes to the engine once the input method finished its batch of changes, so text, selection and composing span always match.
    private void scheduleReport() {
        if (!editing || applying || reportPending) {
            return;
        }
        reportPending = true;
        post(this::report);
    }

    private void report() {
        reportPending = false;
        if (!editing) {
            return;
        }
        Editable editable = getText();
        String text = editable.toString();
        int start = BaseInputConnection.getComposingSpanStart(editable);
        int end = BaseInputConnection.getComposingSpanEnd(editable);
        boolean composing = start >= 0 && end > start;
        nativeTextEdited(field, revision, text.getBytes(StandardCharsets.UTF_8), toCodePoints(text, getSelectionStart()), toCodePoints(text, getSelectionEnd()), composing ? toCodePoints(text, start) : -1, composing ? toCodePoints(text, end) : -1);
    }

    // Hardware keyboards press tab to move on, escape to cancel and return to submit a single line.
    private boolean onEditKey(int code, KeyEvent event) {
        int action = -1;
        if (code == KeyEvent.KEYCODE_TAB) {
            action = ACTION_NEXT;
        } else if (code == KeyEvent.KEYCODE_ESCAPE) {
            action = ACTION_CANCEL;
        } else if ((code == KeyEvent.KEYCODE_ENTER || code == KeyEvent.KEYCODE_NUMPAD_ENTER) && !multiline) {
            action = ACTION_SUBMIT;
        }
        if (!editing || action < 0) {
            return false;
        }
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            nativeTextAction(field, action);
        }
        return true;
    }

    private static int inputType(int keyboard, int capitalization, boolean autocorrect) {
        int text = InputType.TYPE_CLASS_TEXT | capitalization | (autocorrect ? InputType.TYPE_TEXT_FLAG_AUTO_CORRECT : InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        switch (keyboard) {
            case KEYBOARD_MULTILINE:
                return text | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            case KEYBOARD_NUMBER:
                return InputType.TYPE_CLASS_NUMBER;
            case KEYBOARD_DECIMAL:
                return InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
            case KEYBOARD_PHONE:
                return InputType.TYPE_CLASS_PHONE;
            case KEYBOARD_EMAIL:
                return text | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
            case KEYBOARD_URL:
                return text | InputType.TYPE_TEXT_VARIATION_URI;
            case KEYBOARD_PASSWORD:
                return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
            default:
                return text;
        }
    }

    // The engine counts code points, while Java strings count characters outside the Basic Multilingual Plane twice.
    private static int toCodePoints(String text, int index) {
        return text.codePointCount(0, Math.max(0, Math.min(index, text.length())));
    }

    private static int toUtf16(String text, int codePoints) {
        int count = Math.max(0, Math.min(codePoints, text.codePointCount(0, text.length())));
        return text.offsetByCodePoints(0, count);
    }

    private static native void nativeTextEdited(long field, long revision, byte[] text, int selectionStart, int selectionEnd, int compositionStart, int compositionEnd);

    private static native void nativeTextAction(long field, int action);
}
