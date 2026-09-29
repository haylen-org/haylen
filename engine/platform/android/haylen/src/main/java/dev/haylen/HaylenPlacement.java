package dev.haylen;

// Where HaylenOverlay places a view over the app. The fields match the placements of the web overlay, in dp instead of page pixels.
public final class HaylenPlacement {
    // The edges or the corner the view sits at, or the center. The view is centered along every axis that its anchor leaves free.
    public enum Anchor {
        TOP(0, -1),
        BOTTOM(0, 1),
        LEFT(-1, 0),
        RIGHT(1, 0),
        TOP_LEFT(-1, -1),
        TOP_RIGHT(1, -1),
        BOTTOM_LEFT(-1, 1),
        BOTTOM_RIGHT(1, 1),
        CENTER(0, 0);

        // The side of each axis the view sits at: -1 for the left or top edge, 1 for the right or bottom edge and 0 for the middle.
        final int horizontal;
        final int vertical;

        Anchor(int horizontal, int vertical) {
            this.horizontal = horizontal;
            this.vertical = vertical;
        }
    }

    // A width or height that the view keeps from its own layout parameters, or measures when it has none.
    public static final int MEASURED = -1;

    public Anchor anchor = Anchor.BOTTOM;
    // The distance from the anchored edges.
    public float marginDp;
    // Whether the view stays inside the safe area of the device, clear of cutouts and visible system bars.
    public boolean insideSafeArea = true;
    // Whether the view reserves the edge its anchor names, from the edge of the window to its far side, which widens the safe area of the app by it. A centered view reserves nothing.
    public boolean reserve;
    public int widthDp = MEASURED;
    public int heightDp = MEASURED;

    public HaylenPlacement() {}

    public HaylenPlacement(Anchor anchor) {
        this.anchor = anchor;
    }

    HaylenPlacement copy() {
        HaylenPlacement copy = new HaylenPlacement(anchor);
        copy.marginDp = marginDp;
        copy.insideSafeArea = insideSafeArea;
        copy.reserve = reserve;
        copy.widthDp = widthDp;
        copy.heightDp = heightDp;
        return copy;
    }
}
