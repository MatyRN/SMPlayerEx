//================LIB and INC=====================//
#define WINVER 0x0501          // Windows XP
#define _WIN32_WINNT 0x0501
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Timer.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Shared_Image.H>
#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_MP3
#include "headers/miniaudio.h"
#include <vector>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <cmath>
///=====================================///

#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 200

///======= FLTK ========///
Fl_Window *main_window;
Fl_Slider *volume;
Fl_Button *next_button = nullptr;
Fl_Button *prev_button = nullptr;
Fl_Hold_Browser *browser = nullptr;

///======= FLTK IMAGE=========///
Fl_PNG_Image *background_image = nullptr;
Fl_PNG_Image *play_image = nullptr;
Fl_PNG_Image *play_image_pressed = nullptr;
Fl_PNG_Image *pause_image = nullptr;
Fl_PNG_Image *pause_image_pressed = nullptr;
Fl_PNG_Image *next_image = nullptr;
Fl_PNG_Image *next_image_pressed = nullptr;
Fl_PNG_Image *prev_image = nullptr;
Fl_PNG_Image *prev_image_pressed = nullptr;

///======== MUSIC ============///
std::string scrolled_text;
std::vector<std::string> musicFiles;
int currentTrackIndex = -1;
ma_engine MusicEngine;
ma_sound MusicSound;
bool SoundInitialized = false;
bool IsPaused = false;
ma_uint64 PausedFrame = 0;
const char *selected_file = nullptr;
int scroll_pos = 0;

///=========== VISUALIZER =========///
float tempo_bpm = 120.0f;
class VisualizerBox;
VisualizerBox *visualizer_box;

///==== Get your MUSICS in MUSIC FOLDER ====///
std::string GetUserMusicFolder() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYMUSIC, NULL, 0, path)))
        return std::string(path);
    return "";
}

///==== IMAGE Manager ===///
class ImageButton : public Fl_Button {
    Fl_Image *normal_img;
    Fl_Image *pressed_img;
    bool is_pressed = false;

public:
    ImageButton(int X, int Y, int W, int H, Fl_Image *normal, Fl_Image *pressed = nullptr) : Fl_Button(X, Y, W, H), normal_img(normal), pressed_img(pressed) {
        box(FL_NO_BOX);
        label("");
        clear_visible_focus();
    }

    void set_images(Fl_Image *normal, Fl_Image *pressed = nullptr) {
        normal_img = normal;
        pressed_img = pressed;
        redraw();
    }

    int handle(int event) override {
        switch (event) {
        case FL_PUSH:
            is_pressed = true;
            redraw();
            return 1;
        case FL_RELEASE:
            is_pressed = false;
            redraw();
            do_callback();
            return 1;
        }
        return Fl_Button::handle(event);
    }

    void draw() override {
        if (is_pressed && pressed_img)
            pressed_img->draw(x(), y(), w(), h());
        else if (normal_img)
            normal_img->draw(x(), y(), w(), h());
    }
};

ImageButton *play_button = nullptr;

///==== LOAD MUSIC Folder ====///
void Load_Music_Folder() {
    browser->clear();
    musicFiles.clear();

    std::string musicFolder = GetUserMusicFolder();
    if (musicFolder.empty()) {
        fl_alert("The user's Music folder could not be accessed.");
        return;
    }

    std::string pattern = musicFolder + "\\*.mp3";
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile(pattern.c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string filename = findFileData.cFileName;
            std::string fullPath = musicFolder + "\\" + filename;
            musicFiles.push_back(fullPath);
            browser->add(filename.c_str());
        } while (FindNextFile(hFind, &findFileData));
        FindClose(hFind);
    } else {
        fl_alert("No MP3 files were found in your Music folder.");
    }
}

///==== PLAY MUSIC ====///
void Play_Music(Fl_Widget *, void *) {
    if (!selected_file) {
        fl_alert("First select a song.");
        return;
    }
    if (!SoundInitialized) {
        if (ma_sound_init_from_file(&MusicEngine, selected_file, MA_SOUND_FLAG_STREAM, NULL, NULL, &MusicSound) != MA_SUCCESS) {
            fl_alert("Error uploading file.");
            return;
        }
        SoundInitialized = true;
        IsPaused = false;
        PausedFrame = 0;
    }
    if (!IsPaused) {
        ma_sound_start(&MusicSound);
        play_button->set_images(pause_image, pause_image_pressed);
        IsPaused = true;
    } else {
        ma_sound_get_cursor_in_pcm_frames(&MusicSound, &PausedFrame);
        ma_sound_stop(&MusicSound);
        play_button->set_images(play_image, play_image_pressed);
        IsPaused = false;
    }
}

