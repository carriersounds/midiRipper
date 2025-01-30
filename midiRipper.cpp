// midiRipper.cpp : Defines the entry point for the application.

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <excpt.h>
#include <d3d11.h>
#include <tchar.h>
#include "midiRipper.h"

using namespace smf;
using std::vector;
using std::thread;
using std::to_string;
using std::chrono::high_resolution_clock;
using std::string;

typedef vector<BYTE> pianoColors;
typedef std::chrono::high_resolution_clock::time_point timepoint;


// Global Variables:
//HINSTANCE hInst;                                // current instance
//WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
//WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
//ATOM                MyRegisterClass(HINSTANCE hInstance);
//BOOL                InitInstance(HINSTANCE, int);
//LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);




    //~Interpreter() {}
   // Interpreter(const Interpreter&) = delete; // Delete copy constructor
    //Interpreter& operator=(const Interpreter&) = delete; // Delete assignment operator

Interpreter::Interpreter() {

       isDragging.resize(100);
       squares.resize(100);
       points.resize(100);


       windowLocation = { 0,0 };

       piano = { 100,200,200,300 };      // Initial rectangle position and size
       points[point_sample] = { ((piano.right + piano.left) >> 1), ((piano.bottom + piano.top) >> 1) }; // middle


       amountOfSamples = 1;
       octaveCount = 0;

       absolutePT = { piano.left, piano.top };

       time = 0;

       squareID = 1;

       absolutePoints.resize(1);
       oldPixelValues.resize(1);
       wasPressed.resize(1);
    }
    
