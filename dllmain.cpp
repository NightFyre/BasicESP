#include "pch.h"
#define HookFunction(funcAddr, classFunc, origFuncAddr) { MH_CreateHook((LPVOID)funcAddr, (LPVOID)classFunc, (LPVOID*)&origFuncAddr); MH_EnableHook((LPVOID)funcAddr); }
#define UnHookFunction(funcAddr) { MH_DisableHook((LPVOID)funcAddr); MH_RemoveHook((LPVOID)funcAddr); }
using namespace TaskForce;

/// GLOBALS
BOOL g_Running = TRUE;
BOOL g_Killswitch = FALSE;
HMODULE g_hModule = NULL;
static HANDLE g_moduleHandle = GetModuleHandle(NULL);
static DWORD g_processID = GetCurrentProcessId();
static uintptr_t dwGameBase = (uintptr_t)GetModuleHandle(NULL);

__int64 GetModuleAddr(INT64 addr, const char* module = NULL)
{
    return (__int64)((__int64)GetModuleHandleA(module) + addr);
}

uint64_t m_PostRender;
void* m_OriginalPostRender;
DWORD oPostRender = 0x000000;
static void __fastcall PostRender_hook(TaskForce::UGameViewportClient* viewport, TaskForce::UCanvas* canvas);       //  https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/UGameViewportClient/PostRender/
void PostRender_hook(UGameViewportClient* viewport, UCanvas* canvas)
{
    if (canvas)                                                                                                     //  https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/UCanvas/
    {
        auto WORLD = UWorld::GWorld; if ((*WORLD) == nullptr) return;                                                
        auto GAME_STATE = (*WORLD)->GameState; if ((GAME_STATE) == nullptr) return;                                //   https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Framework/GameMode/  
        auto PERSISTENT_LEVEL = (*WORLD)->PersistentLevel; if ((PERSISTENT_LEVEL) == nullptr) return;               


        //  Get Local Player Information
        //  These are classes custom to the game I have choses to use "TaskForce"
        auto LOCAL_PLAYER = (*WORLD)->OwningGameInstance->LocalPlayers[0]; if ((LOCAL_PLAYER) == nullptr) return;   
        auto PLAYER_CONTROLLER = LOCAL_PLAYER->PlayerController; if ((PLAYER_CONTROLLER) == nullptr) return;        //  https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Framework/Controller/PlayerController/
        auto PAWN = PLAYER_CONTROLLER->AcknowledgedPawn; if ((PAWN) == nullptr) return;                             //  https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Framework/Pawn/
        auto ROOT_COMPONENT = PAWN->RootComponent; if ((ROOT_COMPONENT) == nullptr) return;                         
        ATaskForceCharacter* pATFCharacter = static_cast<TaskForce::ATaskForceCharacter*>(PAWN);
        if (!pATFCharacter->IsAlive()) return;

        //  Begin Looping Entity Array
        auto Actor_Array = GAME_STATE->PlayerArray;         // PERSISTENT_LEVEL->Actors is another method of obtaining actors. You can loop through the levels as well as each levels actor array to find every single actor in the game world;
        for (int i = 0; i < Actor_Array.Count(); i++)
        {
            //  Get Entity Information
            auto Current_Actor = Actor_Array[i];
            if (Current_Actor == nullptr) continue;
            if (Current_Actor->RootComponent == nullptr) continue;
            auto pawn = Current_Actor->PawnPrivate; if ((pawn) == nullptr) continue;
            ACharacter* entACharacter = static_cast<TaskForce::ACharacter*>(pawn);
            ATaskForceCharacter* entATFCharacter = static_cast<TaskForce::ATaskForceCharacter*>(entACharacter);
            if (!entATFCharacter->IsAlive()) continue;

            //  Get Actor Bounds
            FVector actorBOX;
            FVector actorORIGIN;
            pawn->GetActorBounds(true, &actorORIGIN, &actorBOX);

            //  Project to Screen
            FVector2D screen;
            if (!PLAYER_CONTROLLER->ProjectWorldLocationToScreen(actorORIGIN, &screen, false)) continue;

            //  DRAW
            FVector2D heaven = { static_cast<float>(canvas->SizeX) / 2, 0 };
            FLinearColor Color = { 0, 255, 0, 255 };
            canvas->K2_DrawLine(heaven, screen, 1, Color);                                                              //  https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/UCanvas/K2_DrawLine/
        }
    }
    reinterpret_cast<decltype(&PostRender_hook)>(m_OriginalPostRender)(viewport, canvas);
}

extern void ESP_Thread()
{
    /// Initialize main unreal engine componets
    UObject::GObjects = reinterpret_cast<TUObjectArray*>(GetModuleAddr(0x3057438));
    FName::GNames = reinterpret_cast<FNamePool*>(GetModuleAddr(0x303FEC0));
    UWorld::GWorld = reinterpret_cast<UWorld**>(GetModuleAddr(0x3158590));

    /// Initialize Miinhook and hook our functions
    //  Use the source to figure out how to hook and utilize process event
    //  You will then need to link any function calls link "IsAlive(), GetActorBounds() & ProjectWorldLocationToScreen()" 
    //  Finding these functions & there addresses can be obtained with the GuidedHacking Dumper
    //  -   UObject Instance
    //  -   UObject::FindObject<UFunction>("Function <Function Object Name>");
    MH_Initialize();
    m_PostRender = GetModuleAddr(oPostRender);                                      //  Get Function Address
    HookFunction(m_PostRender, &PostRender_hook, m_OriginalPostRender);             //  Hook Function

    //  Main Loop
    //  We do absolutely nothing in here besides check if a key has been pressed to terminate the program
    while (g_Running)
        if (GetAsyncKeyState(VK_END) & 1) g_Running = FALSE;
    
    //  Unhook , Exit Minhook instance and exit the process thread.
    UnHookFunction(m_PostRender);
    MH_Uninitialize();
    FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    g_hModule = hModule;
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH: CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ESP_Thread, g_hModule, NULL, NULL);   break;
        case DLL_PROCESS_DETACH: g_Running = FALSE;                                                                     break;
    }
    return TRUE;
}