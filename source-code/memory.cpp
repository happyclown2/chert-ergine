#define _WIN32_WINNT _WIN32_WINNT_WIN8
#include <windows.h>
#include <process.h>
#include <psapi.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include <map>
#include "imgui_impl_sdlrenderer3.h"
//#include <processthreadsapi.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>

using u32 = unsigned int;
using u64 = unsigned long long;
using i32 = int;
using i64 = long long;

template <typename T>
struct Slice {
    T *ptr;
    u64 len;

    T &operator[](u64 index) {
        return ptr[index];
    }

    const T &operator[](u64 index) const {
        return ptr[index];
    }

    Slice subslice(u64 start, u64 size) const {
        return Slice{this->ptr + start, size};
    }
};

template <typename T>
struct CSlice {
    const T *ptr;
    u64 len;

    const T &operator[](u64 index) const {
        return ptr[index];
    }

    CSlice subslice(u64 start, u64 size) const {
        return CSlice{this->ptr + start, size};
    }
};

template <typename T> class GoodArray {
public:
    T *ptr = nullptr;
    unsigned long count;

    GoodArray(unsigned long size) {
        ptr = new T[size];
        count = size;
    }
    const T &operator[](u64 index) const {
        return ptr[index];
    }
    T &operator[](u64 index) {
        return ptr[index];
    }
    ~GoodArray(){
        delete[] ptr;
    }
    unsigned long size_of() const {
        return sizeof(T)*count;
    }
};

struct processes{
    DWORD pid;
    std::string mainmodules;
    std::size_t hash;
public:
    processes(const char *filename, DWORD pids): mainmodules(filename) {
        pid = pids;
        std::hash<std::string_view> hasher;
        hash = hasher(mainmodules);
    } 
    void get_info(){
        std::cout << "PID of process " << mainmodules << " : " << pid << std::endl;
    }
};

bool process_compare(const processes& a, const processes& b){
    return (a.pid > b.pid);
}

