module nijigenerate.core.logo;

import nijilive;

private {

    Texture incLogoI2D;
    Texture incLogo;

    Texture incAda;
    Texture incGrid;


}


void incInitLogo() {
    // Load image resources
    auto tex = ShallowTexture(cast(ubyte[])import("logo.png"));
    inTexPremultiply(tex.data);
    incLogoI2D = new Texture(tex);

    // Load image resources
    tex = ShallowTexture(cast(ubyte[])import("icon.png"));
    inTexPremultiply(tex.data);
    incLogo = new Texture(tex);

    tex = ShallowTexture(cast(ubyte[])import("ui/ui-ada.png"));
    inTexPremultiply(tex.data);
    incAda = new Texture(tex);

    // Grid texture
    tex = ShallowTexture(cast(ubyte[])import("ui/grid.png"));
    inTexPremultiply(tex.data);
    incGrid = new Texture(tex);
    incGrid.setFiltering(Filtering.Point);
    incGrid.setWrapping(Wrapping.Repeat);

}

/**
    Gets the nijilive Logo
*/
Texture incGetLogo() {
    return incLogo;
}

Texture incGetLogoI2D() {
    return incLogoI2D;
}

/**
    Gets the Ada texture
*/
Texture incGetAda() {
    return incAda;
}


/**
    Gets the grid texture
*/
Texture incGetGrid() {
    return incGrid;
}
