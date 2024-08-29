//
// Created by BramTeurlings on 22-8-2024.
//

#pragma once

#include <Windows.h>
#include <delayimp.h>

// Todo: This can just be a single string instead, checking this for each DLL seems horribly inefficient.
std::vector<std::string> sr_dll_names = {"Glog.dll", "Opencv_world343.dll", "DimencoWeaving.dll", "SimulatedRealityCore.dll", "SimulatedRealityDisplays.dll", "SimulatedRealityFacetrackers.dll", "SimulatedRealityDirectX.dll", "DimencoWeaving32.dll", "SimulatedRealityCore32.dll", "SimulatedRealityDisplays32.dll", "SimulatedRealityFacetrackers32.dll", "SimulatedRealityDirectX32.dll"};

FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    std::string requested_dll;
    switch (dliNotify) {
        case dliStartProcessing:

            // If you want to return control to the helper, return 0.
            // Otherwise, return a pointer to a FARPROC helper function
            // that will be used instead, thereby bypassing the rest
            // of the helper.

            break;

        case dliNotePreLoadLibrary:

            // If you want to return control to the helper, return 0.
            // Otherwise, return your own HMODULE to be used by the
            // helper instead of having it call LoadLibrary itself.

            // Check if the DLL in question is one we want to delayed load.
            requested_dll = pdli->szDll;
            for (int i = 0; i < sr_dll_names.size(); i++) {
                if (sr_dll_names[i].find(requested_dll) != std::string::npos) {
                    // DLL matches one we want to load, let's load it
                    const HMODULE hModule = LoadLibraryA((requested_dll + ".dll").c_str());
                    const DWORD errorCode = GetLastError();
                    if (errorCode == ERROR_MOD_NOT_FOUND) {
                        std::cout << "Module not found (ERROR_MOD_NOT_FOUND)" << std::endl;
                        return 0;
                    }
                    if (hModule) {
                        return reinterpret_cast<FARPROC>(hModule);
                    }
                }
            }

            break;

        case dliNotePreGetProcAddress:

            // If you want to return control to the helper, return 0.
            // If you choose you may supply your own FARPROC function
            // address and bypass the helper's call to GetProcAddress.

            break;

        case dliFailLoadLib:

            // LoadLibrary failed.
            // If you don't want to handle this failure yourself, return 0.
            // In this case the helper will raise an exception
            // (ERROR_MOD_NOT_FOUND) and exit.
            // If you want to handle the failure by loading an alternate
            // DLL (for example), then return the HMODULE for
            // the alternate DLL. The helper will continue execution with
            // this alternate DLL and attempt to find the
            // requested entrypoint via GetProcAddress.
            throw std::runtime_error("Failed to load library");

            break;

        case dliFailGetProc:

            // GetProcAddress failed.
            // If you don't want to handle this failure yourself, return 0.
            // In this case the helper will raise an exception
            // (ERROR_PROC_NOT_FOUND) and exit.
            // If you choose, you may handle the failure by returning
            // an alternate FARPROC function address.

            break;

        case dliNoteEndProcessing:

            // This notification is called after all processing is done.
            // There is no opportunity for modifying the helper's behavior
            // at this point except by longjmp()/throw()/RaiseException.
            // No return value is processed.

            break;

        default :

            return NULL;
    }

    return NULL;
}

ExternC const PfnDliHook __pfnDliNotifyHook2 = delayHook;
ExternC const PfnDliHook __pfnDliFailureHook2 = delayHook;
