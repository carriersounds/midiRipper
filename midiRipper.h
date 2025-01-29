#pragma once

#include "resource.h"
#include "framework.h"

#include <string>
#include "MidiFile.h"
#include "debugapi.h"
#include "windowsx.h"
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <excpt.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

using namespace smf;
using std::vector;
using std::thread;
using std::to_string;
using std::chrono::high_resolution_clock;
using std::string;

#define MAX_LOADSTRING 100
#define inc(x) x = (x == 0xFFFFFFFF) ? 0 : x + 1
//#define interp Interpreter::getInstance()
#define now(x) std::chrono::high_resolution_clock::now(x)

#define toPoint(P) {P.x,P.y}

typedef vector<BYTE> pianoColors;
typedef std::chrono::high_resolution_clock::time_point timepoint;


enum isDragging {

    point_sample,
    point_topLeft = 98,
    point_botRight = 99

};
enum eventDetails {
    OnOff, Note, Velocity
};

class Interpreter {



public:




    Interpreter();

    void renderUserInput();
    void renderFrame();

    void log(string text);
    RECT pCreateRect(const POINT& topLeft, const POINT& bottomRight);
    RECT pCreateRect(const POINT& center, int radius);
    RECT pCreateRect(const ImVec2& topLeft, const ImVec2& bottomRight);

    void updatePiano();
    void changeOffset(int increase);
    void addOctave();
    void removeOctave();
    void makeRectangles();
 
    void startSampling();
    void stopSampling();

    void setEmpty();

    // start measuring instance
    void getColor();

    bool saveMidiFile(MidiFile& out);

    int detectNoteEvent(pianoColors& sampo, MidiFile& outFile, int tick);

    pianoColors sampleBitmap();

    void printEvent(vector<uchar> mEvent);

    int createMidiFile();


    bool isSampling = 0;
    POINT windowLocation;

private:

    //  ~Interpreter() {}
    //  Interpreter(const Interpreter&) = delete; // Delete copy constructor
    //  Interpreter& operator=(const Interpreter&) = delete; // Delete assignment operator


    enum isDragging {

        point_sample,
        point_topLeft = 98,
        point_botRight = 99

    };
    enum eventDetails {
        OnOff, Note, Velocity
    };

    vector<string> keyNames = {
        "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };

    RECT piano;
    POINT absolutePT;


    // Dots & interactive GUI
    vector<RECT> squares;
    vector<bool> isDragging;
    vector<POINT> points;
    vector<POINT> absolutePoints;
    int squareID;
    int amountOfSamples;
    int octaveCount;
    double noteOffset = 0;
    bool moved = false;

    ImVec2  windowPos;
    ImVec2 windowSize;

    // sampling & data processing

    thread samplingThread;
    int time;
    std::mutex LockMap;
    HDC hdcScreen = nullptr;
    HDC hdcMem = nullptr;
    HBITMAP hBitmap = nullptr;
    RECT captureRect = { 0, 0, 0, 0 };

    pianoColors oldPixelValues;
    vector<bool> wasPressed;

    int bufferIndex = 0;


};