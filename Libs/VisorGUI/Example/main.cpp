#include <VisorGUI/vgui.h>
#include <GL/glew.h>
#include <SDL.h>


// Пример VisorGUI: окно поверх «сцены» (здесь просто цветной фон) с кнопкой, галкой, полем ввода, списком и числом.
// Переменная окружения VGUI_EXAMPLE_SHOT=файл.bmp - сохранить кадр в файл и выйти (для проверки без человека)
int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("VisorGUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    glewInit();

    {
        vgui::GuiEngine gui(window);

        // Окно со своими виджетами: позиции и размеры - в пикселях при full hd, дальше всё масштабируется под окно
        auto *panel = new vgui::ScrollAreaWidget(vgui::MySize<double>(420, 360), glm::dvec2(40, 40),
                                                 {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                                 {vgui::ABSOLUTE_WINDOW_POSITION, vgui::ABSOLUTE_WINDOW_POSITION},
                                                 glm::dvec2(0, 0), L"Настройки сцены");
        panel->setDraggable(true);
        gui.addGuiObject(panel);

        auto *label = new vgui::Label(vgui::MySize<double>(360, 30), glm::dvec2(20, 20),
                                      {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                      {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                      glm::dvec2(0, 0), L"Нажатий: 0");
        panel->addChild(label);

        auto *button = new vgui::Button(vgui::MySize<double>(180, 36), glm::dvec2(20, 60),
                                        {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                        {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                        glm::dvec2(0, 0), L"Нажми");
        panel->addChild(button);

        auto *checkBox = new vgui::CheckBox(vgui::MySize<double>(160, 30), glm::dvec2(220, 63),
                                            {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                            {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                            glm::dvec2(0, 0), L"Туман");
        panel->addChild(checkBox);

        auto *lineEdit = new vgui::LineEdit(vgui::MySize<double>(360, 34), glm::dvec2(20, 115),
                                            {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                            {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                            glm::dvec2(0, 0), L"Текст можно править");
        panel->addChild(lineEdit);

        auto *comboBox = new vgui::ComboBox(vgui::MySize<double>(360, 34), glm::dvec2(20, 165), vgui::FIXED_SIZE,
                                            {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                            glm::dvec2(0, 0));
        comboBox->addItem(L"Утро", std::any(), true);
        comboBox->addItem(L"День");
        comboBox->addItem(L"Ночь");
        panel->addChild(comboBox);

        auto *valueEdit = new vgui::ValueEdit(&vgui::DOUBLE_Validator, vgui::MySize<double>(180, 34), glm::dvec2(20, 215),
                                              {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                              {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION},
                                              glm::dvec2(0, 0), {0, 10}, 2.5, 0.5);
        valueEdit->setToolTip(L"Колёсико или протяжка мышью вверх-вниз");
        panel->addChild(valueEdit);

        auto *progressBar = new vgui::ProgressBar(vgui::MySize<double>(360, 20), glm::dvec2(20, 270),
                                                  {vgui::FIXED_SIZE, vgui::FIXED_SIZE},
                                                  {vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION});
        progressBar->setValue(0.4f, vgui::Pixel(90, 170, 110));
        panel->addChild(progressBar);

        int clicksCount = 0;
        button->eventManager().subscribe<vgui::Button::Click_Event>(&gui, [&](const vgui::Button::Click_Event &) {
            ++clicksCount;
            label->setText(L"Нажатий: " + std::to_wstring(clicksCount));
        });

        const char *shotPath = SDL_getenv("VGUI_EXAMPLE_SHOT");
        int framesCount = 0;
        bool isRunning = true;
        while (isRunning) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT)
                    isRunning = false;
                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    glViewport(0, 0, event.window.data1, event.window.data2);
                    gui.updateScaleFactor(event.window.data2 / 1080.0, vgui::MySize<int>(event.window.data1, event.window.data2));
                }
                gui.handleEvent(event);
            }

            // «Сцена» под интерфейсом
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.18f, 0.22f, 0.27f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            gui.render();

            // Кадр в файл и выход
            if (shotPath != nullptr && ++framesCount == 5) {
                int width, height;
                SDL_GetWindowSize(window, &width, &height);
                SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 24, SDL_PIXELFORMAT_RGB24);
                glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
                // OpenGL отдаёт строки снизу вверх
                std::vector<unsigned char> row(surface->pitch);
                for (int y = 0; y < height / 2; ++y) {
                    unsigned char *top = static_cast<unsigned char*>(surface->pixels) + y * surface->pitch;
                    unsigned char *bottom = static_cast<unsigned char*>(surface->pixels) + (height - 1 - y) * surface->pitch;
                    std::copy(top, top + surface->pitch, row.data());
                    std::copy(bottom, bottom + surface->pitch, top);
                    std::copy(row.data(), row.data() + surface->pitch, bottom);
                }
                SDL_SaveBMP(surface, shotPath);
                SDL_FreeSurface(surface);
                isRunning = false;
            }

            SDL_GL_SwapWindow(window);
        }
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
