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
#include <ShObjIdl.h>
#include <deque>

#include "imgui.h"
#include "imgui_internal.h"
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

class Counter {

public:
    Counter(float bpm, float tpb) : elapsedMicroSeconds(0) {

        setTicksPerMicroSecond(bpm, tpb);
        mFPS_previous = now();
        mFPS_current = now();
        startMidiTicks = now();
    }

    Counter() {
        setTicksPerMicroSecond(120, 60);
        elapsedMicroSeconds = 0;
        mFPS_previous = now();
        mFPS_current = now();
        startMidiTicks = now();
    }

    void setTicksPerMicroSecond(float bpm, float tpb) {
        tickRate = tpb * (bpm / 60e6);
    }

    // alias
    void setTickRates(const float& bpm, const float& tpb) {
        setTicksPerMicroSecond(bpm, tpb);
    }

    void printFPS() {
        string output = to_string(getFPS()) + " FPS\n";
        OutputDebugStringA(output.data());
    }

    double getFPS() {
        mFPS_current = now();
        size_t duration = std::chrono::duration_cast<std::chrono::microseconds> (mFPS_current - mFPS_previous).count();
        mFPS_previous = mFPS_current;
        double FPS = 1e6 / duration;
        return FPS;
    }

    int getDeltaTick() {
        midiEventTick = now();
        elapsedMicroSeconds = std::chrono::duration_cast<std::chrono::microseconds> (midiEventTick - startMidiTicks).count();
        startMidiTicks = now();
        return elapsedMicroSeconds * tickRate;
    }

    int getAbsTick() {
        midiEventTick = now();
        elapsedMicroSeconds = std::chrono::duration_cast<std::chrono::microseconds> (midiEventTick - startMidiTicks).count();
        return elapsedMicroSeconds * tickRate;
    }




private:
    timepoint mFPS_current;
    timepoint mFPS_previous;

    timepoint startMidiTicks;
    timepoint midiEventTick;
    size_t elapsedMicroSeconds;

    double tickRate; // ticks per second
};
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
    void updatePiano();
    void makeRectangles();
    void renderMidiLog(ImFont* font);

    RECT pCreateRect(const POINT& topLeft, const POINT& bottomRight);
    RECT pCreateRect(const POINT& center, int radius);
    RECT pCreateRect(const ImVec2& topLeft, const ImVec2& bottomRight);


    void startSampling();
    void stopSampling();

    void setEmpty();

    // start measuring instance
    void getColor();
    bool saveMidiFile(MidiFile& out);

    int detectNoteEvent(pianoColors& sampo, MidiFile& outFile, int tick);

    pianoColors sampleBitmap();

    int createMidiFile();
    string savePath = {};
    bool isSampling = 0;
    POINT windowLocation;

    struct midiAppLog
    {
        ImGuiTextBuffer     Buf;
        ImGuiTextFilter     Filter;
        ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
        bool                AutoScroll;  // Keep scrolling if already at the bottom.
        vector<string> keyNames = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        midiAppLog()
        {
            AutoScroll = true;
            Clear();
        }

        void Clear()
        {
            Buf.clear();
            LineOffsets.clear();
            LineOffsets.push_back(0);
        }

        void AddLog(const char* fmt, ...) IM_FMTARGS(2)
        {
            int old_size = Buf.size();
            va_list args;
            va_start(args, fmt);
            Buf.appendfv(fmt, args);
            va_end(args);
            for (int new_size = Buf.size(); old_size < new_size; old_size++)
                if (Buf[old_size] == '\n')
                    LineOffsets.push_back(old_size + 1);
        }

        void writeEvent(vector<uchar> mEvent) {

            int octave = mEvent[Note] / 12;     // rounded down
            int key = mEvent[Note] % 12;
            bool down = mEvent[OnOff] == 0x90;
            string state = down ? "down] " : "up  ] ";

            string output = "[press " + state + keyNames[key] + to_string(octave) + "\n";


            AddLog(output.data());

        }


        void Draw(const char* title, bool* p_open = NULL)
        {


            if (!ImGui::Begin(title, p_open))
            {
                ImGui::End();
                return;
            }

            // Options menu
            if (ImGui::BeginPopup("Options"))
            {
                ImGui::Checkbox("Auto-scroll", &AutoScroll);
                ImGui::EndPopup();
            }

            // Main window
            if (ImGui::Button("Options"))
                ImGui::OpenPopup("Options");
            ImGui::SameLine();
            bool clear = ImGui::Button("Clear");
            ImGui::SameLine();
            bool copy = ImGui::Button("Copy");
            ImGui::SameLine();
            Filter.Draw("Filter", -100.0f);

            ImGui::Separator();

            if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
            {
                if (clear)
                    Clear();
                if (copy)
                    ImGui::LogToClipboard();

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                const char* buf = Buf.begin();
                const char* buf_end = Buf.end();
                if (Filter.IsActive())
                {
                    // In this example we don't use the clipper when Filter is enabled.
                    // This is because we don't have random access to the result of our filter.
                    // A real application processing logs with ten of thousands of entries may want to store the result of
                    // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
                    for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
                    {
                        const char* line_start = buf + LineOffsets[line_no];
                        const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                        if (Filter.PassFilter(line_start, line_end))
                            ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                else
                {
                    // The simplest and easy way to display the entire buffer:
                    //   ImGui::TextUnformatted(buf_begin, buf_end);
                    // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
                    // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
                    // within the visible area.
                    // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
                    // on your side is recommended. Using ImGuiListClipper requires
                    // - A) random access into your data
                    // - B) items all being the  same height,
                    // both of which we can handle since we have an array pointing to the beginning of each line of text.
                    // When using the filter (in the block of code above) we don't have random access into the data to display
                    // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
                    // it possible (and would be recommended if you want to search through tens of thousands of entries).
                    ImGuiListClipper clipper;
                    clipper.Begin(LineOffsets.Size);
                    while (clipper.Step())
                    {
                        for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                        {
                            const char* line_start = buf + LineOffsets[line_no];
                            const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                            ImGui::TextUnformatted(line_start, line_end);
                        }
                    }
                    clipper.End();
                }
                ImGui::PopStyleVar();

                // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
                // Using a scrollbar or mouse-wheel will take away from the bottom edge.
                if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::End();

        }
    };

private:

    enum eventDetails {
        OnOff, Note, Velocity
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
    float noteOffset = 0;
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
    pianoColors oldPixelValues = { 0 };
    vector<bool> wasPressed;
    int bufferIndex = 0;

    midiAppLog midiLog;
    MidiFile outputFile;
    bool doneSaving = 0;
    string saveDirectory = {};

    public:


};