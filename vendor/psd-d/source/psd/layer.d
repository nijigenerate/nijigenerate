module psd.layer;
import psd.parser;
import std.stdio : File;

/**
    Photoshop blending modes
*/
enum BlendingMode : string {
    PassThrough = "pass",
    Normal = "norm",
    Dissolve = "diss",
    Darken = "dark",
    Multiply = "mul ",
    ColorBurn = "idiv",
    LinearBurn = "lbrn",
    DarkerColor = "dkCl",
    Lighten = "lite",
    Screen = "scrn",
    ColorDodge = "div ",
    LinearDodge = "lddg",
    LighterColor = "lgCl",
    Overlay = "over",
    SoftLight = "sLit",
    HardLight = "hLit",
    VividLight = "vLit",
    LinearLight = "lLit",
    PinLight = "pLit",
    HardMix = "hMix",
    Difference = "diff",
    Exclusion = "smud",
    Subtract = "fsub",
    Divide = "fdiv",
    Hue = "hue ",
    Saturation = "sat ",
    Color = "colr",
    Luminosity = "lum "
}

/**
    A struct representing a layer mask as stored in the layers of the Layer Mask section.
*/
struct LayerMask
{
	/**
        Top coordinate of the rectangle that encloses the mask.
    */
    int top;

	/**
        Left coordinate of the rectangle that encloses the mask.
    */
    int left;

	/**
        Bottom coordinate of the rectangle that encloses the mask.
    */
    int bottom;

	/**
        Right coordinate of the rectangle that encloses the mask.
    */
    int right;


	/**
        The offset from the start of the file where the channel's data is stored.
    */
    ulong fileOffset;


	/**
        Planar data, having a size of (right-left)*(bottom-top)*bytesPerPixel.
    */
    ubyte[] data;


	/**
        The mask's feather value.
    */
    double feather;

	/**
        The mask's density value.
    */
    ubyte density;

	/**
        The mask's default color regions outside the enclosing rectangle.
    */
    ubyte defaultColor;
};

/**
    A struct representing a vector mask as stored in the layers of the Layer Mask section.
*/
struct VectorMask
{
	/**
        Top coordinate of the rectangle that encloses the mask.
    */
    int top;

	/**
        Left coordinate of the rectangle that encloses the mask.
    */
    int left;

	/**
        Bottom coordinate of the rectangle that encloses the mask.
    */
    int bottom;

	/**
        Right coordinate of the rectangle that encloses the mask.
    */
    int right;


	/**
        The offset from the start of the file where the channel's data is stored.
    */
    ulong fileOffset;


	/**
        Planar data, having a size of (right-left)*(bottom-top)*bytesPerPixel.
    */
    ubyte[] data;


	/**
        The mask's feather value.
    */
    double feather;

	/**
        The mask's density value.
    */
    ubyte density;

	/**
        The mask's default color regions outside the enclosing rectangle.
    */
    ubyte defaultColor;
};

/**
    Information about Masks
*/
struct MaskData
{
    /**
        Top X coordinate of mask
    */
    int top;

    /**
        Left X coordinate of mask
    */
    int left;

    /**
        Bottom Y coordinate of mask
    */
    int bottom;

    /**
        Right X coordinate of mask
    */
    int right;
    
    /**
        Default color of mask
    */
    ubyte defaultColor;
    
    /**
        If the mask is a vector mask or not
    */
    bool isVectorMask;
};

/**
    Type of channel.
*/
enum ChannelType
{
    INVALID = 32767,					///< Internal value. Used to denote that a channel no longer holds valid data.

    R = 0,								///< Type denoting the R channel, not necessarily the first in a RGB Color Mode document.
    G = 1,								///< Type denoting the G channel, not necessarily the second in a RGB Color Mode document.
    B = 2,								///< Type denoting the B channel, not necessarily the third in a RGB Color Mode document.

    TRANSPARENCY_MASK = -1,				///< The layer's channel data is a transparency mask.
    LAYER_OR_VECTOR_MASK = -2,			///< The layer's channel data is either a layer or vector mask.
    LAYER_MASK = -3						///< The layer's channel data is a layer mask.
}

/**
    Information about color channels
*/
struct ChannelInfo {
    /**
        Type of channel
    */
    short type;

