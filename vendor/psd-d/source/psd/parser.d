/**
    This PSD file parser

    This is more or less based on psd_sdk

    Various features of PSD_SDK are left out for simplicity
*/
module psd.parser;
import utils.io;
import utils;
import psd;
import std.exception;
import std.format;
import std.string;
import psd.rle;

/**
    Parses document
*/
PSD parseDocument(string fileName) {
    auto file = File(fileName, "rb");
    return parseDocument(file);
}

/**
    Parses a Photoshop Document
*/
PSD parseDocument(ref File file) {
    PSD psd;
    file.seek(0); // Seek back to start of file, just in case.

    // First, parse the header section.
    parseHeader(file, psd);

    // Parse the various sections
    parseColorModeSection(file, psd);
    //parseImageResourceSection(file, psd);
    psd.imageResourceSectionOffset = file.tell();
    psd.imageResourceSectionLength = file.readValue!uint;
    file.skip(psd.imageResourceSectionLength);
    
    parseLayerMaskInfoSection(file, psd);
    parseImageDataSectionOffset(file, psd);

    return psd;
}



package(psd):

/**
    Finds the index of the channel
*/
uint findChannel(Layer* layer, ChannelType channelType)
{
	for (uint i = 0; i < layer.channels.length; ++i)
	{
		const ChannelInfo* channel = &layer.channels[i];
		if ((channel.dataLength > 0) && channel.type == channelType)
			return i;
	}

	return ChannelType.INVALID;
}

private ubyte[] channelData(Layer* layer, ChannelType channelType)
{
    auto index = findChannel(layer, channelType);
    if (index == ChannelType.INVALID)
        return null;
    return layer.channels[index].data;
}

private ubyte applyMask(ubyte alpha, ubyte mask)
{
    return cast(ubyte)((cast(uint)alpha * cast(uint)mask + 127u) / 255u);
}


void extractLayer(ref Layer layer) {
    auto file = layer.filePtr;

    // Skip empty layers
    if (layer.width == 0 && layer.height == 0) return;

    const size_t channelCount = layer.channels.length;
    //layer.data = new ubyte[layer.width*layer.height*channelCount];
    
    foreach(i; 0..channelCount) {
        ChannelInfo* channel = &layer.channels[i];
        file.seek(channel.fileOffset);

        // HACK: To allow transparency to be put as RGBA
        //       an offset is applied based on its layer type.
        size_t offset = i;
        if (channelCount > 3) {
            switch(channel.type) {
                case -1:
                    offset = 3;
                    break;
                default:
                    offset = channel.type;
                    break;
            }
        }

        const ushort compressionType = file.readValue!ushort;
        switch(compressionType) {
            //RAW
            case 0:
                channel.data = new ubyte[layer.width*layer.height];
                file.rawRead(channel.data);
                break;
            
            // RLE
            case 1:

                // RLE compressed data is preceded by a 2-byte data count for each scanline
                uint rleDataSize;
                foreach(_; 0..layer.height) {
                    const ushort dataCount = file.readValue!ushort;
                    rleDataSize += dataCount;
                }

                if (rleDataSize > 0) {

                    // Read planar data
                    ubyte[] rleData = new ubyte[rleDataSize];

                    // We need to work around the same D bug as before.
                    file.rawRead(rleData);

                    // Decompress RLE
                    // FIXME:  We're assuming psd.channelsPerBit == 8 right now, and that's not 
                    //         always the case.
                    channel.data = new ubyte[layer.width*layer.height];
                    decodeRLE(rleData, channel.data);
                }
                break;
            default: assert(0, "Unsupported compression type.");
        }

    }

    // Transcode to RGBA
    auto rgba = new ubyte[layer.width*layer.height*4];

    auto r = channelData(&layer, ChannelType.R);
    auto g = channelData(&layer, ChannelType.G);
    auto b = channelData(&layer, ChannelType.B);
    auto alpha = channelData(&layer, ChannelType.TRANSPARENCY_MASK);
    auto layerOrVectorMask = channelData(&layer, ChannelType.LAYER_OR_VECTOR_MASK);
    auto layerMask = channelData(&layer, ChannelType.LAYER_MASK);

    const size_t pixelCount = cast(size_t)layer.width * cast(size_t)layer.height;
    for (size_t i = 0, j = 0; i < pixelCount; ++i, j += 4) {
        rgba[j + 0] = r.length > i ? r[i] : 0;
        rgba[j + 1] = g.length > i ? g[i] : 0;
        rgba[j + 2] = b.length > i ? b[i] : 0;

        ubyte a = alpha.length > i ? alpha[i] : 255;
        if (layerOrVectorMask.length > i)
            a = applyMask(a, layerOrVectorMask[i]);
        if (layerMask.length > i)
            a = applyMask(a, layerMask[i]);
        rgba[j + 3] = a;
    }

    // 
    //foreach (size_t y; 0 .. layer.height)
    //{
    //    ubyte* a = layer.data.ptr + (y * layer.width * channelCount) + (layer.width * aI);
    //    ubyte* r = layer.data.ptr + (y * layer.width * channelCount) + (layer.width * rI);
    //    ubyte* g = layer.data.ptr + (y * layer.width * channelCount) + (layer.width * gI);
    //    ubyte* b = layer.data.ptr + (y * layer.width * channelCount) + (layer.width * bI);
    //    foreach (size_t x; 0 .. layer.width)
    //    {
    //        *rgbaPtr++ = *r++;
    //        *rgbaPtr++ = *g++;
    //        *rgbaPtr++ = *b++;
    //        *rgbaPtr++ = *a++;   
    //    }
    //}

    layer.data = rgba;
}

