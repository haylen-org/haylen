import CoreGraphics
import Foundation

// The fake store, ads and sign-in of the demo, which stand in for the SDKs of real plugins with the same shape of calls, screens and events: a catalog, purchases that the person confirms in a native sheet with a receipt and a transaction that the device keeps, a full-screen ad, a rewarded ad and an account that signs in and out. A purchase, an ad and a sign-in take over the app until they end, so they are screens of the plugin, which the engine covers the app for. Nothing is charged and nothing leaves the device.
final class NativeDemoStore {
    struct Product: Encodable {
        let id: String
        let title: String
        let description: String
        let price: String
        let currency: String
        let kind: String
    }

    struct Purchase: Codable {
        let productId: String
        let transactionId: String
    }

    struct Purchased: Encodable {
        let productId: String
        let transactionId: String
        let receipt: String
        let language: String
    }

    struct Account: Codable {
        let userId: String
        let name: String
        let email: String
        var token: String
        var language: String
    }

    struct Buy: Decodable {
        let productId: String
    }

    struct Closed: Encodable {
        let closed: Bool
        let language: String
    }

    struct Reward: Encodable {
        let rewarded: Bool
        let amount: Int
        let currency: String
        let language: String
    }

    struct Empty: Codable {}

    static let products = [
        Product(id: "coins.small", title: "A pouch of coins", description: "100 coins for the shop of the app.", price: "0.99", currency: "USD", kind: "consumable"),
        Product(id: "coins.large", title: "A chest of coins", description: "1200 coins for the shop of the app.", price: "9.99", currency: "USD", kind: "consumable"),
        Product(id: "ads.remove", title: "No more ads", description: "Removes the ads of the demo for good.", price: "2.99", currency: "USD", kind: "nonConsumable"),
    ]

    private static let language = "Swift"
    private static let accounts = [
        Account(userId: "demo-ana", name: "Ana Souza", email: "ana@example.com", token: "", language: language),
        Account(userId: "demo-bruno", name: "Bruno Lima", email: "bruno@example.com", token: "", language: language),
    ]
    private static let purchasesKey = "native-demo.purchases"
    private static let accountKey = "native-demo.account"
    private let defaults = UserDefaults.standard

    func register(_ context: HaylenPluginContext) {
        context.register("products") { (_: Empty) async throws -> [Product] in
            Self.products
        }

        context.register("restorePurchases") { [unowned self] (_: Empty) async throws -> [Purchase] in
            let kept = self.kept()
            for purchase in kept {
                context.emit("purchaseUpdated", payload: ["productId": purchase.productId, "transactionId": purchase.transactionId, "state": "restored", "language": Self.language])
            }
            return kept
        }

        context.register("currentUser") { [unowned self] (_: Empty) async throws -> Account? in
            self.defaults.data(forKey: Self.accountKey).flatMap { try? JSONDecoder().decode(Account.self, from: $0) }
        }

        context.register("signOut") { [unowned self] (_: Empty) async throws -> Empty in
            self.defaults.removeObject(forKey: Self.accountKey)
            context.emit("userChanged", payload: nil)
            return Empty()
        }

        registerPurchase(context)
        registerAds(context)
        registerSignIn(context)
    }

