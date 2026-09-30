package dev.haylen;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

// The JSON and the byte buffers that cross the bridge together. A byte[] or a ByteBuffer anywhere inside a value becomes a buffer that the JSON refers to as {"$bytes": N}, so binary data never turns into text.
final class HaylenPayload {
    static final Object[] NO_BUFFERS = new Object[0];

    private static final String BYTES_KEY = "$bytes";

    // The JSON as UTF-8 bytes, and every buffer as a byte[] or a direct ByteBuffer that JNI reads without a copy in Java.
    final byte[] json;
    final Object[] buffers;

    private HaylenPayload(byte[] json, Object[] buffers) {
        this.json = json;
        this.buffers = buffers;
    }

    // Accepts null, strings, numbers, booleans, JSONObject, JSONArray, maps, collections, arrays, byte[] and ByteBuffer. A ByteBuffer crosses with its remaining bytes.
    static HaylenPayload encode(Object value) throws JSONException {
        List<Object> buffers = new ArrayList<>();
        Object prepared = prepare(value, buffers);
        String json = prepared instanceof String ? JSONObject.quote((String) prepared) : prepared.toString();
        return new HaylenPayload(json.getBytes(StandardCharsets.UTF_8), buffers.toArray());
    }

    // Parses JSON text in which every {"$bytes": N} becomes buffer N, so objects arrive as JSONObject with byte[] values where the app sent bytes.
    static Object decode(byte[] json, byte[][] buffers) throws JSONException {
        return restore(new JSONTokener(new String(json, StandardCharsets.UTF_8)).nextValue(), buffers);
    }

    private static Object prepare(Object value, List<Object> buffers) throws JSONException {
        if (value instanceof byte[]) {
            return reference(value, buffers);
        }
        if (value instanceof ByteBuffer) {
            ByteBuffer bytes = (ByteBuffer) value;
            if (bytes.isDirect()) {
                return reference(bytes.slice(), buffers);
            }
            byte[] copy = new byte[bytes.remaining()];
            bytes.duplicate().get(copy);
            return reference(copy, buffers);
        }
        if (value instanceof JSONObject) {
            JSONObject source = (JSONObject) value;
            JSONObject object = new JSONObject();
            for (Iterator<String> keys = source.keys(); keys.hasNext();) {
                String key = keys.next();
                object.put(key, prepare(source.opt(key), buffers));
            }
            return object;
        }
        if (value instanceof Map) {
            JSONObject object = new JSONObject();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                object.put(String.valueOf(entry.getKey()), prepare(entry.getValue(), buffers));
            }
            return object;
        }
        if (value instanceof JSONArray) {
            JSONArray source = (JSONArray) value;
            JSONArray array = new JSONArray();
            for (int index = 0; index < source.length(); ++index) {
                array.put(prepare(source.opt(index), buffers));
            }
            return array;
        }
        if (value instanceof Collection) {
            JSONArray array = new JSONArray();
            for (Object element : (Collection<?>) value) {
                array.put(prepare(element, buffers));
            }
            return array;
        }
        if (value instanceof Object[]) {
            JSONArray array = new JSONArray();
            for (Object element : (Object[]) value) {
                array.put(prepare(element, buffers));
            }
            return array;
        }
        Object wrapped = JSONObject.wrap(value);
        return wrapped == null ? JSONObject.NULL : wrapped;
    }

    private static JSONObject reference(Object buffer, List<Object> buffers) throws JSONException {
        buffers.add(buffer);
        return new JSONObject().put(BYTES_KEY, buffers.size() - 1);
    }

    private static Object restore(Object value, byte[][] buffers) throws JSONException {
        if (value instanceof JSONArray) {
            JSONArray array = (JSONArray) value;
            for (int index = 0; index < array.length(); ++index) {
                array.put(index, restore(array.get(index), buffers));
            }
            return array;
        }
        if (!(value instanceof JSONObject)) {
            return value;
        }
        JSONObject object = (JSONObject) value;
        Object index = object.length() == 1 ? object.opt(BYTES_KEY) : null;
        if (index instanceof Integer || index instanceof Long) {
            long position = ((Number) index).longValue();
            if (position >= 0 && position < buffers.length) {
                return buffers[(int) position];
            }
        }
        List<String> keys = new ArrayList<>();
        for (Iterator<String> names = object.keys(); names.hasNext();) {
            keys.add(names.next());
        }
        for (String key : keys) {
            object.put(key, restore(object.get(key), buffers));
        }
        return object;
    }
}