RECT Interpreter::pCreateRect(const POINT& topLeft, const POINT& bottomRight) {
        return { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    }
RECT Interpreter::pCreateRect(const ImVec2& topLeft, const ImVec2& bottomRight) {
    return { (LONG)topLeft.x, (LONG)topLeft.y, (LONG)bottomRight.x, (LONG)bottomRight.y };
}
RECT Interpreter::pCreateRect(const POINT& center, int radius = 5) {
        return {
            center.x - radius,
            center.y - radius,
            center.x + radius,
            center.y + radius
        };
    }


void Interpreter::updatePiano() {

        if (!LockMap.try_lock()) {

            return;
        }

        piano = pCreateRect(windowPos, windowSize + windowPos);


        if (!octaveCount) {   
            points[point_sample] = { ((piano.right + piano.left) >> 1), ((piano.bottom + piano.top) >> 1) }; // middle
            amountOfSamples = 1;
            LockMap.unlock();
            return;
        }
       

        if (octaveCount < 0) octaveCount = -1;

        if (octaveCount > 7) {
            octaveCount = 7;
            MessageBoxA(NULL, "Seven octaves max (88 keys)", "Note range exceeded", MB_OK);
            LockMap.unlock();
            return;
        }


        int height = piano.bottom - piano.top;

        // Calculate the width of one octave
        int pianoWidth = piano.right - piano.left;
        int octaveWidth = octaveCount ? pianoWidth / octaveCount : 0;

        // Pre-defined positions for white and black keys in an octave (relative positions)
        const double HkeyPositions[12]{
            0.5 / 7,                                // C
            1.0 / 7 + noteOffset,       // Db
            1.5 / 7,                                // D
            2.0 / 7 - noteOffset,       // Eb
            2.5 / 7,                                // E
            3.5 / 7,                                // F
            4.0 / 7 + noteOffset,       // F#
            4.5 / 7,                                // G
            5.0 / 7,                    // Ab
            5.5 / 7,                                // A
            6.0 / 7 - noteOffset,       // Bb
            6.5 / 7                                 // B

        };
        const double VkeyPositions[12]{
            0.75,0.25,0.75,0.25,0.75,
            0.75,0.25,0.75,0.25,0.75, 0.25, 0.75
        };
        int notePos = 0;


        for (int octave = 0; octave < octaveCount; ++octave) {
            int octaveStartX = piano.left + octave * octaveWidth;

            for (size_t note = 0; note < 12; note++)
            {
                POINT point;
                point.x = octaveStartX + static_cast<int>(HkeyPositions[note] * octaveWidth);
                point.y = VkeyPositions[note]*height + piano.top;
                points[notePos] = point;
                notePos++;
            }

        }

        amountOfSamples = notePos;
        oldPixelValues.resize(amountOfSamples);
        wasPressed.resize(amountOfSamples);

        LockMap.unlock();

    }

void Interpreter::startSampling() {

        isSampling = 1;

        samplingThread = std::thread(&Interpreter::getColor, this);

     //   samplingThread.join();

        if (samplingThread.joinable()) samplingThread.detach();


    }
void Interpreter::stopSampling() {

        isSampling = 0;
    }
void Interpreter::setEmpty() {
        oldPixelValues = sampleBitmap();
    }


void Interpreter::getColor() {
    
        doneSaving = false;
        hdcScreen = GetDC(NULL);
        hdcMem = CreateCompatibleDC(hdcScreen);
  
        int ticksPerBeat = 500;             
        Counter counter; // 120 BPM = 500ms per beat, 1ms per tick
        pianoColors grayscales(amountOfSamples);
        pianoColors startGray(amountOfSamples);
        MidiFile outFile;
        int currentTick;

        counter.setTickRates(120, ticksPerBeat);
        outFile.setTicksPerQuarterNote(ticksPerBeat);
        outFile.absoluteTicks();
        outFile.addTrack(1);
        oldPixelValues = sampleBitmap();

        while (isSampling) {

            grayscales = sampleBitmap();
            currentTick = counter.getAbsTick();

            detectNoteEvent(grayscales, outFile, currentTick);   

         // counter.printFPS();
        }

        outputFile = outFile;
        doneSaving = true;
    }

bool Interpreter::saveMidiFile(MidiFile& out) {
    out.sortTracks();

    time_t t = std::time(NULL);
    tm thisMoment;
    localtime_s(&thisMoment, &t);

    char timestamp[_MAX_PATH];
    strftime(timestamp, _MAX_PATH, "MIDI Recording on %Y-%m-%d at %H.%M.%S", &thisMoment);

    OPENFILENAMEA ofn = { sizeof(OPENFILENAME) };

    char filePath[_MAX_PATH] = {};  // Use a writable character array
    strcpy_s(filePath, timestamp);  // Copy timestamp into filePath

    ofn.hwndOwner = GetConsoleWindow();
    ofn.lpstrFile = filePath;  // Safe writable buffer
    ofn.nMaxFile = _MAX_PATH;
    ofn.lpstrFilter = "MIDI Files\0*.mid\0";
    ofn.lpstrDefExt = "mid";
    ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;

    std::string FullDirectory;
    int saved = 0;

    if (GetSaveFileNameA(&ofn)) {
        FullDirectory = filePath;
        saved = out.write(FullDirectory);
    }

    return saved;
}
int Interpreter::detectNoteEvent(pianoColors& sampo, MidiFile& outFile, int tick) {

        const int defaultVelocity = 64;
        string samplingMoment;
        int stringIterator = 0;
        int changesDetected = 0;
        vector<bool> isPressed(amountOfSamples);

        for (int i = 0; i < amountOfSamples; i++) {
    

            isPressed[i] = (abs(sampo[i] - oldPixelValues[i]) > 3);         // Note down, different from the "start values" of the piano
            // add sensitivity slider !!!

            if (isPressed[i] != wasPressed[i]) {                             // IS EVENT TRIGGER! up or down 

                vector<uchar> currentEvent = { 0 , 0, 0x80 }; //  (60 = middle C3)

                currentEvent[OnOff] = isPressed[i] ? 0x90 : 0x80;       // note DOWN or UP event ?     0x90 = down   |   0x80 = up             
                currentEvent[Note] = octaveCount > 5 ? i : i + 36;            // start from C 1, if octavecount > 5 start from C -2 (fullrange)

                string time = to_string(tick) + " Absticks: ";
               // OutputDebugStringA(time.data());
     
                midiLog.writeEvent(currentEvent);
                outFile.addEvent(1, tick, currentEvent);

            }
            wasPressed[i] = isPressed[i];
        }    

        return changesDetected; // if change detected, = 0;
    }
pianoColors Interpreter::sampleBitmap() {

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        pianoColors grayscales(amountOfSamples);

        if (hBitmap) DeleteObject(hBitmap);

        int width = piano.right - piano.left;
        int height = piano.bottom - piano.top;

        hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        SelectObject(hdcMem, hBitmap);
        BitBlt(hdcMem, 0, 0, width, height, hdcScreen, piano.left, piano.top, SRCCOPY);

        // Prepare to extract pixel data
        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = piano.right - piano.left;
        bmi.bmiHeader.biHeight = piano.bottom - piano.top; // Top-down bitmap
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;// monochrome 32; // 32 bits per pixel (BGRA format)
        bmi.bmiHeader.biCompression = BI_RGB;

        // Allocate memory for pixel data
        int rowStride = ((width * 3 + 3) & ~3); // Align to 4 bytes
        int totalPixels = rowStride * height;
        bool cought = 0;

        vector<BYTE> pixels(totalPixels);
        GetDIBits(hdcMem, hBitmap, 0, piano.bottom - piano.top, pixels.data(), &bmi, DIB_RGB_COLORS);

        // SAVE BMP FOR DEBUG
#ifdef _DEBUG_
            // string filePath = "A:/Music/Production/Electronics/midiRipper/recording.bmp";

             // Prepare BMP file header
            BITMAPFILEHEADER bfh = { 0 };
            bfh.bfType = 0x4D42;  // 'BM' in little-endian
            bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + totalPixels;
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

            // Open file to write the bitmap
            std::ofstream file("A:/Documents/Tech Projects/Code & Software/midiRipper/imgui.bmp", std::ios::binary);

            // Write the file header, bitmap info header, and pixel data
            file.write(reinterpret_cast<char*>(&bfh), sizeof(BITMAPFILEHEADER));
            file.write(reinterpret_cast<char*>(&bmi.bmiHeader), sizeof(BITMAPINFOHEADER));
            file.write(reinterpret_cast<char*>(pixels.data()), totalPixels);
            // Close the file
            file.close();
#endif


        int horComponent, verComponent, invertedRow, pixelIndex;
        float average;


        for (size_t sPoint = 0; sPoint < amountOfSamples; sPoint++)
        {
            horComponent = points[sPoint].x - piano.left;
            verComponent = points[sPoint].y - piano.top;

            invertedRow = (height - 1) - verComponent;
            pixelIndex = invertedRow * rowStride + horComponent * 3;

            average = (float)pixels[pixelIndex] + (float)pixels[pixelIndex + 1] + (float)pixels[pixelIndex + 2];
            grayscales[sPoint] = average * 0.333;

        }

        return grayscales;
    }

int Interpreter::createMidiFile() {

        // example function from midifile library I used for learning

        MidiFile outputfile;        // create an empty MIDI file with one track
        outputfile.absoluteTicks(); // time information stored as absolute time
                                    // (will be coverted to delta time when written)
        outputfile.addTrack(2);     // Add another two tracks to the MIDI file
        vector<uchar> midievent;    // temporary storage for MIDI events
        midievent.resize(3);        // set the size of the array to 3 bytes
        int tpq = 120;              // default value in MIDI file is 48
        outputfile.setTicksPerQuarterNote(tpq);

        // data to write to MIDI file: (60 = middle C)
        // C5 C  G G A A G-  F F  E  E  D D C-
        int melody[50] = { 72,72,79,79,81,81,79,77,77,76,76,74,74,72,-1 };
        int mrhythm[50] = { 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2,-1 };

        // C3 C4 E C F C E C D B3 C4 A3 F G C-
        int bass[50] = { 48,60,64,60,65,60,64,60,62,59,60,57,53,55,48,-1 };
        int brhythm[50] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,-1 };


        // store a melody line in track 1 (track 0 left empty for conductor info)
        int i = 0;
        int actiontime = 0;      // temporary storage for MIDI event time
        midievent[2] = 64;       // store attack/release velocity for note command
        while (melody[i] >= 0) {
            midievent[0] = 0x90;     // store a note on command (MIDI channel 1)
            midievent[1] = melody[i];
            outputfile.addEvent(1, actiontime, midievent);
            actiontime += tpq * mrhythm[i];
            midievent[0] = 0x80;     // store a note on command (MIDI channel 1)
            outputfile.addEvent(1, actiontime, midievent);
            i++;
        }

        // store a base line in track 2
        i = 0;
        actiontime = 0;          // reset time for beginning of file
        midievent[2] = 64;
        while (bass[i] >= 0) {
            midievent[0] = 0x90;
            midievent[1] = bass[i];
            outputfile.addEvent(2, actiontime, midievent);
            actiontime += tpq * brhythm[i];
            midievent[0] = 0x80;
            outputfile.addEvent(2, actiontime, midievent);
            i++;
        }

        outputfile.sortTracks();         // make sure data is in correct order
        outputfile.write("twinkle.mid"); // write Standard MIDI File twinkle.mid
        return 0;
    
    }

