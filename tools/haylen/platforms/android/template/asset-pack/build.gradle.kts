// An asset pack of the protected release of the app, which Google Play delivers {{DELIVERY_WORDS}}.
plugins {
    id("com.android.asset-pack")
}

assetPack {
    packName.set({{NAME}})
    dynamicDelivery {
        deliveryType.set({{DELIVERY}})
    }
}
