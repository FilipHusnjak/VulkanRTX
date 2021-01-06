#pragma once

#include <vector>

class Input
{
  private:
    Input() = default;

  public:
    Input(const Input &) = delete;
    Input &operator=(const Input &) = delete;

    static bool IsKeyPressed(int key);

    static bool IsMouseButtonPressed(int button);

    static std::pair<float, float> GetMousePosition();

    static float GetMouseX();

    static float GetMouseY();

    static void EnableCursor();

    static void DisableCursor();
};
