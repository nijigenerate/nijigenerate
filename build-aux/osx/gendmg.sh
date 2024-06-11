DMGTITLE="Install nijigenerate"
DMGFILENAME="Install_Inochi_nijigenerate.dmg"

if [ -d "out/nijigenerate.app" ]; then
    if [ -f "out/$DMGFILENAME" ]; then
        echo "Removing prior install dmg..."
        rm "out/$DMGFILENAME"
    fi

    cd out/
    echo "Building $DMGFILENAME..."

    # Create Install Volume directory

    if [ -d "InstallVolume" ]; then
        echo "Cleaning up old install volume..."
        rm -r InstallVolume
    fi

    mkdir -p InstallVolume
    cp ../LICENSE LICENSE
    cp -r "nijigenerate.app" "InstallVolume/nijigenerate.app"
    
    create-dmg \
        --volname "$DMGTITLE" \
        --volicon "nijigenerate.icns" \
        --background "../build-aux/osx/dmgbg.png" \
        --window-size 800 600 \
        --icon "nijigenerate.app" 200 250 \
        --hide-extension "nijigenerate.app" \
        --eula "LICENSE" \
        --app-drop-link 600 250 \
        "$DMGFILENAME" InstallVolume/

    echo "Done! Cleaning up temporaries..."
    rm LICENSE

    echo "DMG generated as $PWD/$DMGFILENAME"
    cd ..
else
    echo "Could not find nijigenerate for packaging..."
    exit 1
fi