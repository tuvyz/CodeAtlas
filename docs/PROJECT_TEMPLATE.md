# Шаблон проекта (черновик)

Файл codeatlas.yaml в корне анализируемого проекта. Без него программа показывает только общий C++: функции, вызовы, чтение и запись полей.

```yaml
# Имя проекта на карте
name: VoxelGame

# Путь к списку флагов сборки (CMake: CMAKE_EXPORT_COMPILE_COMMANDS=ON)
compile_commands: build/compile_commands.json

# Папки, которые на карту не попадают
exclude: [Libs, Tools/WorldGen]

# Свои виды типов: как их узнать и как рисовать
types:
  component:
    template_arg_of: [view, get, try_get, emplace, remove]
  event:
    kind: component
    name_regex: "_[ucdr]Event$"

# Вызовы, которые считаются записью в тип из своего шаблонного аргумента
write_calls: [emplace, emplace_or_replace, remove, patch]
```

## Что хранит программа рядом с проектом

Папка .codeatlas в корне проекта:
- layout.json - координаты блоков и рамок файлов, цвета систем, пометки; можно коммитить вместе с проектом
- history.sqlite - снимки графа по коммитам; в git не кладётся

## Открытые вопросы

- Правило для сигналов и слотов Qt (connect) - какие поля нужны
- Как задавать цвета систем вручную, если автоматические не нравятся