private:

/*
                                PSD HEADER
*/
void parseHeader(ref File file, ref PSD psd) {
    
    // Check signature
    {
        enforce(file.readStr(4) == "8BPS", "Invalid file, must be a Photoshop PSD file. PSB's are not supported!");
    }

    // Check version (must be 1)
    {
        enforce(file.readValue!ushort() == 1, "Version does not match 1.");
    }

    // Check reserve bytes
    {
        enforce(file.read(6) == [0, 0, 0, 0, 0, 0], "Unexpected reserve bytes, file may be corrupted.");
    }

    // Read number of channels
    // This is the number of channels contained in the document for all layers, including alpha channels.
    // e.g. for an RGB document with 3 alpha channels, this would be 3 (RGB) + 3 (Alpha) = 6 channels
    // however, note that the individual layers can have extra channels for transparency masks, vector masks, and user masks.
    // this is different from layer to layer.
    psd.channels = file.readValue!ushort;

    // Read rest of header info
    psd.height = file.readValue!uint;
    psd.width = file.readValue!uint;
    psd.bitsPerChannel = file.readValue!ushort;
    psd.colorMode = cast(ColorMode)file.readValue!ushort;
}




/*
                                COLOR MODE DATA
*/
void parseColorModeSection(ref File file, ref PSD psd) {
    psd.colorModeDataSectionLength = file.readValue!uint;
    psd.colorModeDataSectionOffset = file.tell();

    file.skip(psd.colorModeDataSectionLength);
}




/*
                                IMAGE RESOURCES
*/

/**
    Reads a padded value.
*/
pragma(inline, true)
T readPaddedValue(T)(ref File file, T multipleOf = 2, T addTo = 0) {
    T value = file.readValue!T;
    return cast(T)roundUpToMultiple(value + addTo, multipleOf);
}

