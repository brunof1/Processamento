#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "comctl32.lib")

#define ID_BUTTON_START 101
#define ID_BUTTON_PAUSE 102
#define ID_BUTTON_CANCEL 103
#define ID_PROGRESS_BAR 104

#define SOURCE_DIR L"E:\\Backup\\Compactada"
#define DEST_DIR L"F:\\Mega\\Escritório\\Backup Domínio"
#define ICON_PATH "C:\\Users\\Bruno\\Desktop\\icon.ico"

HWND hwndProgress;
BOOL isPaused = FALSE;
BOOL isCancelled = FALSE;

// Estrutura para passar dados para a thread
typedef struct {
    HWND hwnd;
    WCHAR* sourcePath;
    WCHAR* destPath;
} ThreadData;

// Função para obter o arquivo mais recente
WCHAR* getMostRecentFile(const WCHAR* directory) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR fullPath[MAX_PATH];
    WCHAR* mostRecentFile = NULL;
    FILETIME mostRecentTime = {0};

    wcscpy(fullPath, directory);
    wcscat(fullPath, L"\\*");

    hFind = FindFirstFileW(fullPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (CompareFileTime(&findFileData.ftLastWriteTime, &mostRecentTime) > 0) {
                mostRecentTime = findFileData.ftLastWriteTime;
                free(mostRecentFile);
                mostRecentFile = _wcsdup(findFileData.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);

    FindClose(hFind);
    return mostRecentFile;
}

// Função para verificar se o arquivo é de hoje
BOOL isFileFromToday(const WCHAR* filePath) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(filePath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    FindClose(hFind);

    SYSTEMTIME stFile, stNow;
    FileTimeToSystemTime(&findFileData.ftLastWriteTime, &stFile);
    GetLocalTime(&stNow);

    return (stFile.wYear == stNow.wYear &&
            stFile.wMonth == stNow.wMonth &&
            stFile.wDay == stNow.wDay);
}

// Função para copiar arquivo com pausa e cancelamento
BOOL copyFileWithProgress(HWND hwnd, const WCHAR* source, const WCHAR* destination) {
    HANDLE hSource = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSource == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    HANDLE hDest = CreateFileW(destination, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDest == INVALID_HANDLE_VALUE) {
        CloseHandle(hSource);
        return FALSE;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hSource, &fileSize);

    DWORD bufferSize = 1024 * 1024; // 1MB buffer
    BYTE* buffer = (BYTE*)malloc(bufferSize);
    DWORD bytesRead, bytesWritten, totalBytesWritten = 0;

    while (totalBytesWritten < fileSize.QuadPart) {
        if (isCancelled) {
            free(buffer);
            CloseHandle(hSource);
            CloseHandle(hDest);
            DeleteFileW(destination);
            return FALSE;
        }

        while (isPaused) {
            Sleep(100);  // Wait while paused
            if (isCancelled) break;
        }

        if (!ReadFile(hSource, buffer, bufferSize, &bytesRead, NULL)) {
            break;
        }

        if (!WriteFile(hDest, buffer, bytesRead, &bytesWritten, NULL)) {
            break;
        }

        totalBytesWritten += bytesWritten;
        SendMessage(hwndProgress, PBM_SETPOS, (WPARAM)((totalBytesWritten * 100) / fileSize.QuadPart), 0);
    }

    free(buffer);
    CloseHandle(hSource);
    CloseHandle(hDest);

    return (totalBytesWritten == fileSize.QuadPart);
}

// Função para deletar arquivos mais antigos
void deleteOlderFiles(const WCHAR* directory, const FILETIME* referenceTime) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR fullPath[MAX_PATH];

    wcscpy(fullPath, directory);
    wcscat(fullPath, L"\\*");

    hFind = FindFirstFileW(fullPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (CompareFileTime(&findFileData.ftLastWriteTime, referenceTime) < 0) {
                WCHAR filePath[MAX_PATH];
                wcscpy(filePath, directory);
                wcscat(filePath, L"\\");
                wcscat(filePath, findFileData.cFileName);
                DeleteFileW(filePath);
            }
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);

    FindClose(hFind);
}

// Thread para executar o backup
DWORD WINAPI BackupThread(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    HWND hwnd = data->hwnd;
    WCHAR* sourcePath = data->sourcePath;
    WCHAR* destPath = data->destPath;

    if (!isFileFromToday(sourcePath)) {
        MessageBoxW(hwnd, L"AVISO: O arquivo mais recente não é de hoje. Backup não realizado.", L"Aviso", MB_OK | MB_ICONWARNING);
        free(data->sourcePath);
        free(data->destPath);
        free(data);
        return 1;
    }

    if (copyFileWithProgress(hwnd, sourcePath, destPath)) {
        MessageBoxW(hwnd, L"Backup concluído com sucesso!", L"Sucesso", MB_OK | MB_ICONINFORMATION);

        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW(destPath, &findFileData);
        if (hFind != INVALID_HANDLE_VALUE) {
            deleteOlderFiles(DEST_DIR, &findFileData.ftLastWriteTime);
            FindClose(hFind);
        }
    } else if (!isCancelled) {
        MessageBoxW(hwnd, L"Erro ao fazer o backup.", L"Erro", MB_OK | MB_ICONERROR);
    }

    free(data->sourcePath);
    free(data->destPath);
    free(data);
    return 0;
}

// Procedimento da janela principal
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            // Criar controles
            CreateWindowW(L"BUTTON", L"Iniciar Backup", WS_VISIBLE | WS_CHILD, 10, 10, 100, 30, hwnd, (HMENU)ID_BUTTON_START, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Pausar", WS_VISIBLE | WS_CHILD, 120, 10, 100, 30, hwnd, (HMENU)ID_BUTTON_PAUSE, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Cancelar", WS_VISIBLE | WS_CHILD, 230, 10, 100, 30, hwnd, (HMENU)ID_BUTTON_CANCEL, NULL, NULL);
            hwndProgress = CreateWindowW(PROGRESS_CLASSW, NULL, WS_VISIBLE | WS_CHILD, 10, 50, 320, 20, hwnd, (HMENU)ID_PROGRESS_BAR, NULL, NULL);
            SendMessage(hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            break;
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case ID_BUTTON_START: {
                    WCHAR* mostRecentFile = getMostRecentFile(SOURCE_DIR);
                    if (mostRecentFile) {
                        WCHAR sourcePath[MAX_PATH], destPath[MAX_PATH];
                        wcscpy(sourcePath, SOURCE_DIR);
                        wcscat(sourcePath, L"\\");
                        wcscat(sourcePath, mostRecentFile);

                        wcscpy(destPath, DEST_DIR);
                        wcscat(destPath, L"\\");
                        wcscat(destPath, mostRecentFile);

                        ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
                        data->hwnd = hwnd;
                        data->sourcePath = _wcsdup(sourcePath);
                        data->destPath = _wcsdup(destPath);

                        CreateThread(NULL, 0, BackupThread, data, 0, NULL);
                        free(mostRecentFile);
                    } else {
                        MessageBoxW(hwnd, L"Nenhum arquivo encontrado na pasta de origem.", L"Erro", MB_OK | MB_ICONERROR);
                    }
                    break;
                }
                case ID_BUTTON_PAUSE:
                    isPaused = !isPaused;
                    SetWindowTextW((HWND)lParam, isPaused ? L"Retomar" : L"Pausar");
                    break;
                case ID_BUTTON_CANCEL:
                    isCancelled = TRUE;
                    break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = L"BackupWindowClass";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"BackupWindowClass", L"Backup da Domínio", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 120, NULL, NULL, hInstance, NULL);

    if(hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
