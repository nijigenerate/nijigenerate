module nijigenerate.widgets.modal;

public import nijigenerate.widgets.modal.nagscreen;
import bindbc.sdl;
import nijigenerate.widgets.label;
import nijigenerate.widgets.markdown;
import nijigenerate.widgets.dummy;
import nijigenerate.widgets.modal;
import nijigenerate.core;
import nijigenerate.core.i18n;
import std.string;
import nijigenerate.utils.link;
import i18n;
import nijilive;
import nijigenerate.ver;
import nijigenerate.io;
import nijigenerate;
import nijigenerate.config;

private {
    __gshared Modal[] incModalList;
    ptrdiff_t incModalIndex = -1;

}
/**
    A modal widget
*/
abstract class Modal {
private:
    string title_;
    const(char)* imTitle;
    bool visible;
    bool hasTitlebar;


protected:
    bool drewWindow;
    ImGuiWindowFlags flags;

    abstract void onUpdate();

    void onBeginUpdate() {
        if (imTitle is null) imTitle = title_.toStringz;

        // TITLE
        if (visible && !igIsPopupOpen(imTitle)) {
            igOpenPopup(imTitle);
        }

        drewWindow = igBeginPopupModal(
            imTitle,
            &visible, 
            hasTitlebar ? ImGuiWindowFlags.None : ImGuiWindowFlags.NoResize | ImGuiWindowFlags.NoDecoration
        );
    }
    
    void onEndUpdate() {
        if (drewWindow) igEndPopup();

        // Handle the user closing the modal from the titlebar.
        if (!visible) {
            incModalCloseTop();
        }
    }

    string title() {
        return title_;
    }

public:


    /**
        Constructs a frame
    */
    this(string name, bool hasTitlebar) {
        this.title_ = name;
        this.hasTitlebar = hasTitlebar;
        this.visible = true;
    }

    /**
        Draws the frame
    */
    final void update() {
        this.onBeginUpdate();
            if(drewWindow) this.onUpdate();
        this.onEndUpdate();
    }
}

/**
    Renders current top modal
*/
void incModalRender() {
    if (incModalIndex > -1) {
        incModalList[incModalIndex].update();
    }
}

/**
    Adds a modal to the modal display list.
*/
void incModalAdd(Modal modal) {
    
    // Increase modal list length if need be
    if (incModalIndex+1 >= incModalList.length) incModalList.length++;

    // Set topmost modal
    incModalList[++incModalIndex] = modal;
}

/**
    Closest the top level modal
    Can only be called from within a modal.
*/
void incModalCloseTop() {
    if (incModalIndex >= 0) {
        incModalIndex--;
    }
}