package dev.haylen.plugins.nativedemo

import android.content.Context
import android.graphics.Color
import androidx.appcompat.app.AlertDialog
import dev.haylen.HaylenBridge
import dev.haylen.HaylenPluginContext
import dev.haylen.HaylenScreen
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject

// The fake store, ads and sign-in of the demo, which stand in for the SDKs of real plugins with the same shape of calls, screens and events: a catalog, purchases that the person confirms in a native dialog with a receipt and a transaction that the device keeps, a full-screen ad, a rewarded ad and an account that signs in and out. A purchase, an ad and a sign-in take over the app until they end, so they are screens of the plugin, which the engine covers the app for. Nothing is charged and nothing leaves the device.
class NativeDemoStore(private val context: HaylenPluginContext) {
    private val saved = context.application().getSharedPreferences("native-demo", Context.MODE_PRIVATE)

    fun register() {
        context.register("products") { _, reply ->
            val products = JSONArray()
            for (product in PRODUCTS) {
                products.put(JSONObject().put("id", product.id).put("title", product.title).put("description", product.description).put("price", product.price).put("currency", "USD").put("kind", if (product.keeps) "nonConsumable" else "consumable"))
            }
            reply.success(products)
        }

        context.register("restorePurchases") { _, reply ->
            val kept = kept()
            for (index in 0 until kept.length()) {
                val purchase = kept.getJSONObject(index)
                context.emit("purchaseUpdated", JSONObject(purchase.toString()).put("state", "restored").put("language", LANGUAGE))
            }
            reply.success(kept)
        }

        context.register("currentUser") { _, reply ->
            reply.success(saved.getString(ACCOUNT_KEY, null)?.let { JSONObject(it) })
        }

        context.register("signOut") { _, reply ->
            saved.edit().remove(ACCOUNT_KEY).apply()
            context.emit("userChanged", null)
            reply.success(null)
        }

        context.registerScreen("purchase", HaylenScreen.Opener { params, screen -> purchase((params as JSONObject).getString("productId"), screen) })
        context.registerScreen("interstitial", HaylenScreen.Opener { _, screen -> interstitial(screen) })
        context.registerScreen("rewarded", HaylenScreen.Opener { _, screen -> rewarded(screen) })
        context.registerScreen("signIn", HaylenScreen.Opener { _, screen -> signIn(screen) })
    }

    private fun purchase(productId: String, screen: HaylenScreen) {
        val product = PRODUCTS.firstOrNull { it.id == productId } ?: throw HaylenBridge.Failure("The store has no product \"$productId\".", "unknownProduct", null)
        ask(screen, product.title, product.description + " This purchase is a simulation, and nothing is charged.", listOf("Buy for ${product.price} USD"), "Cancel") { picked ->
            if (picked == null) {
                screen.fail("The person cancelled the purchase.", "cancelled", null)
                return@ask
            }
            val purchase = JSONObject().put("productId", product.id).put("transactionId", UUID.randomUUID().toString())
            if (product.keeps) {
                keep(purchase)
            }
            context.emit("purchaseUpdated", JSONObject(purchase.toString()).put("state", "purchased").put("language", LANGUAGE))
            val receipt = android.util.Base64.encodeToString(purchase.toString().toByteArray(), android.util.Base64.NO_WRAP)
            screen.finish(JSONObject(purchase.toString()).put("receipt", receipt).put("language", LANGUAGE))
        }
    }

    // The full-screen ad is the native screen of the demo in other colors, which ends the screen when the person closes it with its button or Back.
    private fun interstitial(screen: HaylenScreen) {
        val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
        val ad = NativeDemoScreen(activity, "Demo ad", Color.rgb(107, 43, 133), "A fake full-screen ad of the demo plugin, which covers the app until it closes.", "Continue to the app") {
            screen.finish(JSONObject().put("closed", true).put("language", LANGUAGE))
        }
        screen.onCancel { ad.dismiss() }
        ad.show()
    }

