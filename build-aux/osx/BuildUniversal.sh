# Generate version files...
dub build --config=meta

# First build ARM64 version...
echo "Building arm64 binary..."
dub build --build=release --config=osx-full --arch=arm64-apple-macos
mv "out/nijigenerate.app/Contents/MacOS/nijigenerate" "out/nijigenerate.app/Contents/MacOS/nijigenerate-arm64"

# Then the X86_64 version...
echo "Building x86_64 binary..."
dub build --build=release --config=osx-full --arch=x86_64-apple-macos
mv "out/nijigenerate.app/Contents/MacOS/nijigenerate" "out/nijigenerate.app/Contents/MacOS/nijigenerate-x86_64"

# Glue them together with lipo
echo "Gluing them together..."
lipo "out/nijigenerate.app/Contents/MacOS/nijigenerate-x86_64" "out/nijigenerate.app/Contents/MacOS/nijigenerate-arm64" -output "out/nijigenerate.app/Contents/MacOS/nijigenerate" -create

# Print some nice info
echo "Done!"
lipo -info "out/nijigenerate.app/Contents/MacOS/nijigenerate"

# Cleanup and bundle
echo "Cleaning up..."
rm "out/nijigenerate.app/Contents/MacOS/nijigenerate-x86_64" "out/nijigenerate.app/Contents/MacOS/nijigenerate-arm64"
./build-aux/osx/osxbundle.sh