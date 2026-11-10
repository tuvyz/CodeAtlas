#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include <iostream>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <typeindex>
#include <VisorGUI/Core/Core.h>


namespace vgui {


// Базовый класс Event
class Event {
public:
    void* sender;
    Event(void* sender) : sender(sender) {}
    virtual ~Event() {}
};



// Подписчик
struct Subscriber {
    void* receiver;
    std::function<void(const Event&)> handler;

    ~Subscriber() {}

    bool operator==(const Subscriber& other) const {
        return receiver == other.receiver && handler.target_type() == other.handler.target_type();
    }
};



// Менеджер событий
class EventManager {
    std::map<std::type_index, std::vector<Subscriber>> subscribers;

public:
    template<typename EventType>
    void subscribe(void* receiver, std::function<void(const EventType&)> handler) {
        std::type_index typeID = typeid(EventType);
        Subscriber newSubscriber {receiver, [handler](const Event& event) {
                if (const EventType* castedEvent = dynamic_cast<const EventType*>(&event)) {
                    handler(*castedEvent);
                }
            }};

        // Проверяем, не подписан ли уже такой обработчик
        auto& subList = subscribers[typeID];
        if (std::find(subList.begin(), subList.end(), newSubscriber) == subList.end()) {
            subList.push_back(newSubscriber);
        } else {
            platform().log("Subscriber already exists, skipping subscription.");
        }
    }

    template<typename EventType>
    void emitEvent(const EventType& event) {
        std::type_index typeID = typeid(EventType);
        if (subscribers.count(typeID)) {
            for (auto& subscriber : subscribers[typeID]) {
                subscriber.handler(event);
            }
        }
    }

    EventManager &eventManager() {return *this;}
};




// Макрос для упрощения подписки на методы класса
#define SUBSCRIBE_TO_EVENT(SENDER, EVENT_TYPE, RECEIVER, HANDLER_METHOD) \
(SENDER)->eventManager().subscribe<EVENT_TYPE>(RECEIVER, std::bind(HANDLER_METHOD, RECEIVER, std::placeholders::_1))

#define SUBSCRIBE_LAMBDA(SENDER, EVENT_TYPE, HANDLER_METHOD) \
    (SENDER)->eventManager().subscribe<EVENT_TYPE>(this, ([this](const EVENT_TYPE& event)HANDLER_METHOD))








} // namespace vgui

#endif // EVENTMANAGER_H