    private fun rewarded(screen: HaylenScreen) {
        ask(screen, "Demo rewarded ad", "Watch this fake ad to the end to earn 50 coins.", listOf("Watch to the end"), "Skip") { picked ->
            val rewarded = picked != null
            if (rewarded) {
                context.emit("adRewarded", JSONObject().put("amount", 50).put("currency", "coins").put("language", LANGUAGE))
            }
            screen.finish(JSONObject().put("rewarded", rewarded).put("amount", if (rewarded) 50 else 0).put("currency", "coins").put("language", LANGUAGE))
        }
    }

    private fun signIn(screen: HaylenScreen) {
        ask(screen, "Sign in to the demo", "Pick a fake account. No password and no network take part.", ACCOUNTS.map { "${it.getString("name")}, ${it.getString("email")}" }, "Cancel") { picked ->
            if (picked == null) {
                screen.fail("The person cancelled the sign-in.", "cancelled", null)
                return@ask
            }
            val account = JSONObject(ACCOUNTS[picked].toString()).put("token", UUID.randomUUID().toString()).put("language", LANGUAGE)
            saved.edit().putString(ACCOUNT_KEY, account.toString()).apply()
            context.emit("userChanged", account)
            screen.finish(account)
        }
    }

    // Shows a dialog over the activity of the app with the choices and the choice that cancels, and calls `picked` once with the index of the choice or nothing for the one that cancels, Back included. A screen that the app gives up closes the dialog and ends as `cancelled`.
    private fun ask(screen: HaylenScreen, title: String, message: String, choices: List<String>, cancel: String, picked: (Int?) -> Unit) {
        val activity = context.activity() ?: throw HaylenBridge.Failure("The app has no activity yet.", "noWindow", null)
        var answered = false
        val answer = { choice: Int? ->
            if (!answered) {
                answered = true
                picked(choice)
            }
        }
        val builder = AlertDialog.Builder(activity).setTitle(title).setNegativeButton(cancel) { _, _ -> answer(null) }.setOnDismissListener { answer(null) }
        if (choices.size == 1) {
            builder.setMessage(message).setPositiveButton(choices[0]) { _, _ -> answer(0) }
        } else {
            builder.setItems(choices.toTypedArray()) { _, which -> answer(which) }
        }
        val dialog = builder.show()
        screen.onCancel {
            answered = true
            dialog.dismiss()
            screen.fail("The app gave the screen up.", "cancelled", null)
        }
    }

    private fun kept(): JSONArray = JSONArray(saved.getString(PURCHASES_KEY, "[]"))

    private fun keep(purchase: JSONObject) {
        val kept = kept()
        val next = JSONArray()
        for (index in 0 until kept.length()) {
            if (kept.getJSONObject(index).getString("productId") != purchase.getString("productId")) {
                next.put(kept.getJSONObject(index))
            }
        }
        saved.edit().putString(PURCHASES_KEY, next.put(purchase).toString()).apply()
    }

    private class Product(val id: String, val title: String, val description: String, val price: String, val keeps: Boolean)

    private companion object {
        const val LANGUAGE = "Kotlin"
        const val PURCHASES_KEY = "purchases"
        const val ACCOUNT_KEY = "account"
        val PRODUCTS = listOf(
            Product("coins.small", "A pouch of coins", "100 coins for the shop of the app.", "0.99", false),
            Product("coins.large", "A chest of coins", "1200 coins for the shop of the app.", "9.99", false),
            Product("ads.remove", "No more ads", "Removes the ads of the demo for good.", "2.99", true),
        )
        val ACCOUNTS = listOf(
            JSONObject().put("userId", "demo-ana").put("name", "Ana Souza").put("email", "ana@example.com"),
            JSONObject().put("userId", "demo-bruno").put("name", "Bruno Lima").put("email", "bruno@example.com"),
        )
    }
}
