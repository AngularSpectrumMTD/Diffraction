#include "Diffraction.h"

#define APP_INVOKER_IMPLEMENTATION
#include "AppInvoker.h"

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPWSTR /*cmdline*/,
    _In_ int /*nCmdShow*/)
{
    Diffraction diffraction(EXECUTE_SIZE * 3, EXECUTE_SIZE);
    return AppInvoker::Execute(&diffraction, hInstance);
}
