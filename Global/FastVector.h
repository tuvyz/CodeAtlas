#pragma once

#include "Global/Logging_Fmt.h"
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <initializer_list>
#include <utility>



template<typename T>
class FastVector {
public:
    
    
    
    // Сериализация массива
    void serialize(FastVector<uint8_t>& buffer, int32_t & offset) const {
        uint32_t size = static_cast<uint32_t >(_size);
        buffer.resize(offset + sizeof(uint32_t ) + sizeof(T) * size);
        std::memcpy(buffer.data() + offset, &size, sizeof(uint32_t ));
        offset += sizeof(uint32_t );
        if (size > 0) {
            std::memcpy(buffer.data() + offset, arr, sizeof(T) * size);
            offset += sizeof(T) * size;
        }
    }
    // Десериализация массива
    void deserialize(const FastVector<uint8_t>& buffer, int32_t & offset) {
        uint32_t size;
        std::memcpy(&size, buffer.data() + offset, sizeof(uint32_t ));
        offset += sizeof(uint32_t );
        resize(size);
        if (size > 0) {
            std::memcpy(arr, buffer.data() + offset, sizeof(T) * size);
            offset += sizeof(T) * size;
        }
    }
    
    
    
    FastVector() {}
    
    FastVector(size_t capacity) {
        setCapacity(capacity);
    }
    
    
    FastVector(FastVector<T> const &other) {
        copy(other);
    }
    void operator = (FastVector<T> const &other) {
        if (this == &other)
            return;
        copy(other);
    }
    FastVector(FastVector<T>&& other) noexcept {
        grab(other);
    }
    FastVector& operator=(FastVector<T>&& other) noexcept {
        if (this == &other)
            return *this;
        grab(other);
        return *this;
    }
    
    
    FastVector(void const* data, size_t size) {
        copy(data, size);
    }
    
    FastVector(std::initializer_list<T> init) {
        copy(init.begin(), init.size());
    }
    
    ~FastVector(){
        hardClear();
    }
    
    
    
    void push(T const& data) {
        _size++;
        grow(_size);
        arr[_size - 1] = data;
    }
    void push(T&& data) {
        _size++;
        grow(_size);
        arr[_size - 1] = std::move(data); // перемещение
    }
    
    
    void pushRange(T const* data, size_t count) {
        size_t newSize = _size + count;
        grow(newSize);
        std::memcpy(arr + _size, data, count * sizeof(T));
        _size = newSize; // Обновляем размер
    }
    
    
    
    
    T& back() {
        return arr[_size - 1];
    }
    const T& back() const {
        return arr[_size - 1];
    }
    
    T& front() {
        return arr[0];
    }
    const T& front() const {
        return arr[0];
    }
    
    
    
    void setCapacity(size_t capacity) {
        if (capacity == _capacity)
            return;
        T* newArr = new T[capacity];
        size_t numToCopy = std::min(_capacity, capacity);
        for (size_t i = 0; i < numToCopy; ++i) {
            newArr[i] = std::move(arr[i]);
        }
        delete[] arr;
        arr = newArr;
        _capacity = capacity;
        
        // При уменьшении ёмкости размер не должен выходить за пределы буфера
        if (_size > _capacity)
            _size = _capacity;
    }
    void setInitialCapacity(size_t initialCapacity) {
        _initialCapacity = initialCapacity;
    }
    void setCapacityFactor(float capacityFactor) {
        _capacityFactor = capacityFactor;
    }
    
    
    const size_t size() const {
        return _size;
    }
    const size_t capacity() const {
        return _capacity;
    }
    
    
    const bool empty() const {
        return _size == 0;
    }
    
    
    // Очистка с удалением массива
    void hardClear() {
        delete [] arr;
        arr = nullptr;
        _size = 0;
        _capacity = 0;
    }
    
    // Очистка без удаления массива
    void fastClear() {
        _size = 0;
    }
    
    // Очистка с удалением массива, если ёмкость перевалила за допустимый предел
    void autoClear(size_t maximumKeptBytes = 64 * 1024) {
        if (_capacity * sizeof(T) > maximumKeptBytes)
            hardClear();
        else
            fastClear();
    }
    
    
    T& get(size_t id) {
        return arr[id];
    }
    T& operator [] (size_t id) {
        return arr[id];
    }
    
