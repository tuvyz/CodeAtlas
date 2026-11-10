#!/bin/bash
# Разнос библиотеки из этой папки в другие её копии: клон tuvyz/VisorGUI и Libs/VisorGUI других проектов.
# Пример: Libs/VisorGUI/sync.sh ../../VisorGUI ../../OtherProgram/Libs/VisorGUI
# Копируются только отличающиеся файлы (с новым временем - сборка их пересоберёт), лишние в копии удаляются, .git не трогается.
# Время файлов не переносится нарочно: со старым временем ninja и make не видят, что файл сменился, и берут старые объектники.
# Потом в каждом репозитории - коммит и пуш
set -e
SRC=$(cd "$(dirname "$0")" && pwd)
for DST in "$@"; do
    mkdir -p "$DST"
    DST=$(cd "$DST" && pwd)
    # Новые и изменённые
    (cd "$SRC" && find . -type f ! -path "./.git/*" ! -path "./.git") | while read -r FILE; do
        if ! cmp -s "$SRC/$FILE" "$DST/$FILE"; then
            mkdir -p "$(dirname "$DST/$FILE")"
            cp "$SRC/$FILE" "$DST/$FILE"
        fi
    done
    # Лишние
    (cd "$DST" && find . -type f ! -path "./.git/*" ! -path "./.git") | while read -r FILE; do
        [ -e "$SRC/$FILE" ] || rm "$DST/$FILE"
    done
    find "$DST" -mindepth 1 -type d -empty ! -path "*/.git*" -delete
    echo "VisorGUI -> $DST"
done