///===== BROWSER of MUSIC======///
void On_Browser_Select(Fl_Widget *, void *) {
    int selected = browser->value() - 1;
    if (selected >= 0 && selected < musicFiles.size()) {
        currentTrackIndex = selected;
        selected_file = musicFiles[currentTrackIndex].c_str();

        if (SoundInitialized) {
            ma_sound_uninit(&MusicSound);
            SoundInitialized = false;
        }

        if (ma_sound_init_from_file(&MusicEngine, selected_file, MA_SOUND_FLAG_STREAM, NULL, NULL, &MusicSound) == MA_SUCCESS) {
            scrolled_text = musicFiles[selected]; // Reinicia el texto scrolleado
            SoundInitialized = true;
            ma_sound_start(&MusicSound);
            play_button->set_images(pause_image, pause_image_pressed);
            IsPaused = true;
        }
    }
}

///===== NEXT MUSIC =====///
void Next_Music(Fl_Widget *, void *) {
    if (musicFiles.empty()) return;
    currentTrackIndex = (currentTrackIndex + 1) % musicFiles.size();
    browser->select(currentTrackIndex + 1);
    On_Browser_Select(nullptr, nullptr);
}

///===== PREV MUSIC =====///
void Prev_Music(Fl_Widget *, void *) {
    if (musicFiles.empty()) return;
    currentTrackIndex = (currentTrackIndex - 1 + musicFiles.size()) % musicFiles.size();
    browser->select(currentTrackIndex + 1);
    On_Browser_Select(nullptr, nullptr);
}

///====== BACKGROUND BOX ====///
class BackgroundBox : public Fl_Box {
public:
    Fl_PNG_Image *image;

    BackgroundBox(int X, int Y, int W, int H, Fl_PNG_Image *img)
        : Fl_Box(X, Y, W, H), image(img) {}

    void draw() override {
        if (image)
            image->draw(x(), y(), w(), h());
        else {
            fl_draw_box(FL_FLAT_BOX, x(), y(), w(), h(), FL_GRAY);
            fl_color(FL_RED);
            fl_draw("NO IMAGE", x() + 10, y() + 20);
        }
    }
};

///====== VIZUALIZER BOX ======///
class VisualizerBox : public Fl_Box{
public:
    float cube_x, cube_y;
    float cube_dx, cube_dy;
    const int cube_size = 20;

    VisualizerBox(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H), cube_x(10), cube_y(10), cube_dx(2.0f), cube_dy(1.5f) {}

    void draw() override {

        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());

        fl_color(FL_BLUE);
        fl_rectf((int)cube_x, (int)cube_y , cube_size, cube_size);
        fl_color(FL_WHITE);
        fl_rect((int)cube_x, (int)cube_y, cube_size, cube_size);

        fl_color(FL_BLACK);
        fl_rectf((int)cube_x+12, (int)cube_y+2, cube_size/3, cube_size/3);
        fl_color(FL_BLACK);
        fl_rectf((int)cube_x+2, (int)cube_y+2, cube_size/3, cube_size/3);

        fl_color(FL_WHITE);
        fl_rect((int)cube_x+12, (int)cube_y+2, cube_size/3, cube_size/3);
        fl_color(FL_WHITE);
        fl_rect((int)cube_x+2, (int)cube_y+2, cube_size/3, cube_size/3);

        fl_color(FL_WHITE);
        fl_rectf((int)cube_x+13, (int)cube_y+5, cube_size-18, cube_size-18);
        fl_color(FL_WHITE);
        fl_rectf((int)cube_x+5, (int)cube_y+5, cube_size-18, cube_size-18);
    }

    void move_cube() {
        cube_x += cube_dx;
        cube_y += cube_dy;


        if (cube_x < 0 || cube_x + cube_size > w())
            cube_dx = -cube_dx;


        if (cube_y < 0 || cube_y + cube_size > h())
            cube_dy = -cube_dy;

        redraw();
    }

    void update_with_tempo(float intensity) {
        float speed = 1.0f + intensity * 3.0f;
        cube_dx = (cube_dx < 0 ? -1 : 1) * speed;
        cube_dy = (cube_dy < 0 ? -1 : 1) * speed;
    }

};

