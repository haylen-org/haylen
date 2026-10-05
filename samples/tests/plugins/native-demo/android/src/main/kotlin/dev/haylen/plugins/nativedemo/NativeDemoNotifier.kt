package dev.haylen.plugins.nativedemo

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.SystemClock
import android.util.Log
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import dev.haylen.HaylenLinkActivity

// Posts the local notification of the demo when its alarm goes off, whether the app runs, waits in the background or was closed. Its tap starts `HaylenLinkActivity` of the `haylen-links` library, which hands it to the running app or starts the app with it, and the plugin sends it to the app as `notificationOpened`.
class NativeDemoNotifier : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val manager = NotificationManagerCompat.from(context)
        if (!manager.areNotificationsEnabled()) {
            Log.w(TAG, "The notification of the native demo was dropped, because the person turned the notifications of the app off.")
            return
        }
        manager.createNotificationChannel(NotificationChannelCompat.Builder(CHANNEL, NotificationManagerCompat.IMPORTANCE_HIGH).setName("Native Demo").build())
        val identifier = intent.getStringExtra(IDENTIFIER) ?: return
        val title = intent.getStringExtra(TITLE)
        val open = Intent(context, HaylenLinkActivity::class.java).setAction(ACTION_OPENED).putExtra(IDENTIFIER, identifier).putExtra(TITLE, title).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        val tap = PendingIntent.getActivity(context, identifier.hashCode(), open, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val notification = NotificationCompat.Builder(context, CHANNEL).setSmallIcon(android.R.drawable.ic_dialog_info).setContentTitle(title).setContentText(intent.getStringExtra(BODY)).setContentIntent(tap).setAutoCancel(true).build()
        try {
            manager.notify(identifier, 0, notification)
        } catch (denied: SecurityException) {
            Log.w(TAG, "The notification of the native demo was dropped, because the app lacks the permission \"android.permission.POST_NOTIFICATIONS\".", denied)
        }
    }

    companion object {
        // The action of the intent that the tap on a notification of the demo starts.
        const val ACTION_OPENED = "dev.haylen.plugins.nativedemo.NOTIFICATION_OPENED"
        const val IDENTIFIER = "dev.haylen.plugins.nativedemo.IDENTIFIER"
        const val TITLE = "dev.haylen.plugins.nativedemo.TITLE"
        const val BODY = "dev.haylen.plugins.nativedemo.BODY"
        private const val CHANNEL = "native-demo"
        private const val TAG = "native-demo"

        // The alarm wakes the process when it waits in the background or starts it when it was closed, which a timer of the process could not. It goes off inexactly, which for a delay of a few seconds is at once, and needs no permission for exact alarms.
        fun schedule(context: Context, identifier: String, title: String, body: String, seconds: Double) {
            val post = Intent(context, NativeDemoNotifier::class.java).putExtra(IDENTIFIER, identifier).putExtra(TITLE, title).putExtra(BODY, body)
            val alarm = PendingIntent.getBroadcast(context, identifier.hashCode(), post, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
            context.getSystemService(AlarmManager::class.java).set(AlarmManager.ELAPSED_REALTIME_WAKEUP, SystemClock.elapsedRealtime() + (seconds * 1000).toLong(), alarm)
        }
    }
}