void parseImageResourceSection(ref File file, ref PSD psd) {
    psd.imageResourceSectionLength = file.readValue!uint;
    psd.imageResourceSectionOffset = file.tell();

    // TODO: read
    ulong leftToRead = psd.imageResourceSectionLength;

    while (leftToRead > 0) {
        string signature = file.readStr(4);
        
        enforce(signature == "8BIM" || signature == "psdM", 
            "Image resources section seems to be corrupt, signature does not match \"8BIM\" nor \"psdM\".");

        const ushort id = file.readValue!ushort;

        const ubyte nameLength = readPaddedValue!ubyte(file, 2, 1);
        const string name = file.readStr(nameLength - 1);
        
        const uint resourceSize = readPaddedValue!uint(file);

        switch (id) {
			case ImageResourceType.IPTC_NAA:
			case ImageResourceType.CAPTION_DIGEST:
			case ImageResourceType.PRINT_INFORMATION:
			case ImageResourceType.PRINT_STYLE:
			case ImageResourceType.PRINT_SCALE:
			case ImageResourceType.PRINT_FLAGS:
			case ImageResourceType.PRINT_FLAGS_INFO:
			case ImageResourceType.PRINT_INFO:
			case ImageResourceType.RESOLUTION_INFO:
			case ImageResourceType.GLOBAL_ANGLE:
			case ImageResourceType.GLOBAL_ALTITUDE:
			case ImageResourceType.COLOR_HALFTONING_INFO:
			case ImageResourceType.COLOR_TRANSFER_FUNCTIONS:
			case ImageResourceType.MULTICHANNEL_HALFTONING_INFO:
			case ImageResourceType.MULTICHANNEL_TRANSFER_FUNCTIONS:
			case ImageResourceType.LAYER_STATE_INFORMATION:
			case ImageResourceType.LAYER_GROUP_INFORMATION:
			case ImageResourceType.LAYER_GROUP_ENABLED_ID:
			case ImageResourceType.LAYER_SELECTION_ID:
			case ImageResourceType.GRID_GUIDES_INFO:
			case ImageResourceType.URL_LIST:
			case ImageResourceType.SLICES:
			case ImageResourceType.PIXEL_ASPECT_RATIO:
			case ImageResourceType.ICC_UNTAGGED_PROFILE:
			case ImageResourceType.ID_SEED_NUMBER:
			case ImageResourceType.BACKGROUND_COLOR:
			case ImageResourceType.ALPHA_CHANNEL_UNICODE_NAMES:
			case ImageResourceType.ALPHA_IDENTIFIERS:
			case ImageResourceType.COPYRIGHT_FLAG:
			case ImageResourceType.PATH_SELECTION_STATE:
			case ImageResourceType.ONION_SKINS:
			case ImageResourceType.TIMELINE_INFO:
			case ImageResourceType.SHEET_DISCLOSURE:
			case ImageResourceType.WORKING_PATH:
			case ImageResourceType.MAC_PRINT_MANAGER_INFO:
			case ImageResourceType.WINDOWS_DEVMODE:
				// we are currently not interested in this resource type, skip it
			default:
				// this is a resource we know nothing about, so skip it
				file.skip(resourceSize);
				break;
			
			case ImageResourceType.DISPLAY_INFO:
			{
				// the display info resource stores color information and opacity for extra channels contained
				// in the document. these extra channels could be alpha/transparency, as well as spot color
				// channels used for printing.
			
				// check whether storage for alpha channels has been allocated yet
				// (ImageResourceType.ALPHA_CHANNEL_ASCII_NAMES stores the channel names)
				if (psd.imageResourcesData.alphaChannels.length == 0)
				{
					// note that this assumes RGB mode
					const uint channelCount = psd.channels - 3;
					psd.imageResourcesData.alphaChannelCount = channelCount;
					psd.imageResourcesData.alphaChannels.length = channelCount;
				}
			
				const uint versionNum = file.readValue!uint;
			
				for (uint i = 0u; i < psd.imageResourcesData.alphaChannelCount; ++i) {
                    AlphaChannel* channel = &psd.imageResourcesData.alphaChannels[i];
					channel.colorSpace = file.readValue!ushort;
					channel.color[0] = file.readValue!ushort;
					channel.color[1] = file.readValue!ushort;
					channel.color[2] = file.readValue!ushort;
					channel.color[3] = file.readValue!ushort;
					channel.opacity = file.readValue!ushort;
					channel.mode = cast(AlphaChannel.Mode)file.readValue!ubyte;
				}
			}
			break;
			
			case ImageResourceType.VERSION_INFO:
			{
				const uint versionNum = file.readValue!uint;
				const ubyte hasRealMergedData = file.readValue!ubyte;
				psd.imageResourcesData.containsRealMergedData = (hasRealMergedData != 0u);
				file.skip(resourceSize - 5u);
			}
			break;
			
			case ImageResourceType.THUMBNAIL_RESOURCE:
			{
				const uint format = file.readValue!uint;
				const uint width = file.readValue!uint;
				const uint height = file.readValue!uint;
				const uint widthInBytes = file.readValue!uint;
				const uint totalSize = file.readValue!uint;
				const uint binaryJpegSize = file.readValue!uint;
			
				const ushort bitsPerPixel = file.readValue!ushort;
				const ushort numberOfPlanes = file.readValue!ushort;
			
				psd.imageResourcesData.thumbnail.width = width;
				psd.imageResourcesData.thumbnail.height = height;
				psd.imageResourcesData.thumbnail.binaryJpegSize = binaryJpegSize;
				psd.imageResourcesData.thumbnail.binaryJpeg = file.rawRead(new ubyte[binaryJpegSize]);
				
				const uint bytesToSkip = resourceSize - 28u - binaryJpegSize;
				file.skip(bytesToSkip);
			}
			break;
			
			case ImageResourceType.XMP_METADATA:
			{
				// load the XMP metadata as raw data
                enforce(psd.imageResourcesData.xmpMetadata.length != 0, "File contains more than one XMP metadata resource.");
				psd.imageResourcesData.xmpMetadata = file.rawRead(new ubyte[resourceSize]);
			}
			break;
			
			case ImageResourceType.ICC_PROFILE:
			{
				// load the ICC profile as raw data
                enforce(psd.imageResourcesData.iccProfile.length != 0, "File contains more than one ICC profile.");
				psd.imageResourcesData.iccProfile = file.rawRead(new ubyte[resourceSize]);
				psd.imageResourcesData.sizeOfICCProfile = resourceSize;
			}
			break;
			
			case ImageResourceType.EXIF_DATA:
			{
				// load the EXIF data as raw data
                enforce(psd.imageResourcesData.exifData.length != 0, "File contains more than one EXIF data block.");
				psd.imageResourcesData.exifData = file.rawRead(new ubyte[resourceSize]);
				psd.imageResourcesData.sizeOfExifData = resourceSize;
			}
			break;
			
			case ImageResourceType.ALPHA_CHANNEL_ASCII_NAMES:
			{
				// check whether storage for alpha channels has been allocated yet
				// (ImageResourceType.DISPLAY_INFO stores the channel color data)
				if (psd.imageResourcesData.alphaChannels.length == 0)
				{
					// note that this assumes RGB mode
					const uint channelCount = psd.channels - 3;
					psd.imageResourcesData.alphaChannelCount = channelCount;
					psd.imageResourcesData.alphaChannels.length = channelCount;
				}
			
				// the names of the alpha channels are stored as a series of Pascal strings
				uint channel = 0;
				long remaining = resourceSize;
				while (remaining > 0) {
                    string channelName;
					const ubyte channelNameLength = file.readValue!ubyte;
					if (channelNameLength > 0) {
                        channelName = file.readStr(channelNameLength);
					}
			
					remaining -= 1 + channelNameLength;
			
					if (channel < psd.imageResourcesData.alphaChannelCount) {
						psd.imageResourcesData.alphaChannels[channel].asciiName = channelName;
						++channel;
					}
				}
			}
			break;
        }

		leftToRead -= 10 + nameLength + resourceSize;
    }
}






