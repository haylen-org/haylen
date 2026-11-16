#include "NativeDemoStore.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <utility>

const NativeDemoStore::Product* NativeDemoStore::findProduct(std::string_view id) {
    const auto found = std::ranges::find(kProducts, id, &Product::id);
    return found != kProducts.end() ? &*found : nullptr;
}

std::string NativeDemoStore::newTransaction() {
    unsigned number = 0;
    {
        const std::scoped_lock guard(lock);
        number = ++serial;
    }
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return "Cpp-" + std::to_string(milliseconds) + "-" + std::to_string(number);
}

void NativeDemoStore::beginPurchase(const Product& product, std::string transactionId) {
    const std::scoped_lock guard(lock);
    pendingProduct = &product;
    pendingTransaction = std::move(transactionId);
}

NativeDemoStore::Purchase NativeDemoStore::completePurchase() {
    const std::scoped_lock guard(lock);
    const auto index = static_cast<std::size_t>(pendingProduct - kProducts.data());
    if (pendingProduct->keeps) {
        kept[index] = pendingTransaction;
    }
    return {pendingProduct->id, pendingTransaction};
}

std::vector<NativeDemoStore::Purchase> NativeDemoStore::getKeptPurchases() {
    const std::scoped_lock guard(lock);
    std::vector<Purchase> purchases;
    for (std::size_t index = 0; index < kProducts.size(); ++index) {
        if (!kept[index].empty()) {
            purchases.push_back({kProducts[index].id, kept[index]});
        }
    }
    return purchases;
}

void NativeDemoStore::beginSignIn(std::string value) {
    const std::scoped_lock guard(lock);
    pendingToken = std::move(value);
}

std::string NativeDemoStore::completeSignIn() {
    const std::scoped_lock guard(lock);
    token = pendingToken;
    return token;
}

std::optional<std::string> NativeDemoStore::getToken() {
    const std::scoped_lock guard(lock);
    if (token.empty()) {
        return std::nullopt;
    }
    return token;
}

void NativeDemoStore::signOut() {
    const std::scoped_lock guard(lock);
    token.clear();
}