///======= SCROLL of TEXT ======///
void scroll_text_cb(void *){
    if (currentTrackIndex < 0 || currentTrackIndex >= musicFiles.size()) {
        Fl::repeat_timeout(0.1, scroll_text_cb);
        return;
    }

    float time_sec = 0;
    ma_sound_get_cursor_in_seconds(&MusicSound, &time_sec);

    float tempo = fabs(sinf(time_sec * 3.14f));
    visualizer_box->update_with_tempo(tempo);
    visualizer_box->move_cube();
    visualizer_box->redraw();

    int selected = browser->value();
    if (selected > 0 && selected <= musicFiles.size()) {
        if (!scrolled_text.empty()) {
            char first_char = scrolled_text[0];
            scrolled_text.erase(0, 1);
            scrolled_text.push_back(first_char);

            browser->remove(selected);
            browser->insert(selected, scrolled_text.c_str());
            browser->select(selected);
        }
    }

    Fl::repeat_timeout(0.033, scroll_text_cb);
}

///====== INIT TEXTURE and GRAPHICS =====///
void Init_Graphics() {
    Fl_Shared_Image::get("res/BackgroundPanel.PNG");
    background_image = new Fl_PNG_Image("res/BackgroundPanel.PNG");
    play_image = new Fl_PNG_Image("res/Play.png");
    play_image_pressed = new Fl_PNG_Image("res/Play_pressed.png");
    pause_image = new Fl_PNG_Image("res/Stop.png");
    pause_image_pressed = new Fl_PNG_Image("res/Stop_pressed.png");
    next_image = new Fl_PNG_Image("res/Next.png");
    next_image_pressed = new Fl_PNG_Image("res/Next_pressed.png");
    prev_image = new Fl_PNG_Image("res/Prev.png");
    prev_image_pressed = new Fl_PNG_Image("res/Prev_pressed.png");

    new BackgroundBox(0, 50, WINDOW_WIDTH, WINDOW_HEIGHT, background_image);
    visualizer_box = new VisualizerBox(0, 0, 300, 50);

    play_button = new ImageButton(135, 70, play_image->w(), play_image->h(), play_image, play_image_pressed);
    play_button->callback(Play_Music);

    next_button = new ImageButton(180, 70, next_image->w(), next_image->h(), next_image, next_image_pressed);
    next_button->callback(Next_Music);

    prev_button = new ImageButton(90, 70, prev_image->w(), prev_image->h(), prev_image, prev_image_pressed);
    prev_button->callback(Prev_Music);

    browser = new Fl_Hold_Browser(10, 130, 290, 50);
    browser->color(FL_DARK_BLUE);
    browser->textcolor(FL_CYAN);
    browser->callback(On_Browser_Select);

    volume = new Fl_Slider(100, 110, 100, 15, nullptr);
    volume->type(FL_HORIZONTAL);
    volume->range(0.0, 1.0);
    volume->value(0.8);
    volume->callback([](Fl_Widget *w, void *) {
        float vol = ((Fl_Slider *)w)->value();
        ma_engine_set_volume(&MusicEngine, vol);
    });

    Load_Music_Folder();
    Fl::add_timeout(0.03, scroll_text_cb);
}

///====== SELECT THE ICON ====///
void set_icon(Fl_Window* win, const char* iconPath) {
    HWND hwnd = fl_xid(win);
    HICON hIcon = (HICON)LoadImageA(NULL, iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (hIcon) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
}

///====== MAIN =======///
int main(int argc, char **argv) {
    if (ma_engine_init(NULL, &MusicEngine) != MA_SUCCESS) {
        printf("Init Sound Failed.\n");
        return -1;
    }
    atexit([]() {
        if (SoundInitialized)
            ma_sound_uninit(&MusicSound);
        ma_engine_uninit(&MusicEngine);
    });
    main_window = new Fl_Window(WINDOW_WIDTH, WINDOW_HEIGHT, "SMPlayerEX");
    Init_Graphics();

    main_window->end();
    main_window->show(argc, argv);
    set_icon(main_window, "res/Icon.ico");
    return Fl::run();
}