    /**
        Offset into the file of the color channel
    */
    uint fileOffset;

    /**
        Length of data in color channel
    */
    uint dataLength;
    
    /**
        The data of the channel
    */
    ubyte[] data;

    /**
        Gets whether the channel is a mask
    */
    bool isMask() {
        return type < 0;
    }
}

/**
    The different types of layer
*/
enum LayerType {
    /**
        Any other type of layer
    */
    Any = 0,

    /**
        An open folder
    */
    OpenFolder = 1,

    /**
        A closed folder
    */
    ClosedFolder = 2,

    /**
        A bounding section divider
    
        Hidden in the UI
    */
    SectionDivider = 3
}

/**
    Flags for a layer
*/
enum LayerFlags : ubyte {
    TransProtect    = 0b00000001,
    Visible         = 0b00000010,
    Obsolete        = 0b00000100,
    ModernDoc       = 0b00001000,
    PixelIrrel      = 0b00010000,

    /**
        Special mask used for getting whether a layer is a group layer
        flags & GroupMask = 24, for a layer group.
    */
    GroupMask       = 0b00011000
}

/**
    A layer
*/
struct Layer {
package(psd):
    File filePtr;

public:

    /**
        Parent of layer
    */
    //Layer* parent;

    /**
        Name of layer
    */
    string name;

    /**
        Bounding box for layer
    */
    union {
        struct {

            /**
                Top X coordinate of layer
            */
            int top;

            /**
                Left X coordinate of layer
            */
            int left;

            /**
                Bottom Y coordinate of layer
            */
            int bottom;

            /**
                Right X coordinate of layer
            */
            int right;
        }

        /**
            Bounds as array
        */
        int[4] bounds;
    }

    /**
        Blending mode
    */
    BlendingMode blendModeKey;

    /**
        Channels in layer
    */
    ChannelInfo[] channels;
    
    /**
        The layer's user mask, if any.
    */
	LayerMask[] layerMask;
    
    /**
        The layer's vector mask, if any.
    */
	VectorMask[] vectorMask;

    /**
        Opacity of the layer
    */
    ubyte opacity;

    /**
        Whether clipping is base or non-base
    */
    bool clipping;

    /**
        Flags for the layer
    */
    LayerFlags flags;

    /**
        The data of the layer
    */
    ubyte[] data;

    /**
        The type of layer
    */
    LayerType type;

    /**
        Whether the layer is visible or not
    */
    bool isVisible;

    /**
        Gets the center coordinates of the layer
    */
    uint[2] center() {
        return [
            left+(width/2),
            top+(height/2),
        ];
    }

    /**
        Gets the size of this layer
    */
    uint[2] size() {
        return [
            width,
            height
        ];
    }

    /**
        Width
    */
    uint width() {
        return right-left;
    }

    /**
        Height
    */
    uint height() {
        return bottom-top;
    }

    /**
        Returns true if the layer is a group
    */
    bool isLayerGroup() {
        return type == LayerType.OpenFolder || type == LayerType.ClosedFolder;
    }

    /**
        Is the layer useful?
    */
    bool isLayerUseful() {
        return !isLayerGroup() && (width != 0 && height != 0);
    }

    /**
        Length of data
    */
    size_t dataLengthUncompressed() {
        return this.area()*channels.length;
    }

    /**
        Gets total data count
    */
    size_t totalDataCount() {
        uint length;
        foreach(channel; channels) length += channel.dataLength;
        return length;
    }

    /**
        Area of the layer
    */
    size_t area() {
        return width * height;
    }

    /**
        Extracts the layer image
    */
    void extractLayerImage() {
        extractLayer(this);
    }
}

/**
    A layer mask section
*/
struct LayerMaskSection {

    /**
        The layers in the section
    */
    Layer[] layers;

    /**
        The amount of layers in the section
    */
    uint layerCount = 0;

    /**
        The colorspace of the overlay (unused)
    */
    ushort overlayColorSpace = 0;
    
    /**
        The global opacity level (0 = transparent, 100 = opaque)
    */
    ushort opacity = 0;
    
    /**
        The global layer kind
    */
    ubyte kind = 128u;

    /**
        Whether the layer data contains a transparency mask
    */
    bool hasTransparencyMask = false;
}