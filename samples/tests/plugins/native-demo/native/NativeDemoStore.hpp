#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The fake store and the fake account of the demo, which keep for the life of the process the transaction of each product that stays bought and the token of the account that signed in, and what the screen that shows confirms. Every method may run on any thread.
class NativeDemoStore final {
  public:
    // A product of the fake store, which every native part of the plugin lists the same way.
    struct Product {
        std::string_view id;
        std::string_view title;
        std::string_view description;
        std::string_view price;
        bool keeps;
    };

    struct Purchase {
        std::string_view productId;
        std::string transactionId;
    };

    static constexpr std::array<Product, 3> kProducts{{
        {"coins.small", "A pouch of coins", "100 coins for the shop of the app.", "0.99", false},
        {"coins.large", "A chest of coins", "1200 coins for the shop of the app.", "9.99", false},
        {"ads.remove", "No more ads", "Removes the ads of the demo for good.", "2.99", true},
    }};

    [[nodiscard]] static const Product* findProduct(std::string_view id);

    // A new transaction id, which tells the purchases and the sign-ins of the process apart.
    [[nodiscard]] std::string newTransaction();

    // Remembers the purchase that the screen that shows asks the person to confirm.
    void beginPurchase(const Product& product, std::string transactionId);

    // Ends the purchase that the person confirmed, and keeps it when the product stays bought.
    [[nodiscard]] Purchase completePurchase();

    // The purchases of the products that stay bought.
    [[nodiscard]] std::vector<Purchase> getKeptPurchases();

    // Remembers the token of the sign-in that the screen that shows asks the person to confirm.
    void beginSignIn(std::string value);

    // Signs the account in with the token of the sign-in that the person confirmed, and returns the token.
    [[nodiscard]] std::string completeSignIn();

    // The token of the account that signed in, or nothing while no account is in.
    [[nodiscard]] std::optional<std::string> getToken();

    void signOut();

  private:
    std::mutex lock;
    std::array<std::string, kProducts.size()> kept;
    std::string token;
    const Product* pendingProduct = &kProducts[0];
    std::string pendingTransaction;
    std::string pendingToken;
    unsigned serial = 0;
};