bool find_hash(const std::vector<processes>& haystack, std::size_t needle_hash){
    for(processes i : haystack){
        if(i.hash == needle_hash){
            return true;
        }
    }
    return false;
}
struct main_n_background_process{
    std::vector<processes> background_infos;
    std::vector<processes> main_infos;
    public:
    main_n_background_process(std::vector<processes> &m, std::vector<processes> &n){
        background_infos = m;
        main_infos = n;
    }
};
struct window_data{
    unsigned long id;
    HWND window_handle;
};
BOOL is_main_window(HWND handle){
    return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
}
BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam){
    window_data& data = *(window_data*)lParam;
    unsigned int processid = 0;
    GetWindowThreadProcessId(handle, &processid);
    if(processid != data.id || !is_main_window(handle)){
        return TRUE;
    }
    data.window_handle = handle;
    return FALSE;
}
HWND find_main_window(unsigned int process_id){
    window_data data;
    data.id = process_id;
    data.window_handle = 0;
    EnumWindows((WNDENUMPROC)&enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}
class Managemem{
    public:
    DWORD *get = nullptr;
    DWORD bytes = 0;
    PROCESS_MEMORY_COUNTERS count;
    LPVOID buf;
    HANDLE opened_proc;
    int pid_count = 0;

    const static unsigned long MAX_PID_COUNT = 1024;
    
    Managemem() {
        get = new DWORD[MAX_PID_COUNT];

        int pls = EnumProcesses(get, MAX_PID_COUNT*sizeof(DWORD), &bytes);
        if(pls == 0){
            std::cout << "Failed to get processes: " << GetLastError() << std::endl;

        } else {
            pid_count = bytes / sizeof(DWORD);
            std::cout << "Got " << pid_count << " pids" << std::endl;
        }
    }
    
    DWORD *begin() const {
        return get;
    }

    DWORD *end() const {
        return get + pid_count;
    }
    main_n_background_process gethandles() {
        std::vector<processes> process_infos;
        std::vector<processes> mainapps;
        
        for (DWORD pid : *this){
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, true, pid);
            if (process != nullptr){
                //Just because it's the same size, not actually max pid count
                GoodArray<char> modex(MAX_PID_COUNT);
                GoodArray<char> imgex(MAX_PID_COUNT);
                unsigned hi = sizeof(imgex.ptr)*MAX_PID_COUNT;
                DWORD modubyte;
                GoodArray<HMODULE> modules(32);
                //GetProcessImageFileNameA(process, imgex.ptr, imgex.size_of());
                QueryFullProcessImageNameA(process, PROCESS_NAME_NATIVE, imgex.ptr, &hi);
                GetModuleFileNameExA(process, NULL, modex.ptr, modex.size_of());
                //std::cout << "Process name queried: '" << imgex.ptr << "'\n" << std::endl;
                //std::cout<< "Your funky name for pid #" << pid << " is: " << modex.ptr << std::endl;
                if(EnumProcessModules(process, modules.ptr, modules.size_of(), &modubyte)){
                    const auto moducount = modubyte / sizeof(HMODULE);
                    for(int i = 0; i < moducount; i++){
                        GoodArray<char> module_file_name(1024);
                        if(GetModuleFileNameExA(process, modules.ptr[i], module_file_name.ptr, module_file_name.size_of()) != 0){
                            if(i == 0){
                                processes NewProcess(module_file_name.ptr, pid);
                                if(!find_hash(process_infos, NewProcess.hash)){
                                    process_infos.push_back(NewProcess);
                                    MEMORY_PRIORITY_INFORMATION get_info;
                                    //std::cout << module_file_name.ptr << std::endl;
                                }
                            }
                        } else{
                            //std:: cout << "Didn't get executable from process " << pid << " cuz " << GetLastError() << std::endl; 
                        }
                    }
                } else {
                    //std::cout <<" Failed to enum this process #" << pid << " because: " << GetLastError() << std::endl;
                }
                //bool read = ReadProcessMemory(process, process[0], );
                CloseHandle(process);
            } else {
                std::cout <<" Failed  to open process #" << pid << " because: " << GetLastError() << std::endl;
            }
        }
        //std::sort(process_infos.begin(), process_infos.end(), &process_compare);
        //return process_infos;
        std::sort(mainapps.begin(), mainapps.end(), &process_compare);
        return main_n_background_process(process_infos, mainapps);
    }



    bool read_and_display(processes *process, unsigned char look, std::vector<int> &put){
        put.clear();
        opened_proc = OpenProcess(PROCESS_VM_READ, false, process->pid);
        if(opened_proc == INVALID_HANDLE_VALUE){
            std::cout << "Invalid process" << std::endl;
        }
        auto buf = GoodArray<unsigned char>(50000);
        UINT8* baseaddr = 0;
        SIZE_T read = 0;
        while(ReadProcessMemory(opened_proc, baseaddr, buf.ptr, buf.size_of(), &read) == 0){
            //std::cout << "Error on reading memory " << reinterpret_cast<size_t>(baseaddr) << ": " << GetLastError() << std::endl;
            baseaddr += 4096;
        }
        const SIZE_T lock = reinterpret_cast<SIZE_T>(baseaddr);
        // xxd makes this:  000060d0: d45e a748 efaf 3d87 d89f d033 f495 83fd  .^.H..=....3....
        for(int i = 0; i < read; i++){
            if(buf[i] == look){
                char buffer[17] = {0};
                printf("%x   : value = %d\r\n", i, buf[i]);
                const int written_size = sprintf(buffer, "%x", &buf[i]);
                if (written_size < 0) {
                        std::cerr << "Failed to sprintf my buffer!" << std::endl;
                        continue;
                }
                put.push_back(lock+i);
            }
        }
        std::cout << "Finished reading memory" << std::endl;
        return true;
    }

    bool write(unsigned char to_write, int base){
        unsigned char* point = 0;point+=base;
        unsigned char prev = *point;
        *point = to_write;
        if(prev != *point){
            std::cout << "Was able to write" << std::endl;
            return true;
        }
        else{
            std::cout << "Couldn't write to destination because bad value or invalid memory" << "\nAttempted position: " << base << " to write " << to_write << std::endl;
            return false;
        }
    }
    
    inline void close(){
        CloseHandle(opened_proc);
    }

    ~Managemem() {
        delete[] get;
    }
};