void Interpreter::renderFrame() {

    ImGui::SetNextWindowBgAlpha(0.00f);
    ImGui::Begin("Rendering frame");// , nullptr, ImGuiWindowFlags_NoBackground);

    ImVec4 startvalue = { windowPos.x, windowPos.y,windowSize.x, windowSize.y };
    windowPos = ImGui::GetWindowPos();
    windowSize = ImGui::GetWindowSize();
    ImVec4 newvalue = { windowPos.x, windowPos.y,windowSize.x, windowSize.y };

    if (startvalue != newvalue) {
        updatePiano();
    }

    makeRectangles();
    ImGui::End();
}

void Interpreter::makeRectangles() {

    ImVec4 lineColor = { 255, 40, 40, 255 };

    float TitleBarHeight = 22;

    float thic = 5;
    ImVec2 offset = { 0, TitleBarHeight };

 //   HBRUSH lineColor = CreateSolidBrush(RGB(255, 40, 40));


    ImDrawList* draw_list = ImGui::GetWindowDrawList();


    // Define rectangle coordinates (screen space)
    ImVec2 top_left = windowPos + offset;
    ImVec2 bottom_right = windowPos + windowSize;


    // Draw a rectangle (border only, no fill)
    draw_list->AddRect(top_left, bottom_right, IM_COL32(170, 0, 255, 255), 0.5f, 0, thic); // purple border
    int squareSize = 5;

    // sample points
    for (size_t sPoint = 0; sPoint < amountOfSamples; sPoint++)
    {
        points[sPoint];

        ImVec2 samp = points[sPoint];

        draw_list->AddRect(samp - squareSize, samp + squareSize, IM_COL32(255, 0, 0, 255), 0.5f, 0, 1.0f); // Red border, thickness 3.0f
   
    }

}

