#pragma once

#include "Event.h"

class KeyEvent : public Event
{
  public:
    inline int GetKeyCode() const
    {
        return m_KeyCode;
    }

    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput);

  protected:
    int m_KeyCode;

  protected:
    KeyEvent(int keycode) : m_KeyCode(keycode)
    {
    }
};

class KeyPressedEvent : public KeyEvent
{
  public:
    KeyPressedEvent(int keycode) : KeyEvent(keycode)
    {
    }

    EVENT_CLASS_TYPE(KeyPressed);
};

class KeyReleasedEvent : public KeyEvent
{
  public:
    KeyReleasedEvent(int keycode) : KeyEvent(keycode)
    {
    }

    EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent
{
  public:
    KeyTypedEvent(int keycode) : KeyEvent(keycode)
    {
    }

    EVENT_CLASS_TYPE(KeyTyped)
};