int main(int argc, char const *argv[])
{
    // setup SDL
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

    SDL_Window *const my_window = SDL_CreateWindow("ChertErgine", 800, 600, 0);
    SDL_Renderer *const my_renderer = SDL_CreateRenderer(my_window, nullptr);

    // setup imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(my_window, my_renderer);
    ImGui_ImplSDLRenderer3_Init(my_renderer);
  
    // main code sections
    Managemem hey {};
    main_n_background_process mainmodules = hey.gethandles();
    

    bool running = true;
    int lock = 0;
    std::vector<int> get;
    processes* get_clicked_process;
    int last_clicked_memory = 0;
    bool wantsmodified = false;
    while (running) {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(my_window))
                running = false;
        }


        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        /// where our main imgui calls should go
        ImGui::Begin("Cherter's Engine");
        bool popup = false;
        for(processes p : mainmodules.background_infos){
            HWND newhand = find_main_window(p.pid);
            if(newhand != nullptr){
                if (ImGui::Button(p.mainmodules.c_str())) {
                    std::cout << "got " << p.mainmodules << " and ID " << p.pid << std::endl;
                    ImGui::OpenPopup("my_popup_id");
                    get_clicked_process = &p;
                }
            }
        }
        ImGui::InputInt("Memory Lock(type unsigned char)", &lock);
        if(ImGui::BeginPopup("my_popup_id")){
            if(ImGui::Button("Close")){
                hey.close();
                ImGui::CloseCurrentPopup();
            }
            if(ImGui::Button("Read Memory")){
                hey.read_and_display(get_clicked_process, static_cast<unsigned char>(lock), get);
            }
            ImGui::EndPopup();
        }
        if(get.size() != 0){
            ImGui::OpenPopup("memory_slots");
        }
        if(ImGui::BeginPopup("memory_slots")){
            if (get.size() != 0) {
                ImGui::Text("Got Memory:");
            } else {
                ImGui::Text("No Memory!");
            }
            for(int i = 0; i < get.size(); i++){
                ImGui::PushID(i);
                char buffer[17] = {0};
                const int written_amount = sprintf(buffer, "%x", get[i]);
                std::string_view view(buffer, written_amount);
                std::string mymemory_str(view);
                if(ImGui::Button(buffer)){
                    std::cout << const_cast<const char*>(buffer) << std::endl;
                    wantsmodified = true;
                    last_clicked_memory = get[i];
                }
                ImGui::PopID();
            }
            char buf2[17] = {0};
            sprintf(buf2, "%x", last_clicked_memory);
            ImGui::Text(reinterpret_cast<const char*>(buf2));
            if(ImGui::Button("Modify Memory(uses last clicked memory)")){
                if(hey.write((unsigned char)lock, last_clicked_memory)){
                    std::cout << "Yay you wrote to memory" << std::endl;
                }
            }
            if(ImGui::Button("Refine Memory")){
                std::vector<int> temp;
                for(int i = 0; i < get.size(); i++){
                    UINT8* pointer = 0;
                    pointer += get[i];
                    if(*pointer == lock){
                        temp.push_back(get[i]);
                    }
                }
                get = temp;
            }
            if(wantsmodified){

            }
            ImGui::InputInt("Target", &lock, ImGuiInputTextFlags_EnterReturnsTrue);
            if(ImGui::Button("Close")){
                get.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();

        }
        ImGui::End();

        // Finalize Rendering
        ImGui::Render();
        SDL_SetRenderDrawColorFloat(my_renderer, 0.5f, 0.5f, 1.0f, 1.0f);
        SDL_RenderClear(my_renderer);

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), my_renderer);
        SDL_RenderPresent(my_renderer);
    }


    // destroy everything!
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // sdl destroy
    SDL_DestroyWindow(my_window);
    SDL_Quit();
    return 0;
}
// cmake -S . -B build   ## to configure, run once
// cmake --build build   ## to build, run every h/cpp change

// i386   is windows 32-bit
// x86_64 is windows 64-bit
// amd64 may also be 64-bit

// CXX=x86_64-pc-msys-g++.exe cmake --build build