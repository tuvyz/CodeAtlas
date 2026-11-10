#include "Settings.h"


// Настройки программы (одни на процесс)
Settings &settings() {
    static Settings instance;
    return instance;
}


// Файл настроек
std::filesystem::path Settings::filePath() {
    // Рядом с программой, как папки проектов; не пишется туда - в папке пользователя
    std::error_code errorCode;
    char *basePath = SDL_GetBasePath();
    std::filesystem::path result = basePath != nullptr ? std::filesystem::u8path(basePath) / "settings.txt" : std::filesystem::path();
    SDL_free(basePath);
    if (!result.empty()) {
        std::ofstream probe(result, std::ios::app);
        if (probe)
            return result;
    }
    char *prefPath = SDL_GetPrefPath("tuvyz", "CodeAtlas");
    result = prefPath != nullptr ? std::filesystem::u8path(prefPath) / "settings.txt" : std::filesystem::path("settings.txt");
    SDL_free(prefPath);
    return result;
}


// Загрузка (нет файла - значения по умолчанию) и сохранение
void Settings::load() {
    *this = Settings();
    std::ifstream file(filePath(), std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;
        std::string key = line.substr(0, colonPos);
        double value = 0;
        std::istringstream valueStream(line.substr(colonPos + 1));
        valueStream.imbue(std::locale::classic());
        if (!(valueStream >> value))
            continue;

        // Значение за пределами ползунка окна настроек обрезается
        if (key == "file_title_size")
            fileTitleSize = std::clamp(value, 12.0, 120.0);
        else if (key == "folder_title_size")
            folderTitleSize = std::clamp(value, 12.0, 300.0);
        else if (key == "node_text_size")
            nodeTextSize = std::clamp(value, 9.0, 32.0);
        else if (key == "note_text_size")
            noteTextSize = std::clamp(value, 9.0, 40.0);
        else if (key == "edge_thickness")
            edgeThickness = std::clamp(value, 0.5, 5.0);
        else if (key == "wheel_zoom_step")
            wheelZoomStep = std::clamp(value, 5.0, 60.0);
        else if (key == "scan_threads")
            scanThreads = std::clamp(int(value), 0, 64);
        else if (key == "node_size_mode")
            nodeSizeMode = std::clamp(int(value), 0, 2);
        else if (key == "lines_per_step")
            linesPerStep = std::clamp(value, 5.0, 200.0);
    }
}

bool Settings::save() const {
    std::ofstream file(filePath(), std::ios::binary);
    if (!file)
        return false;
    file.imbue(std::locale::classic());
    file << "file_title_size: " << fileTitleSize << '\n'
         << "folder_title_size: " << folderTitleSize << '\n'
         << "node_text_size: " << nodeTextSize << '\n'
         << "note_text_size: " << noteTextSize << '\n'
         << "edge_thickness: " << edgeThickness << '\n'
         << "wheel_zoom_step: " << wheelZoomStep << '\n'
         << "scan_threads: " << scanThreads << '\n'
         << "node_size_mode: " << nodeSizeMode << '\n'
         << "lines_per_step: " << linesPerStep << '\n';
    return bool(file);
}
