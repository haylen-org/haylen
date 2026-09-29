package dev.haylen;

import android.app.Application;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;

// Loads the plugins of the app when its process starts. Android creates content providers before Application.onCreate, so plugins set up their SDKs before any app code runs. The provider shares nothing, and its queries answer nothing.
public final class HaylenPluginProvider extends ContentProvider {
    @Override
    public boolean onCreate() {
        HaylenPlugins.load((Application) getContext().getApplicationContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