/*
                                LAYER MASK INFO
*/

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
long ReadMaskRectangle(ref File file, ref MaskData maskData)
{
    maskData.top = file.readValue!int;
    maskData.left = file.readValue!int;
    maskData.bottom = file.readValue!int;
    maskData.right = file.readValue!int;

    return 4u*int.sizeof;
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
long ReadMaskDensity(ref File file, ref ubyte density)
{
    density = file.readValue!ubyte;
    return ubyte.sizeof;
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
long ReadMaskFeather(ref File file, ref double feather)
{
    feather = file.readValue!double;
    return double.sizeof;
}


// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
long ReadMaskParameters(ref File file, ref ubyte layerDensity, ref double layerFeather, ref ubyte vectorDensity, ref double vectorFeather)
{
    long bytesRead = 0;

    const ubyte flags = file.readValue!ubyte;
    bytesRead += ubyte.sizeof;

    const bool hasUserDensity = (flags & (1u << 0)) != 0;
    const bool hasUserFeather = (flags & (1u << 1)) != 0;
    const bool hasVectorDensity = (flags & (1u << 2)) != 0;
    const bool hasVectorFeather = (flags & (1u << 3)) != 0;
    if (hasUserDensity)
    {
        bytesRead += ReadMaskDensity(file, layerDensity);
    }
    if (hasUserFeather)
    {
        bytesRead += ReadMaskFeather(file, layerFeather);
    }
    if (hasVectorDensity)
    {
        bytesRead += ReadMaskDensity(file, vectorDensity);
    }
    if (hasVectorFeather)
    {
        bytesRead += ReadMaskFeather(file, vectorFeather);
    }

    return bytesRead;
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
template ApplyMaskData(T)
{
    void ApplyMaskData(ref const MaskData maskData, double feather, ubyte density, T* layerMask)
    {
        layerMask.top = maskData.top;
        layerMask.left = maskData.left;
        layerMask.bottom = maskData.bottom;
        layerMask.right = maskData.right;
        layerMask.feather = feather;
        layerMask.density = density;
        layerMask.defaultColor = maskData.defaultColor;
    }
}














void parseLayerMaskInfoSection(ref File file, ref PSD psd) {
    psd.layerMaskInfoSectionLength = file.readValue!uint;
    psd.layerMaskInfoSectionOffset = file.tell();
    
    // Parse the length of the layer info section
    uint layerInfoSectionLength = file.readValue!uint;
    LayerMaskSection* layerMaskSection = parseLayer(file, psd, psd.layerMaskInfoSectionOffset, cast(uint)psd.layerMaskInfoSectionLength, cast(uint)layerInfoSectionLength);

    // TODO: Build hirearchy
    psd.layers = layerMaskSection.layers;
}

LayerMaskSection* parseLayer(ref File file, ref PSD psd, ulong sectionOffset, uint sectionLength, uint layerLength) {
    LayerMaskSection* layerMaskSection = new LayerMaskSection;

    if (layerLength != 0) {

        // Read the layer count. If it is a negative number, its absolute value is the number of the layers and the
        // first alpha channel contains the transparency data for the merged result.
        // this will also be reflected in the channelCount of the document.
        short layerCount = file.readValue!short;
        layerMaskSection.hasTransparencyMask = (layerCount < 0);
        if (layerCount < 0) layerCount *= -1;

        layerMaskSection.layerCount = cast(uint)layerCount;
        layerMaskSection.layers = new Layer[layerCount];

        foreach(i; 0..layerMaskSection.layers.length) {
            Layer layer;
            layer.filePtr = file;
            layer.type = LayerType.Any;

            layer.top = file.readValue!int;
            layer.left = file.readValue!int;

            // NOTE: It breaks here WTF???
            layer.bottom = file.readValue!int;
            layer.right = file.readValue!int;

            // Number of channels in the layer.
            // this includes channels for transparency, layer, and vector masks, if any.
            const ushort channelCount = file.readValue!ushort;
            layer.channels = new ChannelInfo[channelCount];

            foreach(j; 0..layer.channels.length) {
                ChannelInfo* channel = &layer.channels[j];
                channel.type = file.readValue!short;
                channel.dataLength = file.readValue!uint;
            }

            auto blendModeSignature = file.readStr(4);
            enforce(blendModeSignature == "8BIM", "Layer mask info section seems to be corrupt, signature does not match \"8BIM\". (was \"%s\")".format(blendModeSignature));

            layer.blendModeKey = cast(BlendingMode)file.readStr(4);
            layer.opacity = file.readValue!ubyte;
            layer.clipping = !file.readValue!bool;
            layer.flags = cast(LayerFlags)file.readValue!ubyte;

            file.skip(1);

            const uint extraDataLength = file.readValue!uint;
            const uint layerMaskDataLength = file.readValue!uint;
            
            // the layer mask data section is weird. it may contain extra data for masks, such as density and feather parameters.
            // there are 3 main possibilities:
            //	*) length == zero		.	skip this section
            //	*) length == [20, 28]	.	there is one mask, and that could be either a layer or vector mask.
            //								the mask flags give rise to mask parameters. they store the mask type, and additional parameters, if any.
            //								there might be some padding at the end of this section, and its size depends on which parameters are there.
            //	*) length == [36, 56]	.	there are two masks. the first mask has parameters, but does NOT store flags yet.
            //								instead, there comes a second section with the same info (flags, default color, rectangle), and
            //								the parameters follow after that. there is also padding at the end of this second section.
            if (layerMaskDataLength != 0)
            {
                // there can be at most two masks, one layer and one vector mask
                MaskData[2] maskData;
                uint maskCount = 1u;

                double layerFeather = 0.0;
                double vectorFeather = 0.0;
                ubyte layerDensity = 0;
                ubyte vectorDensity = 0;

                long toRead = layerMaskDataLength;

                // enclosing rectangle
                toRead -= ReadMaskRectangle(file, maskData[0]);

                maskData[0].defaultColor = file.readValue!ubyte;
                toRead -= ubyte.sizeof;

                const ubyte maskFlags = file.readValue!ubyte;
                toRead -= ubyte.sizeof;

                maskData[0].isVectorMask = (maskFlags & (1u << 3)) != 0;
                bool maskHasParameters = (maskFlags & (1u << 4)) != 0;
                if (maskHasParameters && (layerMaskDataLength <= 28))
                {
                    toRead -= ReadMaskParameters(file, layerDensity, layerFeather, vectorDensity, vectorFeather);
                }

                // check if there is enough data left for another section of mask data
                if (toRead >= 18)
                {
                    // in case there is still data left to read, the following values are for the real layer mask.
                    // the data we just read was for the vector mask.
                    maskCount = 2u;

                    const ubyte realFlags = file.readValue!ubyte;
                    toRead -= ubyte.sizeof;

                    maskData[1].defaultColor = file.readValue!ubyte;
                    toRead -= ubyte.sizeof;

                    toRead -= ReadMaskRectangle(file, maskData[1]);

                    maskData[1].isVectorMask = (realFlags & (1u << 3)) != 0;

                    // note the OR here. whether the following section has mask parameter data or not is influenced by
                    // the availability of parameter data of the previous mask!
                    maskHasParameters |= ((realFlags & (1u << 4)) != 0);
                    if (maskHasParameters)
                    {
                        toRead -= ReadMaskParameters(file, layerDensity, layerFeather, vectorDensity, vectorFeather);
                    }
                }

                // skip the remaining padding bytes, if any
                enforce(toRead >= 0, format("Parsing failed, #d bytes left", toRead));
                file.skip(cast(ulong)toRead);

                // apply mask data to our own data structures
                for (uint mask=0; mask < maskCount; ++mask)
                {
                    const bool isVectorMask = maskData[mask].isVectorMask;
                    if (isVectorMask)
                    {
                        enforce(layer.vectorMask == null, "A vector mask already exists.");
                        layer.vectorMask = new VectorMask[1];
                        layer.vectorMask[0].data = null;
                        layer.vectorMask[0].fileOffset = 0;
                        ApplyMaskData!VectorMask(maskData[mask], vectorFeather, vectorDensity, &layer.vectorMask[0]);
                    }
                    else
                    {
                        enforce(layer.layerMask == null, "A layer mask already exists.");
                        layer.layerMask = new LayerMask[1];
                        layer.layerMask[0].data = null;
                        layer.layerMask[0].fileOffset = 0;
                        ApplyMaskData!LayerMask(maskData[mask], layerFeather, layerDensity, &layer.layerMask[0]);
                    }
                }
            }
            
            // skip blending ranges data, we are not interested in that for now
            const uint layerBlendingRangesDataLength = file.readValue!uint;
            file.skip(layerBlendingRangesDataLength);

            // the layer name is stored as pascal string, padded to a multiple of 4
            // we peek here as the actual calculation happens inside readPascalStr
            // TODO: make this more pretty?
            const ubyte nameLength = file.peekValue!ubyte;
            const uint paddedNameLength = roundUpToMultiple(nameLength + 1u, 4u);

            layer.name = file.readPascalStr(paddedNameLength - 1u);

            // read Additional Layer Information that exists since Photoshop 4.0.
            // getting the size of this data is a bit awkward, because it's not stored explicitly somewhere. furthermore,
            // the PSD format sometimes includes the 4-byte length in its section size, and sometimes not.
            const uint additionalLayerInfoSize = extraDataLength - layerMaskDataLength - layerBlendingRangesDataLength - paddedNameLength - 8u;
            long toRead = additionalLayerInfoSize;

            while (toRead > 0)
            {
                const string signature = file.readStr(4);
                enforce(signature == "8BIM", "Additional Layer Information section seems to be corrupt, signature does not match \"8BIM\". (was \"%s\")".format(signature));

                const string key = file.readStr(4);

                // length needs to be rounded to an even number
                uint length = file.readValue!uint;
                length = roundUpToMultiple(length, 2u);

                // read "Section divider setting" to identify whether a layer is a group, or a section divider
                if (key == "lsct")
                {
                    layer.type = cast(LayerType)file.readValue!uint;

                    // skip the rest of the data
                    file.skip(length - 4u);
                }
                // read Unicode layer name
                else if (key == "luni")
                {
                    // PSD Unicode strings store 4 bytes for the number of characters, NOT bytes, followed by
                    // 2-byte UTF16 Unicode data without the terminating null.
                    const uint characterCountWithoutNull = file.readValue!uint;
                    wstring utf16Name;
                    for (uint c = 0u; c < characterCountWithoutNull; ++c)
                    {
                        utf16Name ~= cast(wchar)file.readValue!ushort;
                    }

                    // If there's a unicode name we may as well use that here.
                    import std.utf : toUTF8;
                    layer.name = utf16Name.toUTF8;

                    // Some PSD exporters throw an extra null in there for good measure, yeet it.
                    if (layer.name[$-1] == '\0') layer.name.length--;

                    // skip possible padding bytes
                    file.skip(length - 4u - characterCountWithoutNull * ushort.sizeof);
                }
                else
                {
                    file.skip(length);
                }

                toRead -= 3*uint.sizeof + length;
            }

            layerMaskSection.layers[i] = layer;
        }


        // walk through the layers and channels, but don't extract their data just yet. only save the file offset for extracting the
        // data later.
        foreach (i; 0..layerMaskSection.layers.length)
        {            
            Layer* layer = &layerMaskSection.layers[i];
            foreach(j; 0..layer.channels.length) {
                ChannelInfo* channel = &layer.channels[j];
                channel.fileOffset = cast(uint)file.tell();
                file.skip(channel.dataLength);
            }
        }
    }

    if (sectionLength > 0)
    {
        // start loading at the global layer mask info section, located after the Layer Information Section.
        // note that the 4 bytes that stored the length of the section are not included in the length itself.
        const ulong globalInfoSectionOffset = sectionOffset + layerLength + 4u;
        file.seek(globalInfoSectionOffset);

        // work out how many bytes are left to read at this point. we need that to figure out the size of the last
        // optional section, the Additional Layer Information.
        if (sectionOffset + sectionLength > globalInfoSectionOffset)
        {
            long toRead = cast(long)(sectionOffset + sectionLength - globalInfoSectionOffset);
            const uint globalLayerMaskLength = file.readValue!uint;
            toRead -= uint.sizeof;

            if (globalLayerMaskLength != 0)
            {
                layerMaskSection.overlayColorSpace = file.readValue!ushort;

                // 4*2 byte color components
                file.skip(8);

                layerMaskSection.opacity = file.readValue!ushort;
                layerMaskSection.kind = file.readValue!ubyte;

                toRead -= 2u*ushort.sizeof + ubyte.sizeof + 8u;

                // filler bytes (zeroes)
                const uint remaining = cast(uint)(globalLayerMaskLength - 2u*ushort.sizeof - ubyte.sizeof - 8u);
                file.skip(remaining);

                toRead -= remaining;
            }

            // are there still bytes left to read? then this is the Additional Layer Information that exists since Photoshop 4.0.
            while (toRead > 0)
            {
                const string signature = file.readStr(4);
                enforce(signature == "8BIM", "Additional Layer Information section seems to be corrupt, signature does not match \"8BIM\".");

                const string key = file.readStr(4);

                // again, length is rounded to a multiple of 4
                uint length = file.readValue!uint;
                length = roundUpToMultiple(length, 4u);

                if (key == "Lr16")
                {
                    const ulong offset = file.tell();
                    
                    // NOTE: I think in D we...can just let this get copied over?
                    //DestroyLayerMaskSection(layerMaskSection, allocator);
                    layerMaskSection = parseLayer(file, psd, 0u, 0u, length);
                    file.seek(offset + length);
                }
                else if (key == "Lr32")
                {
                    const ulong offset = file.tell();
                    
                    // NOTE: I think in D we...can just let this get copied over?
                    //DestroyLayerMaskSection(layerMaskSection, allocator);

                    layerMaskSection = parseLayer(file, psd, 0u, 0u, length);
                    file.seek(offset + length);
                }
                else if (key == "vmsk")
                {
                    // TODO: could read extra vector mask data here
                    file.skip(length);
                }
                else if (key == "lnk2")
                {
                    // TODO: could read individual smart object layer data here
                    file.skip(length);
                }
                else
                {
                    file.skip(length);
                }

                toRead -= 3u*uint.sizeof + length;
            }
        }
    }

    return layerMaskSection;
}


/*
                                IMAGE DATA
*/
void parseImageDataSectionOffset(ref File file, ref PSD psd) {
    psd.imageDataSectionOffset = file.tell();
    psd.imageDataSectionLength = file.size() - psd.imageDataSectionOffset;

    // TODO: read
}
