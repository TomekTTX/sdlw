#include "sdlwin.hpp"
#include <iostream>

namespace sdlw {

    inline constexpr static int sgn(int x) { return (x < 0) - (x > 0); }

    Graphics::Graphics(int w, int h) : w(w), h(h), valid(initItems(w, h)) {}

    Graphics::~Graphics() {
        SDL_FreeSurface(screen);
        SDL_DestroyTexture(scrtex);
        SDL_DestroyWindow(window);
        SDL_DestroyRenderer(renderer);
    }

    void Graphics::drawPixel(int x, int y, Color color) {
        int64_t bpp = screen->format->BytesPerPixel;
        Uint8 *p = (Uint8 *)screen->pixels + (1LL * y * screen->pitch + x * bpp);
        *(Uint32 *)p = color;
    }

    bool error(const char *funcName, const char *msg) {
        std::cerr << funcName << " error: " << msg << '\n';
        return false;
    }

    bool Graphics::initItems(int w, int h) {
        constexpr int initFlags = SDL_INIT_EVERYTHING;

        if (SDL_WasInit(0) != initFlags && SDL_Init(initFlags) != 0)
            return error("SDL_Init", SDL_GetError());
        if (SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer) != 0)
            return error("SDL_CreateWindowAndRenderer", SDL_GetError());

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        SDL_RenderSetLogicalSize(renderer, w, h);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        screen = SDL_CreateRGBSurface(0, w, h, 32, 
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        scrtex = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);

        if (!TTF_WasInit() && TTF_Init() != 0)
            return error("TTF_Init", TTF_GetError());