void Interpreter::renderUserInput() {


    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 150, 200, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(20, 150, 200, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(20, 150, 255, 255));

    ImGui::SetNextItemWidth(200);
    if (ImGui::Button("Start Sampling")) startSampling();    ImGui::SameLine(150);
    ImGui::SetNextItemWidth(200);
    if (ImGui::Button("Stop Sampling")) stopSampling(); 
    ImGui::SetNextItemWidth(200);

    ImGui::PopStyleColor(3);



    if (isSampling) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 10, 10, 180));
        ImGui::SameLine(300);   
        ImGui::Button("Recording...");
        ImGui::PopStyleColor();
    }


    // save recording
    if (doneSaving && !isSampling) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 150, 255, 100));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 100, 255, 100));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(200, 100, 150, 255));

        bool goodSave = false;
        ImGui::SameLine(300);
        if (ImGui::Button("Save current recording")) {

            goodSave = saveMidiFile(outputFile);

        }
        if (goodSave) {
            ImGui::SameLine(150);
            ImGui::TextColored({ 50, 255, 50, 200 }, "midi file Saved!");
        }
        ImGui::PopStyleColor(3);

    }

    ImGui::Spacing();

    ImGui::SeparatorText("Piano adjustment");
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("Set amount of octaves to sample", &octaveCount, 0, 7, "%d octaves", 0)) {
        updatePiano();
    };

    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("Note offset", &noteOffset, -0.1f, 0.1f, "%.3f", 0)) {
        updatePiano();
    };


}

