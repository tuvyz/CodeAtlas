#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <SDL.h>


// Настройки программы (общие для всех проектов): размеры подписей и блоков на холсте, связи, колесо, разбор.
// Хранятся в settings.txt рядом с программой (папка недоступна для записи - в папке пользователя), в проект не пишется ничего
struct Settings {
    double fileLabelSize = 26;   // Высота имени файла, пиксели экрана (одна при любом масштабе)
    double folderLabelSize = 28; // Высота имени папки верхнего уровня, пиксели экрана (вложенные мельче)
    double nodeTextSize = 13;    // Высота имени в блоке, единицы холста (блок растёт вместе с ним)
    double noteTextSize = 15;    // Высота текста пометки, единицы холста
    double edgeThickness = 1.1;  // Толщина связи, пиксели экрана (у выбранного блока - в 1.6 раза толще)
    double wheelZoomStep = 18;   // Шаг масштаба колесом, проценты
    int scanThreads = 0;         // Потоков разбора (0 - половина ядер, не больше 8)

    // Загрузка (нет файла - значения по умолчанию) и сохранение
    void load();
    bool save() const;

    // Файл настроек
    static std::filesystem::path filePath();
};

// Настройки программы (одни на процесс)
Settings &settings();


#endif // SETTINGS_H
