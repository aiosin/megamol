/*
 * OverlayRenderer.cpp
 *
 * Copyright (C) 2019 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "OverlayRenderer.h"


using namespace megamol;
using namespace megamol::core;
using namespace megamol::gui;


OverlayRenderer::OverlayRenderer(void)
    : view::RendererModule<view::CallRender3D_2>()
    , paramMode("mode", "Overlay mode.")
    , paramAnchor("anchor", "Anchor of overlay.")
    , paramCustomPosition("position_offset", "Custom relative position offset in respect to selected anchor.")
    , paramFileName("texture::file_name", "The file name of the texture.")
    , paramRelativeWidth("texture::relative_width", "Relative screen space width of texture.")
    , paramButtonColor("media_buttons::color", "Color of media buttons.")
    , paramDuration("media_buttons::duration", "Duration media buttons are shown after value changes. Value of zero "
                                               "means showing media buttons permanently.")
    , paramScaling(
          "media_buttons::value_scaling", "Parameter value scaling adjusting threshold for media button appearance.")
    , paramPrefix("parameter::prefix", "The parameter value prefix.")
    , paramSufix("parameter::sufix", "The parameter value sufix.")
    , paramParameterName("parameter::name", "The full parameter name, e.g. '::Project_1::View3D_21::anim::time'. "
                                            "Supprted parameter types: float, int, Vector2f/3f/4f")
    , paramText("label::text", "The displayed text.")
    , paramFontName("font::name", "The font name.")
    , paramFontSize("font::size", "The font size.")
    , paramFontColor("font::color", "The font color.")
    , m_texture()
    , m_shader()
    , m_font(nullptr)
    , m_viewport()
    , m_current_rectangle({0.0f, 0.0f, 0.0f, 0.0f})
    , m_parameter_ptr(nullptr)
    , m_media_buttons()
    , m_last_state() {

    this->MakeSlotAvailable(&this->chainRenderSlot);
    this->MakeSlotAvailable(&this->renderSlot);

    param::EnumParam* mep = new param::EnumParam(Mode::TEXTURE);
    mep->SetTypePair(Mode::TEXTURE, "Texture");
    mep->SetTypePair(Mode::MEDIA_BUTTONS, "Media Buttons");
    mep->SetTypePair(Mode::PARAMETER, "Parameter");
    mep->SetTypePair(Mode::LABEL, "Label");
    this->paramMode << mep;
    this->paramMode.SetUpdateCallback(this, &OverlayRenderer::onToggleMode);
    this->MakeSlotAvailable(&this->paramMode);

    param::EnumParam* aep = new param::EnumParam(Anchor::ALIGN_LEFT_TOP);
    aep->SetTypePair(Anchor::ALIGN_LEFT_TOP, "Left Top");
    aep->SetTypePair(Anchor::ALIGN_LEFT_MIDDLE, "Left Middle");
    aep->SetTypePair(Anchor::ALIGN_LEFT_BOTTOM, "Left Bottom");
    aep->SetTypePair(Anchor::ALIGN_CENTER_TOP, "Center Top");
    aep->SetTypePair(Anchor::ALIGN_CENTER_MIDDLE, "Center Middle");
    aep->SetTypePair(Anchor::ALIGN_CENTER_BOTTOM, "Center Bottom");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_TOP, "Right Top");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_MIDDLE, "Right Middle");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_BOTTOM, "Right Bottom");
    this->paramAnchor << aep;
    this->paramAnchor.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramAnchor);

    this->paramCustomPosition << new param::Vector2fParam(vislib::math::Vector<float, 2>(0.0f, 0.0f),
        vislib::math::Vector<float, 2>(0.0f, 0.0f), vislib::math::Vector<float, 2>(100.0f, 100.0f));
    this->paramCustomPosition.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramCustomPosition);

    // Texture Mode
    this->paramFileName << new param::FilePathParam("");
    this->paramFileName.SetUpdateCallback(this, &OverlayRenderer::onTextureFileName);
    this->MakeSlotAvailable(&this->paramFileName);

    this->paramRelativeWidth << new param::FloatParam(25.0f, 0.0f, 100.0f);
    this->paramRelativeWidth.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramRelativeWidth);

    // Media Button Mode
    this->paramButtonColor << new param::ColorParam(0.5f, 0.5f, 0.5f, 1.0f);
    this->MakeSlotAvailable(&this->paramButtonColor);

    this->paramDuration << new param::FloatParam(3.0f, 0.0f);
    this->MakeSlotAvailable(&this->paramDuration);

    this->paramScaling << new param::FloatParam(1.0f, 0.0f);
    this->MakeSlotAvailable(&this->paramScaling);

    // Parameter Mode
    this->paramPrefix << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramPrefix);

    this->paramSufix << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramSufix);

    this->paramParameterName << new param::StringParam("");
    this->paramParameterName.SetUpdateCallback(this, &OverlayRenderer::onParameterName);
    this->MakeSlotAvailable(&this->paramParameterName);

    // Label Mode
    this->paramText << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramText);

    // Font Settings
    param::EnumParam* fep = new param::EnumParam(utility::SDFFont::FontName::ROBOTO_SANS);
    fep->SetTypePair(utility::SDFFont::FontName::ROBOTO_SANS, "Roboto Sans");
    fep->SetTypePair(utility::SDFFont::FontName::EVOLVENTA_SANS, "Evolventa");
    fep->SetTypePair(utility::SDFFont::FontName::UBUNTU_MONO, "Ubuntu Mono");
    fep->SetTypePair(utility::SDFFont::FontName::VOLLKORN_SERIF, "Vollkorn Serif");
    this->paramFontName << fep;
    this->paramFontName.SetUpdateCallback(this, &OverlayRenderer::onFontName);
    this->MakeSlotAvailable(&this->paramFontName);

    this->paramFontSize << new param::FloatParam(20.0f, 0.0f);
    this->MakeSlotAvailable(&this->paramFontSize);

    this->paramFontColor << new param::ColorParam(0.5f, 0.5f, 0.5f, 1.0f);
    this->MakeSlotAvailable(&this->paramFontColor);
}


OverlayRenderer::~OverlayRenderer(void) { this->Release(); }


void OverlayRenderer::release(void) {

    this->m_font.reset();
    this->m_parameter_ptr = nullptr;
    this->m_shader.Release();
    this->m_texture.tex.Release();
    for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
        this->m_media_buttons[i].tex.Release();
    }
}


bool OverlayRenderer::create(void) {

    return this->onToggleMode(this->paramMode);
    ;
}


bool OverlayRenderer::onToggleMode(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_font.reset();
    this->m_parameter_ptr = nullptr;
    this->m_shader.Release();
    this->m_texture.tex.Release();
    for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
        this->m_media_buttons[i].tex.Release();
    }

    this->setParameterGUIVisibility();

    auto mode = static_cast<Mode>(this->paramMode.Param<param::EnumParam>()->Value());
    switch (mode) {
    case (Mode::TEXTURE): {
        if (!this->loadShader(this->m_shader, "overlay::vertex", "overlay::fragment")) return false;
        this->onTextureFileName(slot);
    } break;
    case (Mode::MEDIA_BUTTONS): {
        if (!this->loadShader(this->m_shader, "overlay::vertex", "overlay::fragment")) return false;
        std::string filename;
        for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
            switch (static_cast<MediaButton>(i)) {
            case (MediaButton::PLAY):
                filename = "media_button_play.png";
                break;
            case (MediaButton::PAUSE):
                filename = "media_button_pause.png";
                break;
            case (MediaButton::STOP):
                filename = "media_button_stop.png";
                break;
            case (MediaButton::REWIND):
                filename = "media_button_rewind.png";
                break;
            case (MediaButton::FAST_FORWARD):
                filename = "media_button_fast-forward.png";
                break;
            }
            if (!this->loadTexture(filename, this->m_media_buttons[i])) return false;
        }
    } break;
    case (Mode::PARAMETER): {
        this->onParameterName(slot);
        this->onFontName(slot);
    } break;
    case (Mode::LABEL): {
        this->onFontName(slot);
    } break;
    }

    return true;
}


bool OverlayRenderer::onTextureFileName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_texture.tex.Release();
    std::string filename = std::string(this->paramFileName.Param<param::FilePathParam>()->Value().PeekBuffer());
    if (!this->loadTexture(filename, this->m_texture)) return false;
    this->onTriggerRecalcRectangle(slot);
    return true;
}


bool OverlayRenderer::onFontName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_font.reset();
    auto font_name = static_cast<utility::SDFFont::FontName>(this->paramFontName.Param<param::EnumParam>()->Value());
    this->m_font = std::make_unique<utility::SDFFont>(font_name);
    if (!this->m_font->Initialise(this->GetCoreInstance())) return false;
    return true;
}


bool OverlayRenderer::onParameterName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_parameter_ptr = nullptr;
    auto parameter_name = this->paramParameterName.Param<param::StringParam>()->Value();
    auto parameter_ptr = this->GetCoreInstance()->FindParameter(parameter_name, false, false);
    if (parameter_ptr.IsNull()) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to find parameter by name. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    bool found_valid_param_type = false;
    if (auto* float_param = parameter_ptr.DynamicCast<param::FloatParam>()) {
        found_valid_param_type = true;
    } else if (auto* int_param = parameter_ptr.DynamicCast<param::IntParam>()) {
        found_valid_param_type = true;
    } else if (auto* vec2_param = parameter_ptr.DynamicCast<param::Vector2fParam>()) {
        found_valid_param_type = true;
    } else if (auto* vec3_param = parameter_ptr.DynamicCast<param::Vector3fParam>()) {
        found_valid_param_type = true;
    } else if (auto* vec4_param = parameter_ptr.DynamicCast<param::Vector4fParam>()) {
        found_valid_param_type = true;
    }

    if (found_valid_param_type) {
        this->m_parameter_ptr = parameter_ptr;
    } else {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "No valid parameter type. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
    }

    return found_valid_param_type;
}

bool OverlayRenderer::onTriggerRecalcRectangle(core::param::ParamSlot& slot) {

    slot.ResetDirty();
    auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());
    auto param_position = this->paramCustomPosition.Param<param::Vector2fParam>()->Value();
    glm::vec2 rel_pos = glm::vec2(param_position.X() / 100.0f, param_position.Y() / 100.0f);
    auto rel_width = this->paramRelativeWidth.Param<param::FloatParam>()->Value() / 100.0f;
    this->m_current_rectangle = this->getScreenSpaceRect(rel_pos, rel_width, anchor, this->m_texture, this->m_viewport);
    return true;
}


void OverlayRenderer::setParameterGUIVisibility(void) {

    Mode mode = static_cast<Mode>(this->paramMode.Param<param::EnumParam>()->Value());
    bool texture_mode = (mode == Mode::TEXTURE);
    bool media_mode = (mode == Mode::MEDIA_BUTTONS);
    bool parameter_mode = (mode == Mode::PARAMETER);
    bool label_mode = (mode == Mode::LABEL);

    // Texture Mode
    this->paramFileName.Param<param::FilePathParam>()->SetGUIVisible(texture_mode);
    this->paramRelativeWidth.Param<param::FloatParam>()->SetGUIVisible(texture_mode || media_mode);

    // Media Buttons Mode
    this->paramButtonColor.Param<param::ColorParam>()->SetGUIVisible(media_mode);
    this->paramDuration.Param<param::FloatParam>()->SetGUIVisible(media_mode);
    this->paramScaling.Param<param::FloatParam>()->SetGUIVisible(media_mode);

    // Parameter Mode
    this->paramPrefix.Param<param::StringParam>()->SetGUIVisible(parameter_mode);
    this->paramSufix.Param<param::StringParam>()->SetGUIVisible(parameter_mode);
    this->paramParameterName.Param<param::StringParam>()->SetGUIVisible(parameter_mode || media_mode);

    // Label Mode
    this->paramText.Param<param::StringParam>()->SetGUIVisible(label_mode);

    // Font Settings
    this->paramFontName.Param<param::EnumParam>()->SetGUIVisible(label_mode || parameter_mode);
    this->paramFontSize.Param<param::FloatParam>()->SetGUIVisible(label_mode || parameter_mode);
    this->paramFontColor.Param<param::ColorParam>()->SetGUIVisible(label_mode || parameter_mode);
}


bool OverlayRenderer::GetExtents(view::CallRender3D_2& call) {

    auto* chainedCall = this->chainRenderSlot.CallAs<view::CallRender3D_2>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        bool retVal = (*chainedCall)(view::AbstractCallRender::FnGetExtents);
        call = *chainedCall;
        return retVal;
    }
    return true;
}


bool OverlayRenderer::Render(view::CallRender3D_2& call) {

    auto leftSlotParent = call.PeekCallerSlot()->Parent();
    std::shared_ptr<const view::AbstractView> viewptr =
        std::dynamic_pointer_cast<const view::AbstractView>(leftSlotParent);
    if (viewptr != nullptr) { // TODO move this behind the fbo magic?
        auto vp = call.GetViewport();
        glViewport(vp.Left(), vp.Bottom(), vp.Width(), vp.Height());
        auto backCol = call.BackgroundColor();
        glClearColor(backCol.x, backCol.y, backCol.z, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // First call chained renderer
    auto* chainedCall = this->chainRenderSlot.CallAs<view::CallRender3D_2>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        if (!(*chainedCall)(view::AbstractCallRender::FnRender)) {
            return false;
        }
    }

    // Get current viewport
    view::Camera_2 cam;
    call.GetCamera(cam);
    glm::ivec4 cam_viewport;
    if (!cam.image_tile().empty()) { /// or better: auto viewport = cr3d_in->GetViewport().GetSize()?
        cam_viewport = glm::ivec4(
            cam.image_tile().left(), cam.image_tile().bottom(), cam.image_tile().width(), cam.image_tile().height());
    } else {
        cam_viewport = glm::ivec4(0, 0, cam.resolution_gate().width(), cam.resolution_gate().height());
    }
    if ((this->m_viewport.x != cam_viewport.z) || (this->m_viewport.y != cam_viewport.w)) {
        this->m_viewport = {cam_viewport.z, cam_viewport.w};
        // Reload rectangle on viewport changes
        this->onTriggerRecalcRectangle(this->paramMode);
    }
    // Create 2D orthographic mvp matrix
    glm::mat4 ortho = glm::ortho(
        0.0f, static_cast<float>(this->m_viewport.x), 0.0f, static_cast<float>(this->m_viewport.y), -1.0f, 1.0f);

    // Draw mode dependent stuff
    auto mode = this->paramMode.Param<param::EnumParam>()->Value();
    switch (mode) {
    case (Mode::TEXTURE): {
        auto overwrite_color = glm::vec4(0.0f); /// Ignored when alpha = 0
        this->drawScreenSpaceBillboard(
            ortho, this->m_current_rectangle, this->m_texture, this->m_shader, overwrite_color);
    } break;
    case (Mode::MEDIA_BUTTONS): {
        auto param_color = this->paramButtonColor.Param<param::ColorParam>()->Value();
        glm::vec4 overwrite_color = glm::vec4(param_color[0], param_color[1], param_color[2], param_color[3]);
        float time_scaling = this->paramScaling.Param<param::FloatParam>()->Value();

        float current_value = 0.0f;
        if (auto* float_param = this->m_parameter_ptr.DynamicCast<param::FloatParam>()) {
            current_value = float_param->Value();
        } else if (auto* int_param = this->m_parameter_ptr.DynamicCast<param::IntParam>()) {
            current_value = static_cast<float>(int_param->Value());
        } else if (auto* vec2_param = this->m_parameter_ptr.DynamicCast<param::Vector2fParam>()) {
            current_value = vec2_param->Value().X() + vec2_param->Value().Y();
        } else if (auto* vec3_param = this->m_parameter_ptr.DynamicCast<param::Vector3fParam>()) {
            current_value = vec3_param->Value().X() + vec3_param->Value().Y() + vec3_param->Value().Z();
        } else if (auto* vec4_param = this->m_parameter_ptr.DynamicCast<param::Vector4fParam>()) {
            current_value =
                vec4_param->Value().X() + vec4_param->Value().Y() + vec4_param->Value().Z() + vec4_param->Value().W();
        }
        // if (media_button == MediaButton::NONE) {
        //    vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
        //        "Unable to use parmeter for media buttons [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        //    return false;
        //}

        current_value *= time_scaling;
        MediaButton current_button = MediaButton::NONE;
        float current_delta = current_value - this->m_last_state.value;

        if (current_delta > 1.0f) {
            current_button = MediaButton::FAST_FORWARD;
        } else if (current_delta < -0.01f) { // delta threshold sesitivity can be adjustd via scaling parameter
            current_button = MediaButton::REWIND;
        } else if (current_delta > 0.01f) { // delta threshold sesitivity can be adjustd via scaling parameter
            current_button = MediaButton::PLAY;
        } else if (current_delta == 0.0f) {
            current_button = MediaButton::PAUSE;
        }
        // else if ((current_value == 0.0f) && (this->m_last_delta_time > 0.0f)) {
        //    media_button = MediaButton::STOP;
        //}

        if (current_button != this->m_last_state.button) {
            this->m_last_state.start_time = std::chrono::system_clock::now();
        }
        this->m_last_state.button = current_button;
        this->m_last_state.value = current_value;

        float duration = this->paramDuration.Param<param::FloatParam>()->Value();
        std::chrono::duration<double> diff = (std::chrono::system_clock::now() - this->m_last_state.start_time);
        if (diff.count() > static_cast<double>(duration)) {
            current_button = MediaButton::NONE;
        }

        if (current_button != MediaButton::NONE) {
            this->drawScreenSpaceBillboard(ortho, this->m_current_rectangle, this->m_media_buttons[current_button],
                this->m_shader, overwrite_color);
        }

    } break;
    case (Mode::PARAMETER): {
        auto param_color = this->paramFontColor.Param<param::ColorParam>()->Value();
        glm::vec4 color = glm::vec4(param_color[0], param_color[1], param_color[2], param_color[3]);
        auto font_size = this->paramFontSize.Param<param::FloatParam>()->Value();
        auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());

        auto param_prefix = this->paramPrefix.Param<param::StringParam>()->Value();
        std::string prefix = std::string(param_prefix.PeekBuffer());

        auto param_sufix = this->paramSufix.Param<param::StringParam>()->Value();
        std::string sufix = std::string(param_sufix.PeekBuffer());

        std::string text;
        if (auto* float_param = this->m_parameter_ptr.DynamicCast<param::FloatParam>()) {
            auto value = float_param->Value();
            std::stringstream stream;
            stream << std::fixed << std::setprecision(8) << " " << value << " ";
            text = prefix + stream.str() + sufix;
        } else if (auto* int_param = this->m_parameter_ptr.DynamicCast<param::IntParam>()) {
            auto value = int_param->Value();
            std::stringstream stream;
            stream << " " << value << " ";
            text = prefix + stream.str() + sufix;
        } else if (auto* vec2_param = this->m_parameter_ptr.DynamicCast<param::Vector2fParam>()) {
            auto value = vec2_param->Value();
            std::stringstream stream;
            stream << std::fixed << std::setprecision(8) << " (" << value.X() << ", " << value.Y() << ") ";
            text = prefix + stream.str() + sufix;
        } else if (auto* vec3_param = this->m_parameter_ptr.DynamicCast<param::Vector3fParam>()) {
            auto value = vec3_param->Value();
            std::stringstream stream;
            stream << std::fixed << std::setprecision(8) << " (" << value.X() << ", " << value.Y() << ", " << value.Z()
                   << ") ";
            text = prefix + stream.str() + sufix;
        } else if (auto* vec4_param = this->m_parameter_ptr.DynamicCast<param::Vector4fParam>()) {
            auto value = vec4_param->Value();
            std::stringstream stream;
            stream << std::fixed << std::setprecision(8) << " (" << value.X() << ", " << value.Y() << ", " << value.Z()
                   << ", " << value.W() << ") ";
            text = prefix + stream.str() + sufix;
        }
        if (text.empty()) {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to read parmeter value [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

        this->drawScreenSpaceText(ortho, (*this->m_font), text, color, font_size, anchor, this->m_current_rectangle);
    } break;
    case (Mode::LABEL): {
        auto param_color = this->paramFontColor.Param<param::ColorParam>()->Value();
        glm::vec4 color = glm::vec4(param_color[0], param_color[1], param_color[2], param_color[3]);
        auto font_size = this->paramFontSize.Param<param::FloatParam>()->Value();
        auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());

        auto param_text = this->paramText.Param<param::StringParam>()->Value();
        std::string text = std::string(param_text.PeekBuffer());

        if (this->m_font == nullptr) {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to read texture: %s [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
        this->drawScreenSpaceText(ortho, (*this->m_font), text, color, font_size, anchor, this->m_current_rectangle);
    } break;
    }

    return true;
}


void OverlayRenderer::drawScreenSpaceBillboard(glm::mat4 ortho, Rectangle rectangle, TextureData& texture,
    vislib::graphics::gl::GLSLShader& shader, glm::vec4 overwrite_color) const {

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    texture.tex.Bind();

    shader.Enable();

    glUniformMatrix4fv(shader.ParameterLocation("ortho"), 1, GL_FALSE, glm::value_ptr(ortho));
    glUniform1f(shader.ParameterLocation("left"), rectangle.left);
    glUniform1f(shader.ParameterLocation("right"), rectangle.right);
    glUniform1f(shader.ParameterLocation("top"), rectangle.top);
    glUniform1f(shader.ParameterLocation("bottom"), rectangle.bottom);
    glUniform4fv(shader.ParameterLocation("overwrite_color"), 1, glm::value_ptr(overwrite_color));
    glUniform1i(shader.ParameterLocation("tex"), 0);

    // Vertex position is only given via uniforms.
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    shader.Disable();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);
}

void OverlayRenderer::drawScreenSpaceText(glm::mat4 ortho, megamol::core::utility::SDFFont& font,
    const std::string& text, glm::vec4 color, float size, Anchor anchor, Rectangle rectangle) const {

    float x = rectangle.left;
    float y = rectangle.top;
    float z = -1.0f;

    switch (anchor) {
    // case(Anchor::ALIGN_LEFT_TOP): {} break;
    case (Anchor::ALIGN_LEFT_MIDDLE): {
        y = rectangle.top + (rectangle.bottom - rectangle.top) / 2.0f;
    } break;
    case (Anchor::ALIGN_LEFT_BOTTOM): {
        y = rectangle.bottom;
    } break;
    case (Anchor::ALIGN_CENTER_TOP): {
        x = rectangle.left + (rectangle.right - rectangle.left) / 2.0f;
    } break;
    case (Anchor::ALIGN_CENTER_MIDDLE): {
        x = rectangle.left + (rectangle.right - rectangle.left) / 2.0f;
        y = rectangle.top + (rectangle.bottom - rectangle.top) / 2.0f;
    } break;
    case (Anchor::ALIGN_CENTER_BOTTOM): {
        x = rectangle.left + (rectangle.right - rectangle.left) / 2.0f;
        y = rectangle.bottom;
    } break;
    case (Anchor::ALIGN_RIGHT_TOP): {
        x = rectangle.right;
    } break;
    case (Anchor::ALIGN_RIGHT_MIDDLE): {
        x = rectangle.right;
        y = rectangle.top + (rectangle.bottom - rectangle.top) / 2.0f;
    } break;
    case (Anchor::ALIGN_RIGHT_BOTTOM): {
        x = rectangle.right;
        y = rectangle.bottom;
    } break;
    }

    // Font rendering takes matrices from OpenGL stack
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glLoadMatrixf(glm::value_ptr(ortho));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    font.DrawString(glm::value_ptr(color), x, y, z, size, false, text.c_str(), anchor);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}


OverlayRenderer::Rectangle OverlayRenderer::getScreenSpaceRect(
    glm::vec2 rel_pos, float rel_width, Anchor anchor, const TextureData& texture, glm::ivec2 viewport) const {

    Rectangle rectangle = {0.0f, 0.0f, 0.0f, 0.0f};

    float tex_aspect = static_cast<float>(texture.width) / static_cast<float>(texture.height);
    float viewport_aspect = static_cast<float>(viewport.x) / static_cast<float>(viewport.y);
    float rel_height = rel_width / tex_aspect * viewport_aspect;

    switch (anchor) {
    case (Anchor::ALIGN_LEFT_TOP): {
        rectangle.left = rel_pos.x;
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = 1.0f - rel_pos.y;
        rectangle.bottom = rectangle.top - rel_height;
    } break;

    case (Anchor::ALIGN_LEFT_MIDDLE): {
        rectangle.left = rel_pos.x;
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = 0.5f - (rel_pos.y / 2.0f) + (rel_height / 2.0f);
        rectangle.bottom = rectangle.top - rel_height;
    } break;
    case (Anchor::ALIGN_LEFT_BOTTOM): {
        rectangle.left = rel_pos.x;
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = rel_pos.y + rel_height;
        rectangle.bottom = rel_pos.y;
    } break;
    case (Anchor::ALIGN_CENTER_TOP): {
        rectangle.left = 0.5f + (rel_pos.x / 2.0f) - (rel_width / 2.0f);
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = 1.0 - rel_pos.y;
        rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_CENTER_MIDDLE): {
        rectangle.left = 0.5f + (rel_pos.x / 2.0f) - (rel_width / 2.0f);
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = 0.5f - (rel_pos.y / 2.0f) + (rel_height / 2.0f);
        rectangle.bottom = rectangle.top - rel_height;
    } break;
    case (Anchor::ALIGN_CENTER_BOTTOM): {
        rectangle.left = 0.5f + (rel_pos.x / 2.0f) - (rel_width / 2.0f);
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = rel_pos.y + rel_height;
        rectangle.bottom = rel_pos.y;
    } break;
    case (Anchor::ALIGN_RIGHT_TOP): {
        rectangle.left = 1.0f - rel_pos.x - rel_width;
        rectangle.right = 1.0f - rel_pos.x;
        rectangle.top = 1.0 - rel_pos.y;
        rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_RIGHT_MIDDLE): {
        rectangle.left = 1.0f - rel_pos.x - rel_width;
        rectangle.right = 1.0f - rel_pos.x;
        rectangle.top = 0.5f - (rel_pos.y / 2.0f) + (rel_height / 2.0f);
        rectangle.bottom = rectangle.top - rel_height;
    } break;
    case (Anchor::ALIGN_RIGHT_BOTTOM): {
        rectangle.left = 1.0f - rel_pos.x - rel_width;
        rectangle.right = 1.0f - rel_pos.x;
        rectangle.top = rel_pos.y + rel_height;
        rectangle.bottom = rel_pos.y;
    } break;
    }

    rectangle.left *= viewport.x;
    rectangle.right *= viewport.x;
    rectangle.top *= viewport.y;
    rectangle.bottom *= viewport.y;

    return rectangle;
}


bool OverlayRenderer::loadTexture(const std::string& filename, TextureData& texture) const {

    if (filename.empty()) return false;
    if (texture.tex.IsValid()) texture.tex.Release();

    static vislib::graphics::BitmapImage img;
    static sg::graphics::PngBitmapCodec pbc;
    pbc.Image() = &img;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    void* buf = nullptr;
    size_t size = 0;

    size = utility::ResourceWrapper::LoadResource(
        this->GetCoreInstance()->Configuration(), vislib::StringA(filename.c_str()), (void**)(&buf));
    if (size == 0) {
        size = this->loadRawFile(filename, &buf);
    }

    if (size > 0) {
        if (pbc.Load(buf, size)) {
            img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
            if (texture.tex.Create(img.Width(), img.Height(), false, img.PeekDataAs<BYTE>(), GL_RGBA) != GL_NO_ERROR) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                    "Unable to create texture: %s [%s, %s, line %d]\n", filename.c_str(), __FILE__, __FUNCTION__,
                    __LINE__);
                ARY_SAFE_DELETE(buf);
                return false;
            }
            texture.width = img.Width();
            texture.height = img.Height();
            texture.tex.Bind();
            /// glGenerateMipmap(GL_TEXTURE_2D);
            /// texture.tex.SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
            texture.tex.SetFilter(GL_LINEAR, GL_LINEAR); // Uncomment when MipMaps are used.
            texture.tex.SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            ARY_SAFE_DELETE(buf);
        } else {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to read texture: %s [%s, %s, line %d]\n", filename.c_str(), __FILE__, __FUNCTION__, __LINE__);
            ARY_SAFE_DELETE(buf);
            return false;
        }
    } else {
        ARY_SAFE_DELETE(buf);
        return false;
    }

    return true;
}


bool OverlayRenderer::loadShader(
    vislib::graphics::gl::GLSLShader& io_shader, const std::string& vert_name, const std::string& frag_name) const {

    io_shader.Release();
    vislib::graphics::gl::ShaderSource vert, frag;
    try {
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource(
                vislib::StringA(vert_name.c_str()), vert)) {
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource(
                vislib::StringA(frag_name.c_str()), frag)) {
            return false;
        }
        if (!io_shader.Create(vert.Code(), vert.Count(), frag.Code(), frag.Count())) {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to compile: Unknown error [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
    } catch (vislib::graphics::gl::AbstractOpenGLShader::CompileException ce) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader (@%s): %s [%s, %s, line %d]\n",
            vislib::graphics::gl::AbstractOpenGLShader::CompileException::CompileActionName(ce.FailedAction()),
            ce.GetMsgA(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (vislib::Exception e) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader: %s [%s, %s, line %d]\n", e.GetMsgA(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader: Unknown exception [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


size_t OverlayRenderer::loadRawFile(std::string name, void** outData) const {

    *outData = nullptr;

    vislib::StringW filename = static_cast<vislib::StringW>(name.c_str());
    if (filename.IsEmpty()) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unable to load file: No file name given. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    if (!vislib::sys::File::Exists(filename)) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": Not existing. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    size_t size = static_cast<size_t>(vislib::sys::File::GetSize(filename));
    if (size < 1) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": File is empty. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    vislib::sys::FastFile f;
    if (!f.Open(filename, vislib::sys::File::READ_ONLY, vislib::sys::File::SHARE_READ, vislib::sys::File::OPEN_ONLY)) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": Unable to open file. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    *outData = new BYTE[size];
    size_t num = static_cast<size_t>(f.Read(*outData, size));
    if (num != size) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unable to load file \"%s\": Unable to read whole file. [%s, %s, line %d]\n", name.c_str(), __FILE__,
            __FUNCTION__, __LINE__);
        ARY_SAFE_DELETE(*outData);
        return 0;
    }

    return num;
}