void Interpreter::renderMidiLog(ImFont* font)
{
    ImGui::PushFont(font);
    midiLog.Draw("Midi Log");
    ImGui::PopFont();
}


/*
 
int APIENTRY wwWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Interpreter& initiate = interp;

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MIDIRIPPER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MIDIRIPPER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}


LRESULT CALLBACK wWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // Enable layered window style
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

        // grayscale 250 = color key  (transparancy)
       SetLayeredWindowAttributes(hWnd, RGB(250, 250, 250), 0, LWA_COLORKEY);
        break;

    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        interp.handleMouseDrag(hWnd, message, wParam, lParam);
        break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDB_START:
                interp.startSampling();    
                break;
            case IDB_STOP:
                interp.stopSampling();
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case ID_REMOCT:
                interp.removeOctave(hWnd);
                break;
            case ID_ADDOCT:
                interp.addOctave(hWnd);
                break;
            case ID_OFFSETPLUS:
                interp.changeOffset(hWnd, 1);
                break;
                break;
            case ID_OFFSETMINUS:
                interp.changeOffset(hWnd, -1);
                break;
            case ID_pianoEMPTY:
                interp.setEmpty();
                break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_MOVE:
        interp.windowLocation.x = (int)(short)LOWORD(lParam);   // horizontal position 
        interp.windowLocation.y = (int)(short)HIWORD(lParam);   // vertical position 
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);  
            interp.renderTransparent(hdc);
            interp.makeRectangles(hdc);       
            EndPaint(hWnd, &ps);          
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL wInitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable
    POINT offset = interp.points[interp.point_topLeft];
    offset.y -= 150;

    // BUTTONS :DD
    
    HWND hWnd = CreateWindowEx(
        WS_EX_LAYERED,                // Extended window style
        szWindowClass,             // Class name
        L"My Application",            // Window title
        WS_OVERLAPPEDWINDOW,          // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, // Initial position (x, y)
        1200, 600,                     // Default width and height
        NULL, NULL, hInstance, NULL   // Parent window, menu, instance, additional data
    );


    HWND AddOctave = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"+ octave",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        200 + offset.x,         // x position 
        10 + offset.y,         // y position 
        100,        // Button width
        100,        // Button height
        hWnd,     // Parent window
        (HMENU)ID_ADDOCT,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND RemoveOctave = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"- octave",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10 + offset.x,         // x position 
        10 + offset.y,          // y position 
        100,        // Button width
        100,        // Button height
        hWnd,     // Parent window
        (HMENU)ID_REMOCT,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND centerOffsetPlus = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"skew +",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        130 + offset.x,         // x position 
        10 + offset.y,         // y position 
        50,        // Button width
        40,        // Button height
        hWnd,     // Parent window
        (HMENU)ID_OFFSETPLUS,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND centerOffsetMinus = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"skew -",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        130 + offset.x,         // x position 
        60 + offset.y,         // y position 
        50,        // Button width
        40,        // Button height
        hWnd,     // Parent window
        (HMENU)ID_OFFSETMINUS,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND pianoEMPTY = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"piano = empty",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        400 + offset.x,         // x position 
        10 + offset.y,         // y position 
        100,        // Button width
        50,        // Button height
        hWnd,     // Parent window
        (HMENU)ID_pianoEMPTY,       // No menu.
        (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

ATOM wMyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = wWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIDIRIPPER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MIDIRIPPER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

INT_PTR CALLBACK wAbout(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

*/
