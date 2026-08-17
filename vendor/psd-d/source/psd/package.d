module psd;
import std.stdio;

public import psd.parser : parseDocument;
public import psd.layer;
public import psd.image_resources;

/**
    PSD Color Modes
*/
enum ColorMode : ushort {
    Bitmap,
    Grayscale,
    Indexed,
    RGB,
    CMYK,
    Multichannel,
    Duotone,
    Lab
}

/**
    A photoshop file
*/
struct PSD {
package(psd):
    size_t colorModeDataSectionOffset;
    size_t colorModeDataSectionLength;

    size_t imageResourceSectionOffset;
    size_t imageResourceSectionLength;

    size_t layerMaskInfoSectionOffset;
    size_t layerMaskInfoSectionLength;

    size_t imageDataSectionOffset;
    size_t imageDataSectionLength;

public:

    /**
        Amount of channels in file
    */
    short channels;

    /**
        Width of document
    */
    int width;

    /**
        Height of document
    */
    int height;

    /**
        Bits per channel
    */
    ushort bitsPerChannel;

    /**
        Color mode of document
    */
    ColorMode colorMode;

    /**
        Data for color mode
    */
    ubyte[] colorData;

    /**
        Whether alpha is merged
    */
    bool mergedAlpha;

    /**
        Layers
    */
    Layer[] layers;
    
    /**
        ImageResourcesData
    */
    ImageResourcesData imageResourcesData;

    /**
        Full image data encoded as 8-bit RGBA
    */
    ubyte[] fullImage;
}