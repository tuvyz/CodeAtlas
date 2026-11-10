#pragma once

#include <list>
#include <map>
#include <memory>
#include <cstddef>


namespace vgui::detail {


// Кэш
template<typename K, typename V>
class LruCache {
private:
    // Ограничение размера кэша
    size_t cacheSize = 30;

    // Список для отслеживания порядка использования объектов
    std::list<std::pair<K, std::shared_ptr<V>>> cacheList;

    // Map для быстрого доступа к объектам в кэше
    std::map<K, typename std::list<std::pair<K, std::shared_ptr<V>>>::iterator> cacheMap;

    // Удаление наименее недавно использованного объекта
    void evict() {
        if (cacheList.empty()) return;

        auto last = cacheList.end();
        last--;
        cacheMap.erase(last->first);
        cacheList.pop_back();
    }

public:

    LruCache(){}
    LruCache(size_t size) : cacheSize(size) {}

    // Установить размер кэша
    void setCacheSize(size_t cacheSize) {
        this->cacheSize = cacheSize;
    }

    // Добавление объекта в кэш
    void put(const K& key, std::shared_ptr<V> object) {
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            cacheList.erase(it->second);
            cacheMap.erase(it);
        }

        cacheList.push_front(std::make_pair(key, object));
        cacheMap[key] = cacheList.begin();

        if (cacheMap.size() > cacheSize) {
            evict();
        }
    }

    // Получение объекта из кэша
    std::shared_ptr<V> get(const K& key) {
        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) {
            return nullptr;
        } else {
            cacheList.splice(cacheList.begin(), cacheList, it->second);
            return it->second->second;
        }
    }

};


} // namespace vgui::detail