    private func registerPurchase(_ context: HaylenPluginContext) {
        context.registerScreen("purchase") { [unowned self] (params: Buy, screen: HaylenScreen) in
            guard let product = Self.products.first(where: { $0.id == params.productId }) else {
                throw HaylenFailure("The store has no product \"\(params.productId)\".", code: "unknownProduct")
            }
            let choices = [NativeDemoSheet.Choice(id: "buy", title: "Buy for \(product.price) \(product.currency)"), NativeDemoSheet.Choice(id: "cancel", title: "Cancel", cancels: true)]
            NativeDemoSheet.ask(product.title, message: product.description + " This purchase is a simulation, and nothing is charged.", choices: choices, in: screen) { picked in
                guard picked != nil else {
                    screen.fail("The person cancelled the purchase.", code: "cancelled", data: nil)
                    return
                }
                let purchase = Purchase(productId: product.id, transactionId: UUID().uuidString)
                if product.kind == "nonConsumable" {
                    self.keep(purchase)
                }
                let receipt = Data("{\"productId\":\"\(purchase.productId)\",\"transactionId\":\"\(purchase.transactionId)\"}".utf8).base64EncodedString()
                context.emit("purchaseUpdated", payload: ["productId": purchase.productId, "transactionId": purchase.transactionId, "state": "purchased", "language": Self.language])
                try? screen.finish(encoding: Purchased(productId: purchase.productId, transactionId: purchase.transactionId, receipt: receipt, language: Self.language))
            }
        }
    }

    // The full-screen ad is the native screen of the demo in other colors, and the rewarded ad asks whether the person watches it to the end.
    private func registerAds(_ context: HaylenPluginContext) {
        context.registerScreen("interstitial") { (_: Empty, screen: HaylenScreen) in
            let color = CGColor(srgbRed: 0.42, green: 0.17, blue: 0.52, alpha: 1)
            let detail = "A fake full-screen ad of the demo plugin, which covers the app until it closes."
            let ad = NativeDemoScreen.controller(title: "Demo ad", detail: detail, color: color, action: "Continue to the app") {
                try? screen.finish(encoding: Closed(closed: true, language: Self.language))
            }
            #if os(macOS)
            screen.presentSheet(ad)
            #else
            screen.present(ad)
            #endif
        }

        context.registerScreen("rewarded") { (_: Empty, screen: HaylenScreen) in
            let choices = [NativeDemoSheet.Choice(id: "watch", title: "Watch to the end"), NativeDemoSheet.Choice(id: "skip", title: "Skip", cancels: true)]
            NativeDemoSheet.ask("Demo rewarded ad", message: "Watch this fake ad to the end to earn 50 coins.", choices: choices, in: screen) { picked in
                let rewarded = picked != nil
                if rewarded {
                    context.emit("adRewarded", payload: ["amount": 50, "currency": "coins", "language": Self.language])
                }
                try? screen.finish(encoding: Reward(rewarded: rewarded, amount: rewarded ? 50 : 0, currency: "coins", language: Self.language))
            }
        }
    }

    private func registerSignIn(_ context: HaylenPluginContext) {
        context.registerScreen("signIn") { [unowned self] (_: Empty, screen: HaylenScreen) in
            var choices = Self.accounts.map { NativeDemoSheet.Choice(id: $0.userId, title: "\($0.name), \($0.email)") }
            choices.append(NativeDemoSheet.Choice(id: "cancel", title: "Cancel", cancels: true))
            NativeDemoSheet.ask("Sign in to the demo", message: "Pick a fake account. No password and no network take part.", choices: choices, in: screen) { picked in
                guard var account = Self.accounts.first(where: { $0.userId == picked }) else {
                    screen.fail("The person cancelled the sign-in.", code: "cancelled", data: nil)
                    return
                }
                account.token = UUID().uuidString
                let encoded = try? JSONEncoder().encode(account)
                self.defaults.set(encoded, forKey: Self.accountKey)
                context.emit("userChanged", payload: encoded.flatMap { try? JSONSerialization.jsonObject(with: $0) })
                try? screen.finish(encoding: account)
            }
        }
    }

    private func kept() -> [Purchase] {
        defaults.data(forKey: Self.purchasesKey).flatMap { try? JSONDecoder().decode([Purchase].self, from: $0) } ?? []
    }

    private func keep(_ purchase: Purchase) {
        var purchases = kept().filter { $0.productId != purchase.productId }
        purchases.append(purchase)
        defaults.set(try? JSONEncoder().encode(purchases), forKey: Self.purchasesKey)
    }
}
