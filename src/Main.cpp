#include "D3DApp.h"
#include "D3DUtils.h"
#include "CrossWindow/CrossWindow.h"
#include "Renderer.h"

int D3dAppMain(int argc, const char** argv)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        InitD3DApp app;

        constexpr D3DAppInfo appInfo
        {
            .windowSize = glm::ivec2(800, 600),
        };
        if (!app.Initialize(appInfo))
            return 1;
        
        return app.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), "HR Failed", MB_OK);
        return 1;
    }
}

void xmain(int argc, const char** argv)
{
    // 🖼️ Create a window
    xwin::EventQueue eventQueue;
    xwin::Window window;

    xwin::WindowDesc windowDesc;
    windowDesc.name = "MainWindow";
    windowDesc.title = "Hello Triangle";
    windowDesc.visible = true;
    windowDesc.width = 1280;
    windowDesc.height = 720;
    //windowDesc.fullscreen = true;
    window.create(windowDesc, eventQueue);

    // 📸 Create a renderer
    Renderer renderer(window);

    // 🏁 Engine loop
    bool isRunning = true;
    while (isRunning)
    {
        bool shouldRender = true;

        // ♻️ Update the event queue
        eventQueue.update();

        // 🎈 Iterate through that queue:
        while (!eventQueue.empty())
        {
            //Update Events
            const xwin::Event& event = eventQueue.front();

            if (event.type == xwin::EventType::Resize)
            {
                const xwin::ResizeData data = event.data.resize;
                renderer.resize(data.width, data.height);
                shouldRender = false;
            }

            if (event.type == xwin::EventType::Close)
            {
                window.close();
                shouldRender = false;
                isRunning = false;
            }

            eventQueue.pop();
        }

        // ✨ Update Visuals
        if (shouldRender)
        {
            renderer.render();
        }
    }

}