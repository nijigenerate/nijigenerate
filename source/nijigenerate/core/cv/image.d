module nijigenerate.core.cv.image;

import std.exception : enforce;
import mir.ndslice.slice; // sliced() 拡張メソッドを利用するため
import mir.ndslice.allocation;
 
/// Image (pixel) format.
enum ImageFormat
{
    IF_UNASSIGNED = 0, /// Not assigned format.
    IF_MONO, /// Mono, single channel format.
    IF_RGB, /// RGB format.
    IF_RGB_ALPHA /// RGB format with alpha.
}
 
immutable size_t[] imageFormatChannelCount = [0, // unassigned
    1, // mono
    3, // rgb
    4  // rgba
    ];
 
/// Bit depth of a pixel in an image.
enum BitDepth : size_t
{
    BD_UNASSIGNED = 0, /// Not assigned depth info.
    BD_8 = 8, /// 8-bit (ubyte) depth type.
}
 
class Image
{
private:
    ImageFormat _format = ImageFormat.IF_UNASSIGNED;
    BitDepth _depth = BitDepth.BD_UNASSIGNED;
    size_t _width = 0;
    size_t _height = 0;
    ubyte[] _data = null;
 
public:
    // 新しいバッファを確保する場合
    this(size_t width, size_t height, ImageFormat format) {
        _width = width;
        _height = height;
        _format = format;
        _depth = BitDepth.BD_8;
        _data = new ubyte[width * height * channels];
    }
 
    // 既存のバッファを利用する場合
    this(size_t width, size_t height, ImageFormat format, BitDepth depth, ubyte[] data) {
        _width = width;
        _height = height;
        _format = format;
        _depth = depth;
        enforce(data.length == width * height * channels,
            "バッファサイズが画像のサイズとチャンネル数に一致しません");
        _data = data;
    }
 
    @property size_t width() const { return _width; }
    @property size_t height() const { return _height; }
    @property ImageFormat format() const { return _format; }
    @property BitDepth depth() const { return _depth; }
    @property size_t channels() const {
        switch (format) {
            case ImageFormat.IF_MONO:       return 1;
            case ImageFormat.IF_RGB:        return 3;
            case ImageFormat.IF_RGB_ALPHA:  return 4;
            default: return 0;
        }
    }
    @property ulong[] shape() const { return [_height, _width, channels]; }
 
    /**
    Get data array from this image.
    Cast data array to corresponding dynamic array type,
    and return it.
    8-bit data is considered ubyte, etc.
    */
    pure inout auto data(T = ubyte)() {
        import std.range : ElementType;
 
        if (_data is null)
            return null;
        static assert(is(T == ubyte) || is(T == ushort) || is(T == float),
            "Pixel data type not supported.");
//        enforce(isOfType!T, "Invalid pixel data type cast.");
        static if (is(ElementType!(typeof(_data)) == T))
            return _data;
        else
            return cast(T[])_data;
    }
 
    @property auto empty() const { return _data is null; }
 
    /// sliced プロパティ：内部データを mir.ndslice の sliced() 拡張メソッドで取得
    @property auto sliced(T = ubyte)() inout {
        // ※ data() は関数であり、this.data!T() を呼び出す必要がある
        return this.data!T().sliced(_height, _width, channels);
    }
 
    // 以下、asType やその他のメソッドは省略（参考コードそのまま）
}
 
unittest {
    // 新しいバッファを確保する例 (RGB_ALPHA: 4 チャンネル)
    auto imbin = new Image(640, 480, ImageFormat.IF_RGB_ALPHA);
    auto slice1 = imbin.sliced!float; // float型としてスライス
    assert(slice1.shape == [480, 640, 4]);
 
    // 既存のバッファを利用する例 (モノクロ: 1 チャンネル)
    ubyte[] d = new ubyte[200 * 150];
    auto img = new Image(200, 150, ImageFormat.IF_MONO, BitDepth.BD_8, d);
    auto imgSlice = img.sliced;
    assert(imgSlice.shape == [150, 200, 1]);
}
