module nijigenerate.io.imageexport;
import nijigenerate.core.tasks;
import imagefmt;
import i18n;
import std.format;
import std.path;
import std.string;

/**
    Exports image from RGB(A) color data
*/
void incExportImage(string file, ubyte[] data, int width, int height, int channels = 0) {
    int e;
    ubyte[] encoded = write_image_mem(incImageFormat(file), width, height, data, channels, e);
    if (e != 0) {
        incSetStatus(_("%s failed to export with error (%s)...".format(file, IF_ERROR[e])));
        return;
    }
    scope(exit) _free(encoded.ptr);

    try {
        import std.file : write;
        write(file, encoded);
        incSetStatus(_("%s was exported...".format(file)));
    } catch (Exception ex) {
        e = ERROR.fopen;
        incSetStatus(_("%s failed to export with error (%s)...".format(file, IF_ERROR[e])));
    }
}

private int incImageFormat(string file) {
    switch (file.extension.toLower) {
        case ".bmp": return IF_BMP;
        case ".tga": return IF_TGA;
        case ".png": return IF_PNG;
        case ".jpg", ".jpeg": return IF_JPG;
        default: return -1;
    }
}
