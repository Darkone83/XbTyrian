// main.cpp — Xbox entry point for OpenTyrian

#include <xtl.h>

int tyrian_main(int argc, char* argv[]);

void __cdecl main()
{
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = FALSE;
    pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.Flags = 0;

    Direct3DCreate8(D3D_SDK_VERSION);
    Direct3D_CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, NULL);

    tyrian_main(0, NULL);
}