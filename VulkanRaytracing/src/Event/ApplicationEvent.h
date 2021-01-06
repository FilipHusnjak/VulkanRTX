#pragma once

#include "Event.h"

class WindowCloseEvent : public Event
{
  public:
    WindowCloseEvent()
    {
    }

    EVENT_CLASS_TYPE(WindowClose);
    EVENT_CLASS_CATEGORY(EventCategory::EventCategoryApplication);
};

class WindowResizeEvent : public Event
{
  public:
    WindowResizeEvent(int width, int height) : m_Width(width), m_Height(height)
    {
    }

    inline int GetWidth()
    {
        return m_Width;
    }

     inline int GetHeight()
    {
        return m_Height;
    }

    EVENT_CLASS_TYPE(WindowResize);
    EVENT_CLASS_CATEGORY(EventCategory::EventCategoryApplication);

  private:
    int m_Width;
    int m_Height;
};