        return true;
    }

    SDL_Color Graphics::sdlc(Color color) {
        return SDL_Color{
            (uint8_t)((color >> 16) & 0xFF),
            (uint8_t)((color >> 8) & 0xFF),
            (uint8_t)(color & 0xFF)
        };
    }

    void Graphics::drawRect(SDL_Rect rect, Color color) {
        SDL_FillRect(screen, &rect, color);
    }

    void Graphics::drawRect(SDL_Rect rect, int borderW, Color color, Color borderColor) {
        const SDL_Rect borders[] = {
            {rect.x, rect.y, borderW, rect.h},
            {rect.x + rect.w - borderW, rect.y, borderW, rect.h},
            {rect.x, rect.y, rect.w, borderW},
            {rect.x, rect.y + rect.h - borderW, rect.w, borderW},
        };

        SDL_FillRect(screen, &rect, color);
        SDL_FillRects(screen, borders, _countof(borders), borderColor);
    }

    TTF_Font *Graphics::getFont(Font fontName, int fontSize) {
        switch (fontName)
        {
        case Font::ARIAL: return TTF_OpenFont("./fonts/arial.ttf", fontSize);
        case Font::SANS: return TTF_OpenFont("./fonts/sans.ttf", fontSize);
        case Font::UBUNTU: return TTF_OpenFont("./fonts/ubuntu.ttf", fontSize);
        case Font::COMIC_SANS: return TTF_OpenFont("./fonts/comic_sans.ttf", fontSize);
        case Font::CONSOLAS: return TTF_OpenFont("./fonts/consolas.ttf", fontSize);
        case Font::WINGDINGS: return TTF_OpenFont("./fonts/wingdings.ttf", fontSize);
        case Font::WEBDINGS: return TTF_OpenFont("./fonts/webdings.ttf", fontSize);
        default: return nullptr;
        }
    }

    void Graphics::drawString(int x, int y, std::string_view text, int fontSize, Font fontName, Color color) {
        if (TTF_Font *font = getFont(fontName, fontSize)) {
            drawString(x, y, text, font, color);
            TTF_CloseFont(font);
        }
    }

    void Graphics::drawString(int x, int y, std::string_view text, TTF_Font *font, Color color) {
        if (!font) return;
        SDL_Surface *surface = TTF_RenderText_Solid(font, text.data(), sdlc(color));
        if (!surface) return;
        SDL_Rect textRect{ x,y };
        SDL_BlitSurface(surface, NULL, screen, &textRect);
        SDL_FreeSurface(surface);
    }

    void Graphics::drawString(SDL_Rect rect, std::string_view text, TTF_Font *font, Color color,
        bool hCenter, bool vCenter) {
        if (!font) return;
        SDL_Surface *surface = TTF_RenderText_Solid(font, text.data(), sdlc(color));
        if (!surface) return;
        SDL_Rect textRect{
            rect.x + hCenter * (rect.w - surface->w) / 2,
            rect.y + vCenter * (rect.h - surface->h) / 2
        };
        SDL_BlitSurface(surface, NULL, screen, &textRect);
        SDL_FreeSurface(surface);
    }

    Window::Window(int width, int height, std::string_view title, Font fontName, int fontSize) :
        w(width), h(height), title(title), state(State::RUN),
        g(w, h), winfont(g.getFont(fontName, fontSize)) 
    {
        if (!g.isValid()) {
            state = State::EXIT;
            return;
        }
        SDL_SetWindowTitle(g.window, title.data());
    }

    Component *Window::addComponent(std::unique_ptr<Component> &&comp, std::string_view id) {
        comp->mapColors(g);
        comp->setWindow(this);
        components[id] = std::move(comp);

        return components[id].get();
    }

    void Window::draw() {
        g.clear();
        for (const auto &[_, comp] : components)
            comp->draw(g);
    }
    
    void Window::update() {
        SDL_UpdateTexture(g.scrtex, NULL, g.screen->pixels, g.screen->pitch);
        SDL_RenderCopy(g.renderer, g.scrtex, NULL, NULL);
        SDL_RenderPresent(g.renderer);
        pendingUpdate = false;
    }

    bool Window::handleEvent(const SDL_Event &event) {
        if (event.type == SDL_QUIT) {
            state = State::EXIT;
            return true;
        }
        else if (event.type != SDL_KEYUP)
            return false;

        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            state = State::EXIT;
            return true;
        default:
            return false;
        }
    }

    void Window::events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (this->handleEvent(event))
                continue;
            for (const auto &[_, comp] : components) {
                if (comp->handleEvent(event)) {
                    pendingUpdate = true;
                    break;
                }
            }
        }
    }

    void Window::run() {
        while (state == State::RUN) {
            SDL_Delay(5);
            events();
            if (pendingUpdate) {
                draw();
                update();
            }
        }
    }

    void Component::mapColors(const Graphics &g) {
        int index = 0;
        auto cols = colors.ptrs();
        for (auto raw : rawColors.ptrs())
            *cols[index++] = g.color(*raw);
    }

    bool Component::handleHoverHL(const SDL_Event &event) {
        if (event.type == SDL_MOUSEMOTION 
            && posInside({ event.button.x, event.button.y }) != hovered) {
            hovered ^= true;
            return true;
        }
        return false;
    }

    int Component::thisWasClicked(const SDL_Event &event) const {
        if (event.type == SDL_MOUSEBUTTONDOWN 
            && posInside({ event.button.x, event.button.y }))
            return event.button.clicks;
        return 0;
    }

    int Component::clickOutside(const SDL_Event &event) const {
        if (event.type == SDL_MOUSEBUTTONDOWN
            && !posInside({ event.button.x, event.button.y }))
            return event.button.clicks;
        return 0;
    }

    bool Panel::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        for (const auto &comp : comps)
            if (comp->handleEvent(event))
                return true;
        return false;
    }

    void Panel::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, colors.bg, colors.line);
        for (const auto &comp : comps)
            comp->draw(g);
    }

    void Panel::setWindow(Window *window) {
        Component::setWindow(window);
        for (const auto &comp : comps) {
            comp->setWindow(win);
            comp->mapColors(win->graphics());
        }
    }

    void Panel::translate(int x, int y) {
        Component::translate(x, y);
        for (auto &comp : comps)
            comp->translate(x, y);
    }
    
    Component *Panel::addComponent(std::unique_ptr<Component> &&comp) {
        if (win) {
            comp->setWindow(win);
            comp->mapColors(win->graphics());
        }
        comps.push_back(std::move(comp));

        return comps.back().get();
    }

    void ScrollPanel::translate(int x, int y) {
        Panel::translate(x, y);
        scrollBegin.x += x;
        scrollBegin.y += y;
    }

    bool ScrollPanel::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (Panel::handleEvent(event)) return true;
        if (event.type != SDL_MOUSEWHEEL) return false;

        int offset = sgn(event.wheel.y);
        if (index + offset >= 0 && index + offset + numShown <= (int)comps.size()) {
            index += offset;
            scrollContent();
        }
        return true;
    }

    Component *ScrollPanel::addComponent(std::unique_ptr<Component> &&comp) {
        auto ret = Panel::addComponent(std::move(comp));
        scrollContent();
        return ret;
    }

    void ScrollPanel::scrollContent() {
        int yoff = 0;
        for (std::size_t i = 0; i < comps.size(); ++i) {
            auto &cur = *comps[i];
            if (i >= index && i < 0ULL + index + numShown) {
                cur.show();
                cur.setPos(scrollBegin.x, scrollBegin.y + yoff);
                yoff += cur.h();
            }
            else {
                cur.hide();
            }
        }
    }

    bool Button::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (handleHoverHL(event)) return true;
        if (thisWasClicked(event)) {
            callback(this);
            return true;
        }
        return false;
    }

    void Button::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, hovered ? colors.hl : colors.bg, colors.line);
        g.drawString(rect, text, win->font(), colors.text);
    }

    Expandable::Expandable(SDL_Rect rect, std::string_view text, const CompColors &colors,
        std::unique_ptr<Panel> &&panel, ExpandDir expDir) :
        Component(rect, colors), text(text), panel(std::move(panel)) {
        if (win) {
            this->panel->setWindow(win);
            this->panel->mapColors(win->graphics());
        }
        this->panel->hide();
        setExpandDir(expDir);
    }

    void Expandable::setExpandDir(ExpandDir dir) {
        switch (dir) {
        case ExpandDir::UP:
            expOffset = { 0, -panel->h() };
            break;
        case ExpandDir::DOWN:
            expOffset = { 0, h() };
            break;
        case ExpandDir::LEFT_UP:
            expOffset = { -panel->w(), -(panel->h() - h()) };
            break;
        case ExpandDir::RIGHT_UP:
            expOffset = { w(), -(panel->h() - h()) };
            break;
        case ExpandDir::LEFT_DOWN:
            expOffset = { -panel->w(), 0 };
            break;
        case ExpandDir::RIGHT_DOWN:
            expOffset = { w(), 0 };
            break;
        default:
            break;
        }
        adjustPanel();
    }

    void Expandable::setWindow(Window *window) {
        Component::setWindow(window);
        panel->setWindow(window);
        panel->mapColors(win->graphics());
    }

    bool Expandable::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (handleHoverHL(event)) return true;
        if (panel->handleEvent(event)) return true;
        if (thisWasClicked(event) == 1) {
            toggleExpanded();
            return true;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN
            && expanded
            && !panel->posInside({ event.button.x, event.button.y })) {
            setExpanded(false);
            return false;
        }
        return false;
    }

    void Expandable::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, hovered ? colors.hl : colors.bg, colors.line);
        g.drawString(rect, text, win->font(), colors.text);
        panel->draw(g);
    }

    void Expandable::translate(int x, int y) {
        Component::translate(x, y);
        panel->translate(x, y);
    }

    bool ComboBox::Elem::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (handleHoverHL(event)) return true;
        if (thisWasClicked(event)) {
            comboBox->setSelection(ind);
            comboBox->setExpanded(false);
            return true;
        }
        return false;
    }

    void ComboBox::Elem::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, hovered ? colors.hl : colors.bg, colors.line);
        g.drawString(rect, text, win->font(), colors.text);
    }

    std::unique_ptr<ScrollPanel> ComboBox::makePanel(int numShown) {
        const SDL_Rect panelRect{ 0, 0, w(), h() * numShown };
        return std::make_unique<ScrollPanel>(panelRect, rawColors.bg,
            rawColors.line, numShown);
    }

    void ComboBox::setWindow(Window *window) {
        Expandable::setWindow(window);
        finalizePanel();
    }

    void ComboBox::finalizePanel() {
        for (int i = 0; i < (int)options.size(); ++i)
            panel->addComponent(std::make_unique<Elem>(rect, rawColors, i, options[i], this));
        panel->setColors(rawColors);
        panel->setWindow(win);
    }

    bool Slider::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        const SDL_Point p = { event.button.x, event.button.y };
        const bool inRect = SDL_PointInRect(&p, &sliderRect);
        switch (event.type) {
        case SDL_MOUSEMOTION:
            if (hovered != inRect) {
                hovered = inRect;
                return true;
            }
            if (dragging) {
                dragDiff(p - mousePos);
                checkCallback();
                mousePos = p;
                return true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (inRect) {
                mousePos = p;
                dragging = true;
                return true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            dragging = false;
            break;
        default:
            break;
        }
        return false;
    }

    void Slider::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, colors.extra1, colors.line);
        g.drawRect(sliderRect, 1,
            (hovered || dragging) ? colors.hl : colors.bg, colors.line);
    }

    SDL_Rect Slider::makeSliderRect(int w) const {
        if (vertical) {
            return { rect.x - w / 2, rect.y, rect.w + w, w };
        }
        return { rect.x, rect.y - w / 2, w, rect.h + w };
    }

    void Slider::checkCallback() {
        if (!onValChange) return;
        const int nv = trueVal();
        if (nv != lastVal) {
            lastVal = nv;
            onValChange(nv);
        }
    }

    void Slider::dragDiff(SDL_Point dp) {
        if (vertical) {
            val += (dp.y * upp());
            capVal();
            sliderRect.y = rect.y + static_cast<int>(std::round(stepn() * pps()));
        }
        else {
            val += (dp.x * upp());
            capVal();
            sliderRect.x = rect.x + static_cast<int>(std::round(stepn() * pps()));
        }
    }

    void Slider::capVal() {
        if (val > max)
            val = float(max);
        else if (val < min)
            val = float(min);
    }

    Color ColorSelect::color() const {
        return
            colSlider[0]->trueVal() << 16 | 
            colSlider[1]->trueVal() << 8 | 
            colSlider[2]->trueVal();
    }

    std::string ColorSelect::str() const {
        return 
            hex(colSlider[0]->trueVal()) +
            hex(colSlider[1]->trueVal()) +
            hex(colSlider[2]->trueVal());
    }

    std::unique_ptr<Panel> ColorSelect::makePanel() const {
        return std::make_unique<Panel>(SDL_Rect{ 0,0,350,140 }, 0, 0);
    }

    std::unique_ptr<TextInput> ColorSelect::makeInput() {
        auto input = std::make_unique<TextInput>(
            SDL_Rect{rect.x + rect.w, rect.y, rect.w, 30}, 
            rawColors, "", true);
        input->setCallback([csel = this](const std::string &val) {
            csel->setColor(val);
        });
        return input;
    }

    void ColorSelect::finalize() {
        CompColors cols = rawColors;
        for (int i = 0; i < _countof(colSlider); ++i) {
            SDL_Rect r{panel->x() + 70, panel->y() + 30 + 35 * i, 255, 13};
            cols.bg = 0xFF0000 >> (8 * i);
            colSlider[i] = panel->addComponent(
                std::make_unique<Slider>(r, 0, 255, 1, cols, false, 13)
            )->as<Slider>();
        }
        panel->setColors(rawColors);
        panel->setWindow(win);
        panel->mapColors(win->graphics());
        input->setWindow(win);
        input->mapColors(win->graphics());
    }

    void ColorSelect::setColor(Color color) {
        for (int i = 0; i < _countof(colSlider); ++i)
            colSlider[i]->setVal((color >> (16 - 8 * i)) & 0xFF);
    }

    bool ColorSelect::setColor(const std::string &hexStr) {
        if (hexStr.length() < 6) return false;
        Color color{};
        try {
            //std::string s[] = { hexStr.substr(0, 2), hexStr.substr(2, 2), hexStr.substr(4, 2) };
            //int i[] = { std::stoi(s[0], nullptr, 16), std::stoi(s[1], nullptr, 16), std::stoi(s[2], nullptr, 16) };
            color =
                std::stoi(hexStr.substr(0, 2), nullptr, 16) << 16 |
                std::stoi(hexStr.substr(2, 2), nullptr, 16) << 8 |
                std::stoi(hexStr.substr(4, 2), nullptr, 16);
        }
        catch (std::invalid_argument) {
            return false;
        }
        setColor(color);
        return true;
    }

    void ColorSelect::setWindow(Window *window) {
        Expandable::setWindow(window);
        finalize();
    }

    std::string ColorSelect::hex(char c) {
        static constexpr char hx[] = "0123456789ABCDEF";
        return std::string{ hx[(c >> 4) & 0xF] } + hx[c & 0xF];
    }

    bool ColorSelect::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (thisWasClicked(event)) {
            if (input->isVisible()) {
                input->deactivate();
                toggleExpanded();
                return true;
            }
            if (expanded) {
                input->clear();
                input->activate();
                return true;
            }
        }
        if (input->handleEvent(event))
            return true;
        return Expandable::handleEvent(event);
    }

    void ColorSelect::draw(Graphics &g) {
        if (!shown) return;
        Color cur = g.color(color());
        text = std::string{ '#' } + str();
        g.drawRect(rect, 1, hovered ? colors.hl : colors.bg, colors.line);
        g.drawString(rect, text, win->font(), colors.text);
        g.drawRect({rect.x + 10, rect.y + 10, 20, 20}, 1, cur, colors.line);
        if (expanded) {
            panel->draw(g);
            g.drawRect({ panel->x() + 20, panel->y() + 20, 30, 100 }, 1, cur, colors.line);
        }
        input->draw(g);
    }
    
    void TextInput::activate() {
        active = true;
        SDL_StartTextInput();
        if (autoHide)
            show();
    }

    void TextInput::deactivate() {
        active = false;
        SDL_StopTextInput();
        if (autoHide)
            hide();
        if (onConfirm)
            onConfirm(text);
    }

    bool TextInput::handleEvent(const SDL_Event &event) {
        if (!shown) return false;
        if (!active && thisWasClicked(event)) {
            activate();
            return true;
        }
        if (active) {
            if (clickOutside(event)) {
                deactivate();
                return false;
            }
            if (event.type == SDL_TEXTINPUT) {
                insertChar(event.text.text[0], caretPos++);
                return true;
            }
            if (event.type == SDL_KEYDOWN)
                return handleKey(event.key.keysym.sym);
        }
        return false;
    }

    bool TextInput::handleKey(int kp) {
        switch (kp) {
        case SDLK_LEFT:
            --caretPos;
            break;
        case SDLK_RIGHT:
            ++caretPos;
            break;
        case SDLK_HOME:
            caretPos = 0;
            break;
        case SDLK_END:
            caretPos = (int)text.length();
            break;
        case SDLK_DELETE:
            deleteChar(caretPos);
            break;
        case SDLK_BACKSPACE:
            deleteChar(--caretPos);
            break;
        case SDLK_KP_ENTER:
        case SDLK_RETURN:
        case SDLK_RETURN2:
            deactivate();
            break;
        default:
            return false;
        }
        if (caretPos < 0)
            caretPos = 0;
        if (caretPos > (int)text.length())
            caretPos = (int)text.length();
        return true;
    }

    void TextInput::draw(Graphics &g) {
        if (!shown) return;
        g.drawRect(rect, 1, active ? colors.hl : colors.bg, colors.line);
        g.drawString({rect.x + 10,rect.y,rect.w,rect.h},
            text, win->font(), colors.text, false);
    }

    void TextInput::deleteChar(std::size_t index) {
        if (index < 0 || index >= text.length()) return;
        text = text.substr(0, index) + text.substr(index + 1);
    }

    void TextInput::insertChar(char chr, std::size_t index) {
        if (index < 0 || index > text.length()) return;
        text = text.substr(0, index) + char(chr) + text.substr(index);
    }
}