    const T& get(size_t id) const {
        return arr[id];
    }
    const T& operator [] (size_t id) const {
        return arr[id];
    }
    
    
    T* data() {
        return arr;
    }
    T* data() const {
        return arr;
    }
    
    
    
    
    bool operator == (const FastVector<T>& other) const {
        if (_size != other._size) {
            return false;
        }
        
        for (size_t i = 0; i < _size; i++) {
            if (!(arr[i] == other.arr[i])) {
                return false;
            }
        }
        
        return true;
    }
    
    
    
    // @TODO: заменить это на fastCopy, а то ёбнет (в fastvector из анимаций есть пример работающей функции)
    void copy(FastVector<T> const &other) {
        grow(other._size);
        _size = other._size;
        std::memcpy(arr, other.arr, other._size * sizeof(T));
    }
    void copy(void const* data, size_t size) {
        grow(size);
        _size = size;
        std::memcpy(arr, data, size * sizeof(T));
    }
    
    
    void grab(FastVector<T> &other) {
        hardClear();
        arr = other.arr;
        _size = other._size;
        _capacity = other._capacity;
        
        // Настройки роста тоже забираем, чтобы не сбросились на дефолтные
        _initialCapacity = other._initialCapacity;
        _capacityFactor = other._capacityFactor;
        
        other.arr = nullptr;
        other._size = 0;
        other._capacity = 0;
    }
    
    
    T* begin() {
        return arr;
    }
    T* end() {
        return arr + _size;
    }
    const T* begin() const {
        return arr;
    }
    const T* end() const {
        return arr + _size;
    }
    
    
    
    void erase(size_t index) {
        if (index >= _size)
            return;
        
        arr[index].~T();
        
        // Сдвигаем все элементы после удаляемого на одну позицию влево
        std::memmove(arr + index, arr + index + 1, (_size - index - 1) * sizeof(T));
        _size--; // Уменьшаем размер
        
        // Создаём временный объект типа T
        new (arr + _size) T();  // "Затираем" данные последнего элемента, используя placement new
    }
    
    
    
    void resize(size_t size) {
        if (_capacity < size)
            setCapacity(size);
        _size = size;
    }
    void resize(size_t size, const T& value) {
        resize(size);
        fill(value);
    }
    
    
    void fill(const T& value) {
        std::fill(arr, arr + _size, value);
    }
    
    
    // Вставка по индексу
    void insertAt(size_t index, T const& data) {
        if (index > _size) {
            return;
        }
        
        // Увеличиваем размер, если нужно
        grow(_size + 1);
        
        // Сдвигаем элементы, начиная с позиции index, на одну позицию вправо
        std::memmove(arr + index + 1, arr + index, (_size - index) * sizeof(T));
        
        // Вставляем новый элемент
        arr[index] = data;
        
        // Увеличиваем размер вектора
        _size++;
    }
    
    
    
    
    // Урезает ёмкость вектора до текущего размера
    void shrinkToFit() {
        if (_capacity > _size) {
            // Если ёмкость больше размера, уменьшаем до размера
            T* newArr = nullptr;
            if (_size > 0) {
                newArr = new T[_size];
                for (size_t i = 0; i < _size; ++i) {
                    newArr[i] = std::move(arr[i]);
                }
            }
            delete[] arr;
            arr = newArr;
            _capacity = _size;
        }
    }
    
    
    
    // Удаляет дубликаты, оставляя только последнее вхождение каждого элемента
    void removeDuplicatesPreserveOrder() {
        // Временное хранилище для финального результата
        FastVector<T> result;
        
        for (int i = int(_size) - 1; i >= 0; --i) {
            bool found = false;
            for (size_t j = 0; j < result.size(); ++j) {
                if (arr[i] == result[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.push(arr[i]); // добавляем с конца
            }
        }
        
        // Разворачиваем, чтобы восстановить исходный порядок (с конца → в норму)
        for (size_t i = 0; i < result.size() / 2; ++i) {
            std::swap(result[i], result[result.size() - 1 - i]);
        }
        
        // Копируем результат обратно
        copy(result);
    }
    
    
    
    
    
private:
    
    
    // Расширение ёмкости под нужный размер с учётом множителя
    void grow(size_t requiredSize) {
        if (_capacity >= requiredSize)
            return;
        
        // На пустом векторе берём стартовый блок, дальше растём множителем
        size_t newCapacity = (_capacity == 0) ? _initialCapacity : size_t(_capacity * _capacityFactor);
        setCapacity(std::max(requiredSize, newCapacity));
    }
    
    
    T *arr = nullptr;
    size_t _size = 0;
    size_t _capacity = 0;
    
    uint32_t _initialCapacity = 64;
    float _capacityFactor = 1.5f;
};
