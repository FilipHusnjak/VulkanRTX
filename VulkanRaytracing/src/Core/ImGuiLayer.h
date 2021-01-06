#pragma once

#include "Core/Layer.h"

class ImGuiLayer : public Layer
{
  public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    void OnAttach() override;
    void OnDetach() override;

    void Begin();
    void End();
};