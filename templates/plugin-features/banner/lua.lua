-- Shows a native bar over the app with a text and a button at its top or bottom edge. The table `options` takes `text`, `anchor`, `'top'` or `'bottom'`, and `reserve`, which moves the safe area of the app out of the way of the bar.
function plugin.showBanner(options)
    options = options or {}
    return handle:call('showBanner', {text = options.text or '', anchor = options.anchor or 'bottom', reserve = options.reserve == true})
end

function plugin.hideBanner()
    return handle:call('hideBanner', {})
end

-- Calls `listener({count = count})` whenever the person presses the button of the banner, and returns the connection that stops it.
function plugin.onBannerTapped(listener)
    return handle:on('bannerTapped', listener)
end
