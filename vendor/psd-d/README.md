# PSD-D
psd-d is a lose experimental port of psd_sdk to D to support basic extraction of layer info and layer data from photoshop files.
Some features are not supported (yet), but we'll add them over time.

Only the PSD format is supported, there's currently no support for PSB.

# Parsing a document
To parse a PSD document, use `parseDocument` in `psd`.
```d
PSD document = parseDocument("myFile.psd");
```

# Extracting layer data from layer
To extract layer data (textures) from a layer use `Layer.extractLayerImage()`
```d
PSD doc = parseDocument("myfile.psd");
foreach(layer; doc.layers) {
    
    // Skip non-image layers
    if (layer.type != LayerType.Any) continue;

    // Extract the layer image data.
    // The output RGBA output is stored in Layer.data
    layer.extractLayerImage();

    // write_image from imagefmt is used here to export the layer as a PNG
    write_image(buildPath(outputFolder, layer.name~".png"), layer.width, layer.height, layer.data, 4);
}
```