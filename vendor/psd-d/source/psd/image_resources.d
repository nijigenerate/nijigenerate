
//
//          IMAGE RESOURCES
//
module psd.image_resources;
import utils.io;
import utils;
import psd;
import std.exception;
import std.format;

/**
    An image resource block
*/
struct ImageResourceBlock {
    /**
        Unique ID
    */
    ushort uid;

    /**
        Name of resource
    */
    string name;

    /**
        Data of resource
    */
    ubyte[] data;
}

/**
    A struct representing a thumbnail as stored in the image resources section.
*/
struct Thumbnail
{
	uint width;
	uint height;
	uint binaryJpegSize;
	ubyte[] binaryJpeg;
}

/**
    A struct representing an alpha channel as stored in the image resources section.
    NOTE: Note that the image data for alpha channels is stored in the image data section.
*/
struct AlphaChannel
{
    enum Mode : ubyte
    {
        ALPHA = 0,			// The channel stores alpha data.
        INVERTED_ALPHA = 1,	// The channel stores inverted alpha data.
        SPOT = 2			// The channel stores spot color data.
    }

    /**
        The channel's ASCII name.
    */
    string asciiName;
	
    /**
        The color space the colors are stored in.
    */
    ushort colorSpace;
	
    /**
        16-bit color data with 0 being black and 65535 being white (assuming RGBA).
    */
    ushort[4] color;
	
    /**
        The channel's opacity in the range [0, 100].
    */
    ushort opacity;
	
    /**
        The channel's mode, one of AlphaChannel::Mode.
    */
    Mode mode;
}

/**
    A struct representing the information extracted from the Image Resources section.
*/
struct ImageResourcesData
{
	/**
        An array of alpha channels, having alphaChannelCount entries.
    */
    AlphaChannel[] alphaChannels;

	/**
        The number of alpha channels stored in the array.
    */
    uint alphaChannelCount;

	/**
        Raw data of the ICC profile.
    */
    ubyte[] iccProfile;
	uint sizeOfICCProfile;

	/**
        Raw EXIF data.
    */
    ubyte[] exifData;
	uint sizeOfExifData;

	/**
        Whether the PSD contains real merged data.
    */
    bool containsRealMergedData;

	/**
        Raw XMP metadata.
    */
    ubyte[] xmpMetadata;

	/**
        JPEG thumbnail.
    */
    Thumbnail thumbnail;
}


enum ImageResourceType
{
    IPTC_NAA = 1028,
    CAPTION_DIGEST = 1061,
    XMP_METADATA = 1060,
    PRINT_INFORMATION = 1082,
    PRINT_STYLE = 1083,
    PRINT_SCALE = 1062,
    PRINT_FLAGS = 1011,
    PRINT_FLAGS_INFO = 10000,
    PRINT_INFO = 1071,
    RESOLUTION_INFO = 1005,
    DISPLAY_INFO = 1077,
    GLOBAL_ANGLE = 1037,
    GLOBAL_ALTITUDE = 1049,
    COLOR_HALFTONING_INFO = 1013,
    COLOR_TRANSFER_FUNCTIONS = 1016,
    MULTICHANNEL_HALFTONING_INFO = 1012,
    MULTICHANNEL_TRANSFER_FUNCTIONS = 1015,
    LAYER_STATE_INFORMATION = 1024,
    LAYER_GROUP_INFORMATION = 1026,
    LAYER_GROUP_ENABLED_ID = 1072,
    LAYER_SELECTION_ID = 1069,
    GRID_GUIDES_INFO = 1032,
    URL_LIST = 1054,
    SLICES = 1050,
    PIXEL_ASPECT_RATIO = 1064,
    ICC_PROFILE = 1039,
    ICC_UNTAGGED_PROFILE = 1041,
    ID_SEED_NUMBER = 1044,
    THUMBNAIL_RESOURCE = 1036,
    VERSION_INFO = 1057,
    EXIF_DATA = 1058,
    BACKGROUND_COLOR = 1010,
    ALPHA_CHANNEL_ASCII_NAMES = 1006,
    ALPHA_CHANNEL_UNICODE_NAMES = 1045,
    ALPHA_IDENTIFIERS = 1053,
    COPYRIGHT_FLAG = 1034,
    PATH_SELECTION_STATE = 1088,
    ONION_SKINS = 1078,
    TIMELINE_INFO = 1075,
    SHEET_DISCLOSURE = 1076,
    WORKING_PATH = 1025,
    MAC_PRINT_MANAGER_INFO = 1001,
    WINDOWS_DEVMODE = 1085
}
